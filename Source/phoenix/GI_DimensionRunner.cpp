// GI_DimensionRunner.cpp — Реализация GameInstance
#include "GI_DimensionRunner.h"
#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogDimensionRunner, Log, All);

// ============================================================
// КОНСТРУКТОР + INIT
// ============================================================

UGI_DimensionRunner::UGI_DimensionRunner()
{
}

void UGI_DimensionRunner::Init()
{
	Super::Init();

	UE_LOG(LogDimensionRunner, Log, TEXT("=== GI_DimensionRunner initialized ==="));

	if (!OrdersTable)
	{
		UE_LOG(LogDimensionRunner, Warning,
			TEXT("OrdersTable is NOT set! Assign DT_Orders in Blueprint defaults or Class Defaults."));
	}
	if (!ItemsTable)
	{
		UE_LOG(LogDimensionRunner, Warning,
			TEXT("ItemsTable is NOT set! Assign DT_Items in Blueprint defaults or Class Defaults."));
	}

	// Сброс состояния и запуск дня (т.к. нет сохранений пока)
	ResetGame();
}

// ============================================================
// ДЕНЬГИ
// ============================================================

void UGI_DimensionRunner::AddCredits(int32 Amount)
{
	if (Amount <= 0) return;

	Credits += Amount;
	UE_LOG(LogDimensionRunner, Log, TEXT("Credits: +%d → %d"), Amount, Credits);
	OnCreditsChanged.Broadcast(Credits);
}

bool UGI_DimensionRunner::SpendCredits(int32 Amount)
{
	if (Amount <= 0) return true;
	if (Credits < Amount)
	{
		UE_LOG(LogDimensionRunner, Warning, TEXT("Not enough credits! Need %d, have %d"), Amount, Credits);
		return false;
	}

	Credits -= Amount;
	UE_LOG(LogDimensionRunner, Log, TEXT("Credits: -%d → %d"), Amount, Credits);
	OnCreditsChanged.Broadcast(Credits);
	return true;
}

// ============================================================
// РЕЙТИНГ
// ============================================================

void UGI_DimensionRunner::ModifyRating(float Delta)
{
	const float OldRating = Rating;
	Rating = FMath::Clamp(Rating + Delta, MinRating, MaxRating);
	UE_LOG(LogDimensionRunner, Log, TEXT("Rating: %.1f → %.1f (delta: %+.1f) | Section: %d"),
		OldRating, Rating, Delta, (int32)GetRatingSection());
	OnRatingChanged.Broadcast(Rating);
}

ERatingSection UGI_DimensionRunner::GetRatingSection() const
{
	// 1.0–1.99 = S1, 2.0–2.99 = S2, ..., 5.0–5.99 = S5, 6.0 = S6
	int32 Section = FMath::Clamp(FMath::FloorToInt32(Rating), 1, 6);
	return static_cast<ERatingSection>(Section);
}

bool UGI_DimensionRunner::IsPenaltyDay() const
{
	return GetRatingSection() == ERatingSection::Section1;
}

FString UGI_DimensionRunner::GetRatingSectionEmoji() const
{
	switch (GetRatingSection())
	{
	case ERatingSection::Section1: return TEXT("\xF0\x9F\x98\xA1"); // 😡
	case ERatingSection::Section2: return TEXT("\xF0\x9F\x98\xA4"); // 😤
	case ERatingSection::Section3: return TEXT("\xF0\x9F\x98\x90"); // 😐
	case ERatingSection::Section4: return TEXT("\xF0\x9F\x99\x82"); // 🙂
	case ERatingSection::Section5: return TEXT("\xF0\x9F\x98\x8A"); // 😊
	case ERatingSection::Section6: return TEXT("\xF0\x9F\x8C\x9F"); // 🌟
	default: return TEXT("?");
	}
}

