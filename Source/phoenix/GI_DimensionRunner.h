// GI_DimensionRunner.h — Главный GameInstance для Dimension Runner
// Хранит глобальное состояние: экономика, рейтинг, заказы, статус кабины.
// Заменяет Blueprint GI_Phoenix.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Tickable.h"
#include "DimensionRunnerTypes.h"
#include "GI_DimensionRunner.generated.h"

class UDataTable;

// ============================================================
// ДЕЛЕГАТЫ
// ============================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCreditsChanged, int32, NewCredits);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRatingChanged, float, NewRating);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDayChanged, int32, NewDay);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOrderChanged, const FOrderData&, NewOrder);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDayEnded, const FDailySummary&, Summary);

/**
 * UGI_DimensionRunner — C++ GameInstance.
 * 
 * Установить в Project Settings → Maps & Modes → Game Instance Class.
 * Или создать BP ребёнка и назначить его.
 *
 * Хранит всё глобальное состояние между уровнями:
 *   - Экономика (Credits, Rating, Day)
 *   - Текущий заказ
 *   - Статус кабины (Active / Overheated)
 */
UCLASS(Blueprintable)
class PHOENIX_API UGI_DimensionRunner : public UGameInstance, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UGI_DimensionRunner();

	virtual void Init() override;

	// ============================================================
	// FTickableGameObject
	// ============================================================
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGI_DimensionRunner, STATGROUP_Tickables); }

	// ============================================================
	// ГЛОБАЛЬНОЕ ВРЕМЯ И ИТОГИ ДНЯ
	// ============================================================

	/** Сколько секунд длится один день */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Time")
	float DayDurationSeconds = 900.0f; // 15 минут

	/** Оставшееся время текущего дня */
	UPROPERTY(BlueprintReadOnly, Category = "Economy|Time")
	float DayTimeRemaining = 0.0f;

	/** Флаг, идёт ли сейчас день */
	UPROPERTY(BlueprintReadOnly, Category = "Economy|Time")
	bool bIsDayActive = false;

	/** Статистика за текущий день (сбрасывается каждое утро) */
	UPROPERTY(BlueprintReadOnly, Category = "Economy|Time")
	FDailySummary CurrentDayStats;

	UFUNCTION(BlueprintPure, Category = "Economy|Time")
	FString GetFormattedDayTime() const;

	/** Принудительно начать день заново (вызывается при старте игры или после итогов) */
	UFUNCTION(BlueprintCallable, Category = "Economy|Time")
	void StartSequenceNewDay();

	/** Принудительно закончить день (по таймеру или через UI) */
	UFUNCTION(BlueprintCallable, Category = "Economy|Time")
	void EndCurrentDay();

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnDayEnded OnDayEnded;

	// ============================================================
	// СТАТУС КАБИНЫ (перенесено из GI_Phoenix Blueprint)
	// ============================================================

	/** Кабина активна (на миссии)? */
	UPROPERTY(BlueprintReadWrite, Category = "Cabin")
	bool bCabinActive = false;

	/** Кабина перегрета (кулдаун после возврата)? */
	UPROPERTY(BlueprintReadWrite, Category = "Cabin")
	bool bIsOverheated = false;

	/** Есть сохранённые данные инвентаря? (для CabinStorageSubsystem) */
	UPROPERTY(BlueprintReadWrite, Category = "Cabin")
	bool bHasInventoryData = false;

	// Функции совместимости (TravelCabin вызывает напрямую)
	UFUNCTION(BlueprintPure, Category = "Cabin")
	bool CheckCabinStatus() const { return bCabinActive; }

	UFUNCTION(BlueprintCallable, Category = "Cabin")
	void SetCabinStatus(bool bActive) { bCabinActive = bActive; }

	UFUNCTION(BlueprintPure, Category = "Cabin")
	bool CheckOverheatStatus() const { return bIsOverheated; }

	UFUNCTION(BlueprintCallable, Category = "Cabin")
	void SetOverheatStatus(bool bOverheated) { bIsOverheated = bOverheated; }

	// ============================================================
	// ЭКОНОМИКА
	// ============================================================

	/** Деньги игрока (₪) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 Credits = 0;

	/** Рейтинг (1.0–6.0). Стартовый = 3.0. Секция = floor(Rating) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	float Rating = 3.0f;

	/** Текущий день (1–6, потом бесконечно) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 CurrentDay = 1;

	// --- Деньги ---

	UFUNCTION(BlueprintCallable, Category = "Economy")
	void AddCredits(int32 Amount);

	/** Потратить деньги. Возвращает true если хватило */
	UFUNCTION(BlueprintCallable, Category = "Economy")
	bool SpendCredits(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Economy")
	int32 GetCredits() const { return Credits; }

	// --- Рейтинг ---

	/** Изменить рейтинг на Delta (может быть отрицательным). Clamp 1.0–6.0 */
	UFUNCTION(BlueprintCallable, Category = "Economy")
	void ModifyRating(float Delta);

	/** Текущая секция рейтинга (1–6) */
	UFUNCTION(BlueprintPure, Category = "Economy")
	ERatingSection GetRatingSection() const;

	/** Рейтинг в секции 1? Следующий заказ штрафной */
	UFUNCTION(BlueprintPure, Category = "Economy")
	bool IsPenaltyDay() const;

	UFUNCTION(BlueprintPure, Category = "Economy")
	float GetRating() const { return Rating; }

	/** Эмодзи текущей секции рейтинга */
	UFUNCTION(BlueprintPure, Category = "Economy")
	FString GetRatingSectionEmoji() const;

	/** Эмодзи настроения клиента */
	UFUNCTION(BlueprintPure, Category = "Economy")
	static FString GetMoodEmoji(EClientMood Mood);

	/** Отображаемое имя измерения */
	UFUNCTION(BlueprintPure, Category = "Economy")
	static FString GetDimensionDisplayName(EDimensionID Dim);

	// --- День ---

	UFUNCTION(BlueprintPure, Category = "Economy")
	int32 GetCurrentDay() const { return CurrentDay; }

	// ============================================================
	// ЗАКАЗЫ
	// ============================================================

	/** Таблица с пулом заказов (назначить в BP или в DefaultProperties) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orders")
	TObjectPtr<UDataTable> OrdersTable;

	/** Таблица с каталогом предметов (DT_Items, для UI и данных) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orders")
	TObjectPtr<UDataTable> ItemsTable;

	/** Текущий активный заказ */
	UPROPERTY(BlueprintReadOnly, Category = "Orders")
	FOrderData CurrentOrder;

	/** Есть ли активный заказ? */
	UPROPERTY(BlueprintReadOnly, Category = "Orders")
	bool bOrderActive = false;

	/** Найти строку в DT_Items по ItemID. Возвращает nullptr, если таблица не настроена или строки нет. */
	const FItemData* FindItemData(FName ItemID) const;

	/** Сгенерировать случайный заказ из OrdersTable. Учитывает штрафной день */
	UFUNCTION(BlueprintCallable, Category = "Orders")
	void GenerateNextOrder();

	/**
	 * Завершить заказ — игрок принёс предмет.
	 * @param DeliveredItemID — ItemID артефакта из кабины
	 * @return Сумма начисленных кредитов (0 если не тот предмет + плохое настроение)
	 */
	UFUNCTION(BlueprintCallable, Category = "Orders")
	int32 CompleteOrder(FName DeliveredItemID);

	/** Провалить заказ (смерть / время вышло). Штраф -12% денег, -1 секция рейтинга */
	UFUNCTION(BlueprintCallable, Category = "Orders")
	void FailOrder();

	UFUNCTION(BlueprintPure, Category = "Orders")
	bool IsOrderActive() const { return bOrderActive; }

	UFUNCTION(BlueprintPure, Category = "Orders")
	FOrderData GetCurrentOrder() const { return CurrentOrder; }

	/** Получить имя уровня для текущего заказа */
	UFUNCTION(BlueprintPure, Category = "Orders")
	FName GetCurrentMissionLevel() const;

	// ============================================================
	// ДЕЛЕГАТЫ (для UI)
	// ============================================================

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnCreditsChanged OnCreditsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnRatingChanged OnRatingChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnDayChanged OnDayChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnOrderChanged OnOrderChanged;

	// ============================================================
	// СБРОС
	// ============================================================

	/** Полный сброс до начального состояния (новая игра) */
	UFUNCTION(BlueprintCallable, Category = "Game")
	void ResetGame();

private:
	/** Штрафной таймер = базовый × PenaltyTimerMultiplier */
	static constexpr float PenaltyTimerMultiplier = 0.5f;

	/** Штраф за смерть: -12% от счёта */
	static constexpr float DeathCreditPenalty = 0.12f;

	/** Минимальный рейтинг */
	static constexpr float MinRating = 1.0f;

	/** Максимальный рейтинг */
	static constexpr float MaxRating = 6.0f;

	/** Стартовый рейтинг */
	static constexpr float DefaultRating = 3.0f;
};
