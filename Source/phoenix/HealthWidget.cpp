// Fill out your copyright notice in the Description page of Project Settings.

#include "HealthWidget.h"
#include "HealthComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"

void UHealthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			HealthComp = Pawn->FindComponentByClass<UHealthComponent>();
		}
	}

	CurrentOpacity = IdleOpacity;
	DisplayPercent = 1.0f;
	PrevHealthPercent = 1.0f;
	BounceVelocity = 0.0f;
	TimeSinceFullHealth = 0.0f;

	// Если установлен ProgressBar, берем его динамический материал (поможет для изогнутого материала)
	if (ProgressBar)
	{
		// Получаем стиль и кисть
		FProgressBarStyle Style = ProgressBar->GetWidgetStyle();
		FSlateBrush FillBrush = Style.FillImage;

		UMaterialInterface* Mat = FillBrush.GetResourceObject() ? Cast<UMaterialInterface>(FillBrush.GetResourceObject()) : nullptr;
		if (Mat)
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(Mat, this);
			// Перезаписываем кисть
			FillBrush.SetResourceObject(DynamicMaterial);
			Style.FillImage = FillBrush;
			ProgressBar->SetWidgetStyle(Style);
		}
	}
}

void UHealthWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!HealthComp.IsValid())
	{
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			HealthComp = Pawn->FindComponentByClass<UHealthComponent>();
		}
		return;
	}

	const float HealthPct = HealthComp->HealthPercent;

	// 1. АВТО-СКРЫТИЕ ПРИ ПОЛНОМ ЗДОРОВЬЕ
	if (HealthPct >= 1.0f)
	{
		TimeSinceFullHealth += InDeltaTime;
	}
	else
	{
		TimeSinceFullHealth = 0.0f; // Сбрасываем, если получено ранение
	}

	float TargetOpacity = IdleOpacity;
	if (TimeSinceFullHealth < IdleDelaySeconds)
	{
		TargetOpacity = ActiveOpacity;
	}

	CurrentOpacity = FMath::FInterpTo(CurrentOpacity, TargetOpacity, InDeltaTime, 5.0f);

	// 2. ПРУЖИННЫЙ ЭФФЕКТ ДЛЯ АНИМАЦИИ ПОЛОСКИ
	const float HealthDelta = HealthPct - PrevHealthPercent;
	const bool bIsRecovering = HealthDelta > 0.001f;

	if (bIsRecovering)
	{
		BounceVelocity += HealthDelta * 2.0f;
	}

	const float SpringStiffness = 60.0f;
	const float Damping = 6.0f;

	float SpringForce = (HealthPct - DisplayPercent) * SpringStiffness;
	BounceVelocity += SpringForce * InDeltaTime;
	BounceVelocity *= FMath::Exp(-Damping * InDeltaTime);
	DisplayPercent += BounceVelocity * InDeltaTime;
	DisplayPercent = FMath::Clamp(DisplayPercent, 0.0f, 1.05f);

	PrevHealthPercent = HealthPct;

	// 3. ЭФФЕКТЫ ПРИ КРИТИЧЕСКОМ ЗДОРОВЬЕ (< 20%)
	FLinearColor CurrentColor = BaseColor;
	FVector2D ShakeOffset = FVector2D::ZeroVector;
	float ScalePulse = 1.0f;

	if (HealthPct <= 0.20f && HealthPct > 0.0f)
	{
		// Мигание цвета (стробоскоп красный -> ярко-красный)
		float PulseWave = (FMath::Sin(GetWorld()->GetTimeSeconds() * 10.0f) + 1.0f) * 0.5f;
		FLinearColor FlashColor(1.0f, 0.0f, 0.0f); 
		CurrentColor = FLinearColor::LerpUsingHSV(CurrentColor, FlashColor, PulseWave * 0.8f);

		// Тряска
		float ShakeIntensity = (1.0f - (HealthPct / 0.20f)) * 5.0f; // Чем ближе к 0, тем сильнее
		ShakeOffset.X = FMath::RandRange(-ShakeIntensity, ShakeIntensity);
		ShakeOffset.Y = FMath::RandRange(-ShakeIntensity * 0.5f, ShakeIntensity * 0.5f);

		ScalePulse = 1.0f + (FMath::Sin(GetWorld()->GetTimeSeconds() * 12.0f) * 0.08f);
	}

	// 4. ПРИМЕНЯЕМ К ВИДЖЕТАМ
	if (ProgressBar)
	{
		ProgressBar->SetPercent(FMath::Clamp(DisplayPercent, 0.0f, 1.0f));
		ProgressBar->SetFillColorAndOpacity(CurrentColor);
		ProgressBar->SetRenderOpacity(CurrentOpacity);
		
		ProgressBar->SetRenderTranslation(ShakeOffset);
		ProgressBar->SetRenderScale(FVector2D(1.0f, ScalePulse));

		// Передача в материал (если используется сложный изогнутый шейдер)
		if (DynamicMaterial)
		{
			DynamicMaterial->SetScalarParameterValue("Percent", DisplayPercent);
			DynamicMaterial->SetVectorParameterValue("Color", CurrentColor);
		}
	}

	if (HealthText)
	{
		HealthText->SetRenderOpacity(CurrentOpacity);
		HealthText->SetRenderTranslation(ShakeOffset);
		
		// Обновляем текст (значение HP)
		HealthText->SetText(FText::AsNumber(FMath::RoundToInt32(HealthComp->CurrentHealth)));
	}
}