FString UGI_DimensionRunner::GetMoodEmoji(EClientMood Mood)
{
	switch (Mood)
	{
	case EClientMood::Bad:     return TEXT("\xF0\x9F\x98\xA1"); // 😡
	case EClientMood::Neutral: return TEXT("\xF0\x9F\x98\x90"); // 😐
	case EClientMood::Good:    return TEXT("\xF0\x9F\x98\x8A"); // 😊
	default: return TEXT("?");
	}
}

FString UGI_DimensionRunner::GetDimensionDisplayName(EDimensionID Dim)
{
	switch (Dim)
	{
	case EDimensionID::Delta01_Mirror:    return TEXT("\u0394-01 \u0417\u0435\u0440\u043a\u0430\u043b\u043e");
	case EDimensionID::Delta02_ZeroG:     return TEXT("\u0394-02 \u041d\u0435\u0432\u0435\u0441\u043e\u043c\u043e\u0441\u0442\u044c");
	case EDimensionID::Delta03_Darkness:  return TEXT("\u0394-03 \u0422\u0435\u043c\u043d\u043e\u0442\u0430");
	case EDimensionID::Delta04_Collapse:  return TEXT("\u0394-04 \u041a\u043e\u043b\u043b\u0430\u043f\u0441");
	case EDimensionID::Delta05_Labyrinth: return TEXT("\u0394-05 \u041b\u0430\u0431\u0438\u0440\u0438\u043d\u0442");
	default: return TEXT("\u0394-?? \u041d\u0435\u0438\u0437\u0432\u0435\u0441\u0442\u043d\u043e");
	}
}
// ============================================================

void UGI_DimensionRunner::Tick(float DeltaTime)
{
	if (bIsDayActive && DayTimeRemaining > 0.0f)
	{
		// Пауза таймера в главном меню
		if (GetWorld() && GetWorld()->GetMapName().Contains(TEXT("MainMenu")))
		{
			return;
		}

		DayTimeRemaining -= DeltaTime;

		if (DayTimeRemaining <= 0.0f)
		{
			DayTimeRemaining = 0.0f;
			EndCurrentDay();
		}
	}
}

FString UGI_DimensionRunner::GetFormattedDayTime() const
{
	// Вычисляем сколько реальных секунд прошло с начала дня
	float ElapsedRealSeconds = FMath::Max(0.0f, DayDurationSeconds - DayTimeRemaining);

	// Переводим реальное время в игровые минуты. 
	// Полный день (DayDurationSeconds, например 480с) равен 24 часам (1440 минут)
	// Значит за 1 реальную секунду проходит (1440 / DayDurationSeconds) игровых минут
	float InGameMinutesElapsed = ElapsedRealSeconds * (1440.0f / DayDurationSeconds);

	int32 TotalMins = FMath::FloorToInt32(InGameMinutesElapsed);
	int32 Hours = (TotalMins / 60) % 24; // от 00 до 23
	int32 Mins = TotalMins % 60;         // от 00 до 59

	return FString::Printf(TEXT("%02d:%02d"), Hours, Mins);
}

void UGI_DimensionRunner::StartSequenceNewDay()
{
	CurrentDay++;

	// Секция 5-6 → рейтинг делится на 2 (откат напряжения)
	if (Rating >= 5.0f)
	{
		const float OldRating = Rating;
		Rating = FMath::Max(MinRating, Rating / 2.0f);
		UE_LOG(LogDimensionRunner, Log, TEXT("Rating reset! %.1f → %.1f (Section 5+ reached)"),
			OldRating, Rating);
		OnRatingChanged.Broadcast(Rating);
	}

	// Сброс таймера и статистики
	DayTimeRemaining = DayDurationSeconds;
	bIsDayActive = true;
	CurrentDayStats = FDailySummary();

	UE_LOG(LogDimensionRunner, Log, TEXT("=== DAY %d STARTED ==="), CurrentDay);
	OnDayChanged.Broadcast(CurrentDay);
}

void UGI_DimensionRunner::EndCurrentDay()
{
	if (!bIsDayActive) return;

	bIsDayActive = false;
	UE_LOG(LogDimensionRunner, Warning, TEXT("=== DAY %d ENDED ==="), CurrentDay);

	// Вызов делегата для показа экрана итогов
	OnDayEnded.Broadcast(CurrentDayStats);

	// (UI отвечает за показ анимации и вызов StartSequenceNewDay когда готово)
}

