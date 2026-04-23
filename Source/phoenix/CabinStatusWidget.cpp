// CabinStatusWidget.cpp — Реализация экрана рейтинга

#include "CabinStatusWidget.h"
#include "GI_DimensionRunner.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

// ============================================================
// CONSTRUCT / DESTRUCT
// ============================================================

void UCabinStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// ASCII-метки секций (вместо emoji, которые шрифт UE не поддерживает)
	if (EmojiText_1) EmojiText_1->SetText(FText::FromString(TEXT(">:(")));
	if (EmojiText_2) EmojiText_2->SetText(FText::FromString(TEXT(":(")));
	if (EmojiText_3) EmojiText_3->SetText(FText::FromString(TEXT(":/")));
	if (EmojiText_4) EmojiText_4->SetText(FText::FromString(TEXT(":)")));
	if (EmojiText_5) EmojiText_5->SetText(FText::FromString(TEXT(":D")));
	if (EmojiText_6) EmojiText_6->SetText(FText::FromString(TEXT("XD")));

	// Цвета для меток секций
	if (EmojiText_1) EmojiText_1->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.15f, 0.1f)));  // Красный
	if (EmojiText_2) EmojiText_2->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.4f, 0.1f)));   // Оранжевый
	if (EmojiText_3) EmojiText_3->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.8f, 0.2f)));   // Жёлтый
	if (EmojiText_4) EmojiText_4->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.85f, 0.3f)));  // Салатовый
	if (EmojiText_5) EmojiText_5->SetColorAndOpacity(FSlateColor(FLinearColor(0.1f, 0.9f, 0.35f)));  // Зелёный
	if (EmojiText_6) EmojiText_6->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.7f, 1.0f)));   // Голубой

	// Подписка на делегаты GI
	UGI_DimensionRunner* GI = GetGI();
	if (GI)
	{
		GI->OnRatingChanged.AddDynamic(this, &UCabinStatusWidget::OnRatingUpdated);
		GI->OnCreditsChanged.AddDynamic(this, &UCabinStatusWidget::OnCreditsUpdated);
		GI->OnDayChanged.AddDynamic(this, &UCabinStatusWidget::OnDayUpdated);
	}

	// Первичное заполнение
	RefreshFromGI();
}

void UCabinStatusWidget::NativeDestruct()
{
	// Снимаем подписки
	UGI_DimensionRunner* GI = GetGI();
	if (GI)
	{
		GI->OnRatingChanged.RemoveDynamic(this, &UCabinStatusWidget::OnRatingUpdated);
		GI->OnCreditsChanged.RemoveDynamic(this, &UCabinStatusWidget::OnCreditsUpdated);
		GI->OnDayChanged.RemoveDynamic(this, &UCabinStatusWidget::OnDayUpdated);
	}

	Super::NativeDestruct();
}

// ============================================================
// ОБНОВЛЕНИЕ
// ============================================================

void UCabinStatusWidget::RefreshFromGI()
{
	UGI_DimensionRunner* GI = GetGI();
	if (!GI) return;

	UpdateRatingBar(GI->GetRating());
	UpdateCredits(GI->GetCredits());
	UpdateDay(GI->GetCurrentDay());
}

void UCabinStatusWidget::OnRatingUpdated(float NewRating)
{
	UpdateRatingBar(NewRating);

	// Обновить текст секции тоже
	UGI_DimensionRunner* GI = GetGI();
	if (GI && SectionText)
	{
		FString Label = GetSectionLabel(GI->GetRatingSection());
		int32 Section = (int32)GI->GetRatingSection();
		SectionText->SetText(FText::FromString(
			FString::Printf(TEXT("SEC %d  %s"), Section, *Label)));
		SectionText->SetColorAndOpacity(FSlateColor(GetSectionColor(Section, true)));
	}
}

void UCabinStatusWidget::OnCreditsUpdated(int32 NewCredits)
{
	UpdateCredits(NewCredits);
}

void UCabinStatusWidget::OnDayUpdated(int32 NewDay)
{
	UpdateDay(NewDay);
}

// ============================================================
// РЕЙТИНГ-БАР (6 секций)
// ============================================================

