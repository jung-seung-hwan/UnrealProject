// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DataGameInstanceSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class UNREALPROJECT_API UDataGameInstanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	FString UserID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	FString Password;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	FString ServerIP;

	/** 로그인/서버 등록 요청을 보내는 웹 API 서버 주소 */
	UPROPERTY(BlueprintReadOnly, Category = "Data")
	FString WebServerHost;

	/** 로그인 응답에서 받은 Unreal 게임 서버 접속 주소 (host:port) */
	UPROPERTY(BlueprintReadOnly, Category = "Data")
	FString GameServerAddress;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	bool bLoggedIn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	int32 Idx = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	FString Nickname;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	int32 Level = 0;

};
