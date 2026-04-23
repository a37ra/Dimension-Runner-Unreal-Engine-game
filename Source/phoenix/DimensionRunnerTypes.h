// DimensionRunnerTypes.h — Общие типы для Dimension Runner
// Энумы, структуры заказов и измерений.
// Этот файл подключается везде: GameInstance, TravelCabin, UI и т.д.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DimensionRunnerTypes.generated.h"

// ============================================================
// НАСТРОЕНИЕ КЛИЕНТА
// ============================================================

UENUM(BlueprintType)
enum class EClientMood : uint8
{
	Bad      UMETA(DisplayName = "Плохое 😡"),
	Neutral  UMETA(DisplayName = "Нейтральное 😐"),
	Good     UMETA(DisplayName = "Хорошее 😊")
};

// ============================================================
// ИЗМЕРЕНИЯ
// ============================================================

UENUM(BlueprintType)
enum class EDimensionID : uint8
{
	Delta01_Mirror       UMETA(DisplayName = "Δ-01 Зеркало"),
	Delta02_ZeroG        UMETA(DisplayName = "Δ-02 Невесомость"),
	Delta03_Darkness     UMETA(DisplayName = "Δ-03 Темнота"),
	Delta04_Collapse     UMETA(DisplayName = "Δ-04 Коллапс"),
	Delta05_Labyrinth    UMETA(DisplayName = "Δ-05 Лабиринт")
};

// ============================================================
// СЕКЦИЯ РЕЙТИНГА (1–6)
// ============================================================

UENUM(BlueprintType)
enum class ERatingSection : uint8
{
	None = 0      UMETA(Hidden),
	Section1 = 1  UMETA(DisplayName = "😡 Критично"),
	Section2 = 2  UMETA(DisplayName = "😤 Плохо"),
	Section3 = 3  UMETA(DisplayName = "😐 Норма"),
	Section4 = 4  UMETA(DisplayName = "🙂 Хорошо"),
	Section5 = 5  UMETA(DisplayName = "😊 Отлично"),
	Section6 = 6  UMETA(DisplayName = "🌟 Максимум")
};

// ============================================================
// ДАННЫЕ ЗАКАЗА (строка DataTable — DT_Orders)
// ============================================================

/**
 * FOrderData — одна строка в таблице заказов DT_Orders.
 * 
 * Связь с предметами:
 *   ItemID (FName) == AArtifactBase::ItemID
 *   При сдаче заказа сравниваем эти два поля.
 *
 * Связь с DT_Items (каталог предметов):
 *   RowName в DT_Items == ItemID здесь.
 *   Так можно найти иконку, меш, Class и т.д. для UI.
 */
USTRUCT(BlueprintType)
struct FOrderData : public FTableRowBase
{
	GENERATED_BODY()

	/** ID предмета — совпадает с AArtifactBase::ItemID и RowName в DT_Items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	FName ItemID;

	/** Отображаемое имя для UI карточки заказа */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	FText DisplayName;

	/** Базовая оплата за правильный предмет (₪) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	int32 Payment = 100;

	/** Настроение клиента — влияет на штрафы/бонусы */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	EClientMood Mood = EClientMood::Neutral;

	/** Измерение — куда летит кабина */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	EDimensionID DimensionID = EDimensionID::Delta05_Labyrinth;

	/** Время на выполнение (секунды). Штрафной заказ = ×0.5 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	float TimerSeconds = 185.0f;

	/** Бонус к оплате за опасность измерения (0.0 = +0%, 0.25 = +25%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	float DimensionBonus = 0.0f;

	bool IsValid() const { return !ItemID.IsNone(); }
};

// ============================================================
// СЛОТ ХОТБАРА
// ============================================================

USTRUCT(BlueprintType)
struct FHotbarSlot
{
	GENERATED_BODY()

	/** ID предмета (совпадает с RowName в DT_Items) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hotbar")
	FName ItemID;

	/** Количество (для расходников) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hotbar")
	int32 Quantity = 0;

	bool IsEmpty() const { return ItemID.IsNone() || Quantity <= 0; }
};

// ============================================================
// ИТОГИ ДНЯ (ДЛЯ СТАТИСТИКИ И ЭКРАНА)
// ============================================================

USTRUCT(BlueprintType)
struct FDailySummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "DayStats")
	int32 OrdersCompleted = 0;

	UPROPERTY(BlueprintReadWrite, Category = "DayStats")
	int32 OrdersFailed = 0;

	UPROPERTY(BlueprintReadWrite, Category = "DayStats")
	int32 CreditsEarned = 0;

	UPROPERTY(BlueprintReadWrite, Category = "DayStats")
	float RatingDelta = 0.0f;
};

// ============================================================
// МАППИНГ ИЗМЕРЕНИЙ → УРОВНИ
// ============================================================

/**
 * Возвращает имя уровня для указанного измерения.
 * Добавляй сюда новые измерения по мере создания уровней.
 */
inline FName GetLevelNameForDimension(EDimensionID Dimension)
{
	switch (Dimension)
	{
	case EDimensionID::Delta01_Mirror:    return TEXT("L_Mirror");
	case EDimensionID::Delta02_ZeroG:     return TEXT("L_ZeroG");
	case EDimensionID::Delta03_Darkness:  return TEXT("L_Darkness");
	case EDimensionID::Delta04_Collapse:  return TEXT("L_Collapse");
	case EDimensionID::Delta05_Labyrinth: return TEXT("L_WorldA"); // Твой текущий уровень!
	default:                              return TEXT("L_WorldA"); // По умолчанию L_WorldA
	}
}

/** Имя уровня базы (лаборатория) */
inline FName GetBaseLevelName()
{
	return TEXT("L_Laboratory");
}