void UCabinStatusWidget::UpdateRatingBar(float Rating)
{
	// Rating 1.0–6.0 → 6 секций
	// Секция i (1-based):
	//   i < floor(Rating) → 100% (заполнена)
	//   i == floor(Rating) → дробная часть
	//   i > floor(Rating) → 0%

	UProgressBar* Bars[] = { RatingBar_1, RatingBar_2, RatingBar_3,
	                          RatingBar_4, RatingBar_5, RatingBar_6 };

	const int32 FullSection = FMath::FloorToInt32(Rating);       // 1–6
	const float FractionalPart = Rating - (float)FullSection;    // 0.0–0.99

	for (int32 i = 0; i < 6; i++)
	{
		if (!Bars[i]) continue;

		const int32 SectionNum = i + 1; // 1-based
		float Percent = 0.0f;
		bool bIsActive = false;

		if (SectionNum < FullSection)
		{
			// Полностью заполнена
			Percent = 1.0f;
		}
		else if (SectionNum == FullSection)
		{
			// Текущая секция — дробная часть
			Percent = FractionalPart;
			bIsActive = true;

			// При Rating == 6.0 (макс) → секция 6 полная
			if (Rating >= 6.0f) Percent = 1.0f;
		}
		else
		{
			// Пустая
			Percent = 0.0f;
		}

		Bars[i]->SetPercent(Percent);
		Bars[i]->SetFillColorAndOpacity(GetSectionColor(SectionNum, bIsActive));
	}

	// Текст секции
	UGI_DimensionRunner* GI = GetGI();
	if (GI && SectionText)
	{
		FString Label = GetSectionLabel(GI->GetRatingSection());
		int32 Section = (int32)GI->GetRatingSection();
		SectionText->SetText(FText::FromString(
			FString::Printf(TEXT("SEC %d  %s"), Section, *Label)));
		SectionText->SetColorAndOpacity(FSlateColor(GetSectionColor(Section, true)));
	}
}

void UCabinStatusWidget::UpdateCredits(int32 Credits)
{
	if (CreditsText)
	{
		CreditsText->SetText(FText::FromString(
			FString::Printf(TEXT("%d₪"), Credits)));
		// Тёмно-зелёный цвет для суммы денег
		CreditsText->SetColorAndOpacity(FSlateColor(FLinearColor(0.15f, 0.75f, 0.2f)));
	}
}

void UCabinStatusWidget::UpdateDay(int32 Day)
{
	if (DayText)
	{
		DayText->SetText(FText::FromString(
			FString::Printf(TEXT("ДЕНЬ %d"), Day)));
		// Бирюзовый цвет для дня
		DayText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.85f, 0.9f)));
	}
}

// ============================================================
// ЦВЕТА СЕКЦИЙ
// ============================================================

FLinearColor UCabinStatusWidget::GetSectionColor(int32 Section, bool bActive)
{
	// Пульсирующая текущая секция — чуть ярче
	const float Brightness = bActive ? 1.0f : 0.7f;

	switch (Section)
	{
	case 1: return FLinearColor(0.9f, 0.1f, 0.1f, 1.0f) * Brightness; // Красный
	case 2: return FLinearColor(0.9f, 0.4f, 0.1f, 1.0f) * Brightness; // Оранжевый
	case 3: return FLinearColor(0.9f, 0.8f, 0.1f, 1.0f) * Brightness; // Жёлтый
	case 4: return FLinearColor(0.4f, 0.8f, 0.2f, 1.0f) * Brightness; // Салатовый
	case 5: return FLinearColor(0.1f, 0.8f, 0.3f, 1.0f) * Brightness; // Зелёный
	case 6: return FLinearColor(0.2f, 0.6f, 1.0f, 1.0f) * Brightness; // Голубой/золотой
	default: return FLinearColor::Gray;
	}
}

// ============================================================
// ТЕКСТ СЕКЦИИ (вместо emoji)
// ============================================================

FString UCabinStatusWidget::GetSectionLabel(ERatingSection Section)
{
	switch (Section)
	{
	case ERatingSection::Section1: return TEXT("КРИТИЧНО");
	case ERatingSection::Section2: return TEXT("ПЛОХО");
	case ERatingSection::Section3: return TEXT("НОРМА");
	case ERatingSection::Section4: return TEXT("ХОРОШО");
	case ERatingSection::Section5: return TEXT("ОТЛИЧНО");
	case ERatingSection::Section6: return TEXT("МАКСИМУМ");
	default: return TEXT("?");
	}
}

// ============================================================
// ХЕЛПЕР
// ============================================================

UGI_DimensionRunner* UCabinStatusWidget::GetGI() const
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return Cast<UGI_DimensionRunner>(GI);
}
