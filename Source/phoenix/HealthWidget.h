// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthWidget.generated.h"

class UHealthComponent;
class UProgressBar;
class UTextBlock;

UCLASS(Abstract)
class PHOENIX_API UHealthWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ================= Настройки =================

	UPROPERTY(EditDefaultsOnly, Category = "Health|Opacity")
	float IdleOpacity = 0.3f; // Слегка ярче стамины в простое

	UPROPERTY(EditDefaultsOnly, Category = "Health|Opacity")
	float ActiveOpacity = 0.85f; // Чуть прозрачнее, чтобы не мешало

	UPROPERTY(EditDefaultsOnly, Category = "Health|Opacity")
	float IdleDelaySeconds = 2.0f; // Через сколько секунд скрываться 

	UPROPERTY(EditDefaultsOnly, Category = "Health|Color")
	FLinearColor BaseColor = FColor::FromHex("8F0B13");

	UPROPERTY(EditDefaultsOnly, Category = "Health|Color")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.04f, 0.55f);

	UPROPERTY(EditDefaultsOnly, Category = "Health|Shape")
	float CornerRadius = 10.0f;

	// ================= Виджеты (привязка в Blueprint) =================

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;

	// Для кастомного материала "гнутого" прогресс-бара
	// Если ты назначишь материал на ProgressBar, мы будем менять его параметр "Percent"
	UPROPERTY(Transient)
	class UMaterialInstanceDynamic* DynamicMaterial;

private:
	TWeakObjectPtr<UHealthComponent> HealthComp;

	float CurrentOpacity = 0.0f;
	float DisplayPercent = 0.0f;

	// Bounce
	float BounceVelocity = 0.0f;
	float PrevHealthPercent = 1.0f;
	
	// Таймер ухода в сон
	float TimeSinceFullHealth = 0.0f;
};
