// CabinOrderWidget.cpp — Реализация карточки заказа с CRT-глитчем

#include "CabinOrderWidget.h"
#include "GI_DimensionRunner.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

// ============================================================
// CONSTRUCT / DESTRUCT
// ============================================================

void UCabinOrderWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Подписка на делегат
	UGI_DimensionRunner* GI = GetGI();
	if (GI)
	{
		GI->OnOrderChanged.AddDynamic(this, &UCabinOrderWidget::OnOrderUpdated);
	}

	// Первичное заполнение
	RefreshFromGI();
}

void UCabinOrderWidget::NativeDestruct()
{
	UGI_DimensionRunner* GI = GetGI();
	if (GI)
	{
		GI->OnOrderChanged.RemoveDynamic(this, &UCabinOrderWidget::OnOrderUpdated);
	}

	Super::NativeDestruct();
}

// ============================================================
// TICK (CRT-глитч)
// ============================================================

void UCabinOrderWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bGlitching) return;

	GlitchTimer += InDeltaTime;
	const float Alpha = FMath::Clamp(GlitchTimer / GlitchDuration, 0.0f, 1.0f);

	if (Alpha < 0.5f)
	{
		// Фаза 1: затухание + горизонтальный сдвиг
		const float FadeOut = 1.0f - (Alpha * 2.0f);
		SetRenderOpacity(FadeOut);
		SetRenderTranslation(FVector2D(FMath::RandRange(-10.0f, 10.0f) * (1.0f - FadeOut), 0.0f));
	}
	else
	{
		// Фаза 2: применяем данные + появление
		if (bHasPendingData)
		{
			if (bPendingIsEmpty)
			{
				if (OrderTitleText)
				{
					OrderTitleText->SetText(FText::FromString(TEXT("НЕТ ЗАКАЗА")));
					OrderTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
				}
				if (ItemNameText) ItemNameText->SetText(FText::FromString(TEXT("---")));
				if (MoodEmojiText) MoodEmojiText->SetText(FText::FromString(TEXT("")));
				if (PaymentText) PaymentText->SetText(FText::FromString(TEXT("")));
				if (TimerText) TimerText->SetText(FText::FromString(TEXT("")));
				if (DimensionText) DimensionText->SetText(FText::FromString(TEXT("")));
			}
			else
			{
				ApplyOrderData(PendingOrder);
			}
			bHasPendingData = false;
		}

		const float FadeIn = (Alpha - 0.5f) * 2.0f;
		SetRenderOpacity(FadeIn);
		SetRenderTranslation(FVector2D(FMath::RandRange(-4.0f, 4.0f) * (1.0f - FadeIn), 0.0f));
	}

	if (Alpha >= 1.0f)
	{
		bGlitching = false;
		SetRenderOpacity(1.0f);
		SetRenderTranslation(FVector2D::ZeroVector);
	}
}

// ============================================================
// ПОКАЗАТЬ ЗАКАЗ
// ============================================================

void UCabinOrderWidget::ShowOrder(const FOrderData& Order)
{
	PendingOrder = Order;
	bHasPendingData = true;
	bPendingIsEmpty = false;
	PlayGlitchTransition();
}

void UCabinOrderWidget::ShowNoOrder()
{
	bHasPendingData = true;
	bPendingIsEmpty = true;
	PlayGlitchTransition();
}

void UCabinOrderWidget::PlayGlitchTransition()
{
	bGlitching = true;
	GlitchTimer = 0.0f;
}

void UCabinOrderWidget::RefreshFromGI()
{
	UGI_DimensionRunner* GI = GetGI();
	if (!GI)
	{
		ShowNoOrder();
		return;
	}

	if (GI->IsOrderActive())
	{
		ApplyOrderData(GI->GetCurrentOrder());
	}
	else
	{
		if (OrderTitleText)
		{
			OrderTitleText->SetText(FText::FromString(TEXT("НЕТ ЗАКАЗА")));
			OrderTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
		}
		if (ItemNameText) ItemNameText->SetText(FText::FromString(TEXT("---")));
		if (MoodEmojiText) MoodEmojiText->SetText(FText::FromString(TEXT("")));
		if (PaymentText) PaymentText->SetText(FText::FromString(TEXT("")));
		if (TimerText) TimerText->SetText(FText::FromString(TEXT("")));
		if (DimensionText) DimensionText->SetText(FText::FromString(TEXT("")));
	}
}

