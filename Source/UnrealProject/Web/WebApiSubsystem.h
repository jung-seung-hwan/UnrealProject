// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "TimerManager.h"
#include "WebApiSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWebApiResultSignature, const bool, bInSuccess, const FString&, InMessage);

/**
 * 웹서버와의 HTTP 통신을 전담한다. 결과는 델리게이트로만 알린다.
 */
UCLASS()
class UNREALPROJECT_API UWebApiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "WebApi")
	FWebApiResultSignature OnLoginResult;

	UPROPERTY(BlueprintAssignable, Category = "WebApi")
	FWebApiResultSignature OnSignUpResult;

	void RequestLogin(const FString& InServerIP, const FString& InUserID, const FString& InPassword);

	void RequestSignUp(const FString& InServerIP, const FString& InUserID, const FString& InPassword);

private:
	void HandleWorldInitialized(UWorld* InWorld, const UWorld::InitializationValues InInitializationValues);

	void SendGameServerRegistration();

	FString GetWebServerHost() const;

	int32 GetWebServerPort() const;

	FString BuildWebUrl(const FString& InServerHost, const FString& InPath) const;

	void SendAuthRequest(const FString& InServerIP, const FString& InPath,
		const FString& InUserID, const FString& InPassword,
		FWebApiResultSignature& InDelegate, const bool bInIsLogin);

	void HandleAuthResponse(FHttpResponsePtr InResponse, const bool bInConnectedSuccessfully,
		FWebApiResultSignature& InDelegate, const bool bInIsLogin);

	FDelegateHandle WorldInitializedHandle;

	FTimerHandle ServerHeartbeatTimerHandle;

	TWeakObjectPtr<UWorld> RegisteredServerWorld;

	FString ServerRegistrationId;
};
