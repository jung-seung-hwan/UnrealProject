// Fill out your copyright notice in the Description page of Project Settings.


#include "WebApiSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Guid.h"
#include "../DataGameInstanceSubsystem.h"

namespace
{
	constexpr int32 DefaultWebServerPort = 8080;
	constexpr int32 DefaultGameServerPort = 7777;
	constexpr float DefaultHeartbeatSeconds = 30.0f;

	bool HasExplicitPort(const FString& InHost)
	{
		FString Host = InHost;
		int32 SchemeIndex = INDEX_NONE;
		if (Host.FindChar(TEXT('/'), SchemeIndex))
		{
			const int32 ProtocolEnd = Host.Find(TEXT("://"));
			if (ProtocolEnd != INDEX_NONE)
			{
				Host.RightChopInline(ProtocolEnd + 3);
			}
		}

		if (Host.StartsWith(TEXT("[")))
		{
			return Host.Contains(TEXT("]:"));
		}

		return Host.Contains(TEXT(":"));
	}
}

void UWebApiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ServerRegistrationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	WorldInitializedHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
		this, &UWebApiSubsystem::HandleWorldInitialized);
}

void UWebApiSubsystem::Deinitialize()
{
	if (RegisteredServerWorld.IsValid())
	{
		RegisteredServerWorld->GetTimerManager().ClearTimer(ServerHeartbeatTimerHandle);
	}

	if (WorldInitializedHandle.IsValid())
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitializedHandle);
		WorldInitializedHandle.Reset();
	}

	RegisteredServerWorld.Reset();
	Super::Deinitialize();
}

void UWebApiSubsystem::RequestLogin(const FString& InServerIP, const FString& InUserID, const FString& InPassword)
{
	SendAuthRequest(InServerIP, TEXT("/login"), InUserID, InPassword, OnLoginResult, true);
}

void UWebApiSubsystem::RequestSignUp(const FString& InServerIP, const FString& InUserID, const FString& InPassword)
{
	SendAuthRequest(InServerIP, TEXT("/signup"), InUserID, InPassword, OnSignUpResult, false);
}

void UWebApiSubsystem::HandleWorldInitialized(UWorld* InWorld,
	const UWorld::InitializationValues InInitializationValues)
{
	if (!IsValid(InWorld))
	{
		return;
	}

	// OnPostWorldInitialization can run before the ?Listen net driver is ready.
	// Check once now and once more when the world actually begins play.
	TWeakObjectPtr<UWorld> WeakWorld(InWorld);
	InWorld->OnWorldBeginPlay.AddWeakLambda(this, [this, WeakWorld]()
	{
		TryRegisterGameServer(WeakWorld.Get());
	});

	TryRegisterGameServer(InWorld);
}

void UWebApiSubsystem::TryRegisterGameServer(UWorld* InWorld)
{
	if (!IsValid(InWorld))
	{
		return;
	}

	const ENetMode NetMode = InWorld->GetNetMode();
	if (NetMode != NM_DedicatedServer && NetMode != NM_ListenServer)
	{
		return;
	}

	if (RegisteredServerWorld.Get() == InWorld)
	{
		return;
	}

	if (RegisteredServerWorld.IsValid())
	{
		RegisteredServerWorld->GetTimerManager().ClearTimer(ServerHeartbeatTimerHandle);
	}

	RegisteredServerWorld = InWorld;
	SendGameServerRegistration();

	float HeartbeatSeconds = DefaultHeartbeatSeconds;
	GConfig->GetFloat(TEXT("WebApi"), TEXT("RegistrationHeartbeatSeconds"), HeartbeatSeconds, GGameIni);
	HeartbeatSeconds = FMath::Max(HeartbeatSeconds, 5.0f);
	InWorld->GetTimerManager().SetTimer(
		ServerHeartbeatTimerHandle,
		this,
		&UWebApiSubsystem::SendGameServerRegistration,
		HeartbeatSeconds,
		true);
}

void UWebApiSubsystem::SendGameServerRegistration()
{
	UWorld* World = RegisteredServerWorld.Get();
	if (!IsValid(World))
	{
		return;
	}

	const FString WebServerHost = GetWebServerHost();
	if (WebServerHost.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Game server registration skipped: web server host is empty"));
		return;
	}

	int32 GameServerPort = World->URL.Port;
	if (GameServerPort <= 0)
	{
		GameServerPort = DefaultGameServerPort;
		GConfig->GetInt(TEXT("WebApi"), TEXT("GameServerPort"), GameServerPort, GGameIni);
	}

	FString AdvertisedHost;
	if (!FParse::Value(FCommandLine::Get(), TEXT("AdvertisedHost="), AdvertisedHost))
	{
		GConfig->GetString(TEXT("WebApi"), TEXT("AdvertisedHost"), AdvertisedHost, GGameIni);
	}

	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("server_id"), ServerRegistrationId);
	JsonObject->SetNumberField(TEXT("port"), GameServerPort);
	JsonObject->SetStringField(TEXT("map_name"), World->GetMapName());
	JsonObject->SetStringField(TEXT("advertised_host"), AdvertisedHost.TrimStartAndEnd());

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(JsonObject, Writer);

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BuildWebUrl(WebServerHost, TEXT("/servers/register")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(Body);

	TWeakObjectPtr<UWebApiSubsystem> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr, FHttpResponsePtr Response, const bool bConnectedSuccessfully)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			if (!bConnectedSuccessfully || !Response.IsValid() || Response->GetResponseCode() != 200)
			{
				const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
				UE_LOG(LogTemp, Warning, TEXT("Game server registration failed (HTTP %d)"), ResponseCode);
				return;
			}

			UE_LOG(LogTemp, Log, TEXT("Game server registration refreshed"));
		});

	Request->ProcessRequest();
}