// ============================================================
// ПРИМЕНИТЬ ДАННЫЕ
// ============================================================

/** Текстовая метка настроения (вместо emoji) */
static FString GetMoodLabel(EClientMood Mood)
{
	switch (Mood)
	{
	case EClientMood::Bad:     return TEXT("[ЗЛОЙ]");
	case EClientMood::Neutral: return TEXT("[НЕЙТРАЛЬНЫЙ]");
	case EClientMood::Good:    return TEXT("[ДОБРЫЙ]");
	default: return TEXT("?");
	}
}

/** Цвет настроения */
static FLinearColor GetMoodColor(EClientMood Mood)
{
	switch (Mood)
	{
	case EClientMood::Bad:     return FLinearColor(0.9f, 0.15f, 0.1f);  // Красный
	case EClientMood::Neutral: return FLinearColor(0.8f, 0.75f, 0.3f);  // Жёлтый
	case EClientMood::Good:    return FLinearColor(0.15f, 0.85f, 0.3f);  // Зелёный
	default: return FLinearColor::White;
	}
}

/** Текстовое имя измерения (ASCII, без Unicode-дельт) */
static FString GetDimensionLabel(EDimensionID Dim)
{
	switch (Dim)
	{
	case EDimensionID::Delta01_Mirror:    return TEXT("D-01 ЗЕРКАЛО");
	case EDimensionID::Delta02_ZeroG:     return TEXT("D-02 НЕВЕСОМОСТЬ");
	case EDimensionID::Delta03_Darkness:  return TEXT("D-03 ТЕМНОТА");
	case EDimensionID::Delta04_Collapse:  return TEXT("D-04 КОЛЛАПС");
	case EDimensionID::Delta05_Labyrinth: return TEXT("D-05 ЛАБИРИНТ");
	default: return TEXT("D-?? НЕИЗВЕСТНО");
	}
}

void UCabinOrderWidget::ApplyOrderData(const FOrderData& Order)
{
	if (OrderTitleText)
	{
		OrderTitleText->SetText(FText::FromString(TEXT("ЗАКАЗ")));
		OrderTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.85f, 0.2f))); // Золотой
	}

	if (ItemNameText)
	{
		ItemNameText->SetText(Order.DisplayName);
		ItemNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}

	if (MoodEmojiText)
	{
		MoodEmojiText->SetText(FText::FromString(GetMoodLabel(Order.Mood)));
		MoodEmojiText->SetColorAndOpacity(FSlateColor(GetMoodColor(Order.Mood)));
	}

	if (PaymentText)
	{
		PaymentText->SetText(FText::FromString(
			FString::Printf(TEXT("%d CR"), Order.Payment)));
		// Тёмно-зелёный для суммы оплаты
		PaymentText->SetColorAndOpacity(FSlateColor(FLinearColor(0.15f, 0.75f, 0.2f)));
	}

	if (TimerText)
	{
		int32 TotalSec = FMath::RoundToInt32(Order.TimerSeconds);
		int32 Min = TotalSec / 60;
		int32 Sec = TotalSec % 60;
		TimerText->SetText(FText::FromString(
			FString::Printf(TEXT("%d:%02d"), Min, Sec)));
		// Голубой для таймера
		TimerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.85f, 0.95f)));
	}

	if (DimensionText)
	{
		DimensionText->SetText(FText::FromString(GetDimensionLabel(Order.DimensionID)));
		// Фиолетовый для измерения
		DimensionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.3f, 0.9f)));
	}
}

// ============================================================
// ДЕЛЕГАТ
// ============================================================

void UCabinOrderWidget::OnOrderUpdated(const FOrderData& NewOrder)
{
	if (NewOrder.IsValid())
	{
		ShowOrder(NewOrder);
	}
	else
	{
		ShowNoOrder();
	}
}

// ============================================================
// ХЕЛПЕР
// ============================================================

UGI_DimensionRunner* UCabinOrderWidget::GetGI() const
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return Cast<UGI_DimensionRunner>(GI);
}
