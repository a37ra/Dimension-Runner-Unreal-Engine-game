// Fill out your copyright notice in the Description page of Project Settings.

#include "StaminaWidget.h"
#include "SprintStaminaComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

void UStaminaWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			StaminaComp = Pawn->FindComponentByClass<USprintStaminaComponent>();
		}
	}

	CurrentOpacity = IdleOpacity;
	DisplayPercent = 1.0f;
	PrevStaminaPercent = 1.0f;
	BounceVelocity = 0.0f;

	// Настраиваем стиль: скруглённые углы + полупрозрачный фон
	if (ProgressBar)
	{
		FProgressBarStyle Style = ProgressBar->GetWidgetStyle();
		const FVector4 Radii(CornerRadius, CornerRadius, CornerRadius, CornerRadius);

		// Фон
		FSlateBrush BgBrush;
		BgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		BgBrush.TintColor = FSlateColor(BackgroundColor);
		BgBrush.OutlineSettings.CornerRadii = Radii;
		BgBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Style.BackgroundImage = BgBrush;

		// Заполнение
		FSlateBrush FillBrush;
		FillBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		FillBrush.TintColor = FSlateColor(FLinearColor::White);
		FillBrush.OutlineSettings.CornerRadii = Radii;
		FillBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Style.FillImage = FillBrush;

		ProgressBar->SetWidgetStyle(Style);
	}
}

void UStaminaWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!StaminaComp.IsValid())
	{
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			StaminaComp = Pawn->FindComponentByClass<USprintStaminaComponent>();
		}
		return;
	}

	const float StaminaPct = StaminaComp->StaminaPercent;

	// 1. СОСТОЯНИЕ: Активен или Простаивает
	const bool bIsActive = StaminaComp->bIsSprinting || StaminaComp->CurrentStamina < StaminaComp->MaxStamina;
	
	float TargetOpacity = IdleOpacity;
	if (bIsActive)
	{
		TargetOpacity = ActiveOpacity;
		if (StaminaComp->CurrentStamina >= StaminaComp->MaxStamina)
		{
			if (StaminaComp->GetTimeSinceStoppedSprinting() < IdleDelaySeconds)
			{
				TargetOpacity = ActiveOpacity; 
			}
		}
	}

	CurrentOpacity = FMath::FInterpTo(CurrentOpacity, TargetOpacity, InDeltaTime, 8.0f);

	// 2. ПЛАВНЫЙ ПРОЦЕНТ С BOUNCE (пружинный эффект)
	const float StaminaDelta = StaminaPct - PrevStaminaPercent;
	const bool bIsRecovering = StaminaDelta > 0.001f;

	if (bIsRecovering)
	{
		// Стамина растёт — добавляем "пружинный" импульс
		BounceVelocity += StaminaDelta * 2.0f;
	}

	// Пружинная физика: затухание + возвращение к цели
	const float SpringStiffness = 80.0f;  // Жёсткость пружины
	const float Damping = 8.0f;           // Затухание

	float SpringForce = (StaminaPct - DisplayPercent) * SpringStiffness;
	BounceVelocity += SpringForce * InDeltaTime;
	BounceVelocity *= FMath::Exp(-Damping * InDeltaTime); // Затухание
	DisplayPercent += BounceVelocity * InDeltaTime;
	DisplayPercent = FMath::Clamp(DisplayPercent, 0.0f, 1.05f); // Чуть выше 1.0 для overshoot

	PrevStaminaPercent = StaminaPct;

	// 3. СОЧНЫЙ ЦВЕТ: плавный HSV переход (Полусерый -> Красный)
	float ColorLerpAlpha = FMath::Clamp((0.35f - StaminaPct) / 0.25f, 0.0f, 1.0f);
	FLinearColor SafeColor(0.6f, 0.6f, 0.6f); // Полусерый
	FLinearColor PanicColor(1.0f, 0.0f, 0.0f); // Красный
	FLinearColor TargetColor = FLinearColor::LerpUsingHSV(SafeColor, PanicColor, ColorLerpAlpha);

	// 🌊 Дыхание в покое — лёгкая пульсация яркости (±5%)
	{
		float BreathWave = FMath::Sin(GetWorld()->GetTimeSeconds() * 3.0f) * 0.05f;
		TargetColor = TargetColor * (1.0f + BreathWave);
	}

	// Стробоскоп при критической стамине (<15%)
	if (StaminaPct <= 0.15f)
	{
		float PulseWave = (FMath::Sin(GetWorld()->GetTimeSeconds() * 14.0f) + 1.0f) * 0.5f;
		FLinearColor FlashColor(0.0f, 1.0f, 1.0f);
		TargetColor = FLinearColor::LerpUsingHSV(TargetColor, FlashColor, PulseWave * 0.4f);
	}

	// 4. ПРИМЕНЯЕМ К ВИДЖЕТАМ
	if (ProgressBar)
	{
		ProgressBar->SetPercent(FMath::Clamp(DisplayPercent, 0.0f, 1.0f));
		ProgressBar->SetFillColorAndOpacity(TargetColor);
		ProgressBar->SetRenderOpacity(CurrentOpacity);

		// 💀 Тряска + дыхание при критической стамине
		if (StaminaPct <= 0.15f)
		{
			// Тряска — мелкое рандомное смещение
			float ShakeIntensity = (1.0f - (StaminaPct / 0.15f)) * 3.0f; // Чем ниже — тем сильнее
			float ShakeX = FMath::RandRange(-ShakeIntensity, ShakeIntensity);
			float ShakeY = FMath::RandRange(-ShakeIntensity * 0.5f, ShakeIntensity * 0.5f);
			ProgressBar->SetRenderTranslation(FVector2D(ShakeX, ShakeY));

			// Физическое дыхание (scale)
			float ScalePulse = 1.0f + (FMath::Sin(GetWorld()->GetTimeSeconds() * 14.0f) * 0.05f);
			ProgressBar->SetRenderScale(FVector2D(1.0f, ScalePulse));
		}
		else
		{
			ProgressBar->SetRenderTranslation(FVector2D::ZeroVector);
			ProgressBar->SetRenderScale(FVector2D(1.0f, 1.0f));
		}
	}

	if (staminatext)
	{
		staminatext->SetRenderOpacity(CurrentOpacity);

		// Текст тоже трясётся при критической стамине
		if (StaminaPct <= 0.15f)
		{
			float ShakeIntensity = (1.0f - (StaminaPct / 0.15f)) * 2.0f;
			float ShakeX = FMath::RandRange(-ShakeIntensity, ShakeIntensity);
			float ShakeY = FMath::RandRange(-ShakeIntensity * 0.5f, ShakeIntensity * 0.5f);
			staminatext->SetRenderTranslation(FVector2D(ShakeX, ShakeY));
		}
		else
		{
			staminatext->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}
}