// ============================================================
// ЗАКАЗЫ
// ============================================================

const FItemData* UGI_DimensionRunner::FindItemData(FName ItemID) const
{
	if (!ItemsTable || ItemID.IsNone())
	{
		return nullptr;
	}

	return ItemsTable->FindRow<FItemData>(ItemID, TEXT("FindItemData"));
}

void UGI_DimensionRunner::GenerateNextOrder()
{
	if (!OrdersTable)
	{
		UE_LOG(LogDimensionRunner, Error, TEXT("Cannot generate order: OrdersTable is NULL!"));
		return;
	}

	// Все строки таблицы
	TArray<FName> RowNames = OrdersTable->GetRowNames();
	if (RowNames.Num() == 0)
	{
		UE_LOG(LogDimensionRunner, Error, TEXT("OrdersTable is EMPTY!"));
		return;
	}

	// Случайный заказ
	const int32 RandomIndex = FMath::RandRange(0, RowNames.Num() - 1);
	const FName RowName = RowNames[RandomIndex];

	FOrderData* Row = OrdersTable->FindRow<FOrderData>(RowName, TEXT("GenerateNextOrder"));
	if (!Row)
	{
		UE_LOG(LogDimensionRunner, Error, TEXT("Failed to read row '%s' from OrdersTable!"),
			*RowName.ToString());
		return;
	}

	CurrentOrder = *Row;

	// Штрафной день → таймер ×0.5
	if (IsPenaltyDay())
	{
		CurrentOrder.TimerSeconds *= PenaltyTimerMultiplier;
		UE_LOG(LogDimensionRunner, Warning, TEXT("PENALTY DAY! Timer halved: %.0f sec"),
			CurrentOrder.TimerSeconds);
	}

	bOrderActive = true;

	UE_LOG(LogDimensionRunner, Log,
		TEXT("New Order: '%s' | Pay: %d₪ | Mood: %d | Dim: %d | Timer: %.0fs"),
		*CurrentOrder.ItemID.ToString(),
		CurrentOrder.Payment,
		(int32)CurrentOrder.Mood,
		(int32)CurrentOrder.DimensionID,
		CurrentOrder.TimerSeconds);

	OnOrderChanged.Broadcast(CurrentOrder);
}

