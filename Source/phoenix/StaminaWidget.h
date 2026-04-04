// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StaminaWidget.generated.h"

class USprintStaminaComponent;
class UProgressBar;
class UTextBlock;

UCLASS(Abstract)
class PHOENIX_API UStaminaWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ================= Настройки =================

	UPROPERTY(EditDefaultsOnly, Category = "Stamina|Opacity")
	float IdleOpacity = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Stamina|Opacity")
	float ActiveOpacity = 0.70f;

	UPROPERTY(EditDefaultsOnly, Category = "Stamina|Opacity")
	float IdleDelaySeconds = 1.5f;

	// ================= Виджеты (привязка в Blueprint) =================

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> staminatext;

private:
	TWeakObjectPtr<USprintStaminaComponent> StaminaComp;

	float CurrentOpacity = 0.0f;
	float DisplayPercent = 0.0f;

	// Bounce: пружинный эффект при восстановлении
	float BounceVelocity = 0.0f;
	float PrevStaminaPercent = 1.0f;
};