FString UWebApiSubsystem::GetWebServerHost() const
{
	FString Host;
	if (FParse::Value(FCommandLine::Get(), TEXT("WebServer="), Host))
	{
		return Host.TrimStartAndEnd();
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UDataGameInstanceSubsystem* Data = GameInstance->GetSubsystem<UDataGameInstanceSubsystem>())
		{
			Host = !Data->WebServerHost.IsEmpty() ? Data->WebServerHost : Data->ServerIP;
			if (!Host.IsEmpty())
			{
				return Host.TrimStartAndEnd();
			}
		}
	}

	GConfig->GetString(TEXT("WebApi"), TEXT("DefaultWebServerHost"), Host, GGameIni);
	return Host.TrimStartAndEnd();
}

int32 UWebApiSubsystem::GetWebServerPort() const
{
	int32 Port = DefaultWebServerPort;
	GConfig->GetInt(TEXT("WebApi"), TEXT("WebServerPort"), Port, GGameIni);
	return Port;
}

FString UWebApiSubsystem::BuildWebUrl(const FString& InServerHost, const FString& InPath) const
{
	FString BaseUrl = InServerHost.TrimStartAndEnd();
	BaseUrl.RemoveFromEnd(TEXT("/"));
	if (!BaseUrl.StartsWith(TEXT("http://")) && !BaseUrl.StartsWith(TEXT("https://")))
	{
		BaseUrl = TEXT("http://") + BaseUrl;
	}

	if (!HasExplicitPort(BaseUrl))
	{
		BaseUrl += FString::Printf(TEXT(":%d"), GetWebServerPort());
	}

	return BaseUrl + InPath;
}

void UWebApiSubsystem::SendAuthRequest(const FString& InServerIP, const FString& InPath,
	const FString& InUserID, const FString& InPassword,
	FWebApiResultSignature& InDelegate, const bool bInIsLogin)
{
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("user_id"), InUserID);
	JsonObject->SetStringField(TEXT("passwd"), InPassword);

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(JsonObject, Writer);

	const FString Url = BuildWebUrl(InServerIP, InPath);
	UE_LOG(LogTemp, Warning, TEXT("%s"), *Url);

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(Body);

	// 응답이 도착하기 전에 GameInstance가 정리될 수 있으므로 약참조로 잡는다.
	TWeakObjectPtr<UWebApiSubsystem> WeakThis(this);
	FWebApiResultSignature* DelegatePtr = &InDelegate;

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, DelegatePtr, bInIsLogin](FHttpRequestPtr, FHttpResponsePtr InResponse, bool bInConnectedSuccessfully)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			WeakThis->HandleAuthResponse(InResponse, bInConnectedSuccessfully, *DelegatePtr, bInIsLogin);
		});

	Request->ProcessRequest();
}

void UWebApiSubsystem::HandleAuthResponse(FHttpResponsePtr InResponse, const bool bInConnectedSuccessfully,
	FWebApiResultSignature& InDelegate, const bool bInIsLogin)
{
	if (!bInConnectedSuccessfully || !InResponse.IsValid())
	{
		InDelegate.Broadcast(false, TEXT("서버에 연결할 수 없습니다"));
		return;
	}

	const int32 ResponseCode = InResponse->GetResponseCode();
	if (ResponseCode != 200)
	{
		InDelegate.Broadcast(false, FString::Printf(TEXT("요청을 처리할 수 없습니다 (코드 %d)"), ResponseCode));
		return;
	}

	const FString ResponseBody = InResponse->GetContentAsString();

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		InDelegate.Broadcast(false, TEXT("응답을 해석할 수 없습니다"));
		return;
	}

	if (!JsonObject->GetBoolField(TEXT("result")))
	{
		InDelegate.Broadcast(false, JsonObject->GetStringField(TEXT("message")));
		return;
	}

	if (bInIsLogin)
	{
		UDataGameInstanceSubsystem* Data = GetGameInstance()->GetSubsystem<UDataGameInstanceSubsystem>();
		if (Data)
		{
			Data->Idx = JsonObject->GetIntegerField(TEXT("idx"));
			Data->Nickname = JsonObject->GetStringField(TEXT("nickname"));
			Data->Level = JsonObject->GetIntegerField(TEXT("level"));
			JsonObject->TryGetStringField(TEXT("game_server_address"), Data->GameServerAddress);
			Data->bLoggedIn = true;
		}
	}

	FString Message;
	JsonObject->TryGetStringField(TEXT("message"), Message);
	InDelegate.Broadcast(true, Message);
}
