// CabinSlotWidget.cpp — CRT-экран слотов кабины с глитч-переходом

#include "CabinSlotWidget.h"
#include "ArtifactBase.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UCabinSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Стартуем с пустым слотом
	FCabinSlot EmptySlot;
	ApplySlotData(EmptySlot, 0, 3);
}

void UCabinSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bGlitching) return;

	GlitchTimer += InDeltaTime;
	const float Alpha = FMath::Clamp(GlitchTimer / GlitchDuration, 0.0f, 1.0f);

	if (Alpha < 0.5f)
	{
		// Фаза 1: затухание + горизонтальный сдвиг (CRT-помехи)
		const float FadeOut = 1.0f - (Alpha * 2.0f);
		SetRenderOpacity(FadeOut);

		const float Shift = FMath::RandRange(-8.0f, 8.0f) * (1.0f - FadeOut);
		SetRenderTranslation(FVector2D(Shift, 0.0f));
	}
	else
	{
		// Фаза 2: применяем новые данные + появление
		if (bHasPendingData)
		{
			ApplySlotData(PendingSlot, PendingIndex, PendingTotal);
			bHasPendingData = false;
		}

		const float FadeIn = (Alpha - 0.5f) * 2.0f;
		SetRenderOpacity(FadeIn);
		SetRenderTranslation(FVector2D(
			FMath::RandRange(-3.0f, 3.0f) * (1.0f - FadeIn), 0.0f));
	}

	if (Alpha >= 1.0f)
	{
		bGlitching = false;
		SetRenderOpacity(1.0f);
		SetRenderTranslation(FVector2D::ZeroVector);
	}
}

void UCabinSlotWidget::RefreshSlotDisplay(FCabinSlot InSlot, int32 Index, int32 Total)
{
	// При переключении — глитч + отложенное обновление
	if (SlotNumberText && SlotNumberText->GetText().ToString().Len() > 0)
	{
		PendingSlot = InSlot;
		PendingIndex = Index;
		PendingTotal = Total;
		bHasPendingData = true;
		PlayGlitchTransition();
	}
	else
	{
		ApplySlotData(InSlot, Index, Total);
	}
}

void UCabinSlotWidget::PlayGlitchTransition()
{
	bGlitching = true;
	GlitchTimer = 0.0f;
}

void UCabinSlotWidget::ApplySlotData(FCabinSlot InSlot, int32 Index, int32 Total)
{
	// Номер слота: "[1/3]"
	if (SlotNumberText)
	{
		FString Str = FString::Printf(TEXT("[%d/%d]"), Index + 1, Total);
		SlotNumberText->SetText(FText::FromString(Str));
	}

	// Название предмета
	if (ItemNameText)
	{
		if (InSlot.bOccupied && InSlot.Artifact)
		{
			ItemNameText->SetText(FText::FromName(InSlot.Artifact->GetItemName()));
		}
		else
		{
			ItemNameText->SetText(FText::FromString(TEXT("\u041F\u0423\u0421\u0422\u041E")));
		}
	}

	// Иконка
	if (ItemIconImage)
	{
		if (InSlot.bOccupied && InSlot.Artifact && InSlot.Artifact->ItemIcon)
		{
			ItemIconImage->SetBrushFromTexture(InSlot.Artifact->ItemIcon);
			ItemIconImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			ItemIconImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}