int32 UGI_DimensionRunner::CompleteOrder(FName DeliveredItemID)
{
	if (!bOrderActive)
	{
		UE_LOG(LogDimensionRunner, Warning, TEXT("CompleteOrder called but no active order!"));
		return 0;
	}

	const bool bCorrectItem = (DeliveredItemID == CurrentOrder.ItemID);
	int32 Payout = 0;
	float RatingDelta = 0.0f;

	if (bCorrectItem)
	{
		// === ПРАВИЛЬНЫЙ ПРЕДМЕТ ===
		// Вместо диких +2.0 делаем плавнее: +0.5 (половина секции)
		RatingDelta = 0.5f;
		Payout = CurrentOrder.Payment;

		// Бонус за опасность измерения
		Payout = FMath::RoundToInt32(Payout * (1.0f + CurrentOrder.DimensionBonus));

		// Бонус за хорошее настроение: +10–20%
		if (CurrentOrder.Mood == EClientMood::Good)
		{
			const float GoodBonus = FMath::FRandRange(0.10f, 0.20f);
			Payout = FMath::RoundToInt32(Payout * (1.0f + GoodBonus));
			RatingDelta += 0.25f; // Суммарно +0.75 за правильный предмет с хорошим настроением
		}

		UE_LOG(LogDimensionRunner, Log, TEXT("Order COMPLETE! Correct item '%s'. Pay: %d₪, Rating: +%.2f"),
			*DeliveredItemID.ToString(), Payout, RatingDelta);
	}
	else
	{
		// === НЕ ТОТ ПРЕДМЕТ ===
		switch (CurrentOrder.Mood)
		{
		case EClientMood::Good:
			// 50/50: ±0 или +0.5
			RatingDelta = (FMath::RandBool()) ? 0.5f : 0.0f;
			Payout = FMath::RoundToInt32(CurrentOrder.Payment * FMath::FRandRange(0.20f, 0.40f));
			UE_LOG(LogDimensionRunner, Log,
				TEXT("Wrong item, Good mood. Pay: %d₪, Rating: %+.2f"), Payout, RatingDelta);
			break;

		case EClientMood::Neutral:
			// 50/50: понизить на 0.25 или повысить на 0.25 (как просил юзер)
			RatingDelta = (FMath::RandBool()) ? 0.25f : -0.25f;
			Payout = FMath::RoundToInt32(CurrentOrder.Payment * FMath::FRandRange(0.20f, 0.40f));
			UE_LOG(LogDimensionRunner, Log,
				TEXT("Wrong item, Neutral mood. Pay: %d₪, Rating: %+.2f"), Payout, RatingDelta);
			break;

		case EClientMood::Bad:
			// Штраф за ошибку: -0.3 (чуть строже)
			RatingDelta = -0.3f;
			Payout = FMath::RoundToInt32(CurrentOrder.Payment * FMath::FRandRange(0.10f, 0.20f));
			UE_LOG(LogDimensionRunner, Log,
				TEXT("Wrong item, BAD mood! Pay: %d₪, Rating: %.2f"), Payout, RatingDelta);
			break;
		}
	}

	// Применяем
	if (Payout > 0)
	{
		AddCredits(Payout);
		CurrentDayStats.CreditsEarned += Payout;
	}
	
	if (RatingDelta != 0.0f)
	{
		ModifyRating(RatingDelta);
		CurrentDayStats.RatingDelta += RatingDelta;
	}

	CurrentDayStats.OrdersCompleted++;

	bOrderActive = false;

	return Payout;
}

void UGI_DimensionRunner::FailOrder()
{
	UE_LOG(LogDimensionRunner, Warning, TEXT("Order FAILED! (death or timeout)"));

	CurrentDayStats.OrdersFailed++;

	// -12% от общего счёта
	const int32 Penalty = FMath::RoundToInt32(Credits * DeathCreditPenalty);
	if (Penalty > 0)
	{
		Credits = FMath::Max(0, Credits - Penalty);
		CurrentDayStats.CreditsEarned -= Penalty; // Может уйти в минус (штраф)
		UE_LOG(LogDimensionRunner, Warning, TEXT("Credit penalty: -%d₪ → %d₪"), Penalty, Credits);
		OnCreditsChanged.Broadcast(Credits);
	}

	// -1 секция рейтинга
	ModifyRating(-1.0f);
	CurrentDayStats.RatingDelta -= 1.0f;

	bOrderActive = false;

	// Новый заказ (по GDD) генерируем сразу
	GenerateNextOrder();
}

FName UGI_DimensionRunner::GetCurrentMissionLevel() const
{
	if (!bOrderActive)
	{
		return GetLevelNameForDimension(EDimensionID::Delta05_Labyrinth);
	}
	return GetLevelNameForDimension(CurrentOrder.DimensionID);
}

// ============================================================
// СБРОС
// ============================================================

void UGI_DimensionRunner::ResetGame()
{
	Credits = 0;
	Rating = DefaultRating;
	CurrentDay = 0; // Сбрасываем в 0, чтобы StartSequenceNewDay сделал 1
	CurrentOrder = FOrderData();
	bOrderActive = false;
	bCabinActive = false;
	bIsOverheated = false;
	bHasInventoryData = false;

	UE_LOG(LogDimensionRunner, Log, TEXT("=== GAME RESET ==="));

	OnCreditsChanged.Broadcast(Credits);
	OnRatingChanged.Broadcast(Rating);
	
	StartSequenceNewDay();
	
	// Если нет заказа на старте — сгенерируем
	if (OrdersTable && !bOrderActive)
	{
		GenerateNextOrder();
	}
}
