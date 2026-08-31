// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LobbyPC.generated.h"

/**
 *
 */
UCLASS()
class UNREALPROJECT_API ALobbyPC : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	virtual void OnPossess(APawn* aPawn) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TObjectPtr<class ULobbyWidgetBase> LobbyWidgetObject;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UUserWidget> LoadingScreenWidgetTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TObjectPtr<class UUserWidget> LoadingScreenWidgetObject;


	UFUNCTION(Server, Reliable, WithValidation)
	void C2S_SendMessage(const FText& Message); //UHT ���� ��, ���� ��ġ Ȯ�� �� ����
	bool C2S_SendMessage_Validate(const FText& Message);
	void C2S_SendMessage_Implementation(const FText& Message); //���� ���� ����, �츮 ����

	UFUNCTION(Client, Unreliable)
	void S2C_SendMessage(const FText& Message);
	void S2C_SendMessage_Implementation(const FText& Message);

	UFUNCTION(Client, Reliable)
	void S2C_ShowLoadingScreen();
	void S2C_ShowLoadingScreen_Implementation();


};
