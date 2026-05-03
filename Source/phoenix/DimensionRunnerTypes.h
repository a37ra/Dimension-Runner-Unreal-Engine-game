#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DimensionRunnerTypes.generated.h"

UENUM(BlueprintType)
enum class EClientMood : uint8
{
	Bad,
	Neutral,
	Good
};

UENUM(BlueprintType)
enum class EDimensionID : uint8
{
	Delta01_Mirror,
	Delta02_ZeroG,
	Delta03_Darkness,
	Delta04_Collapse,
	Delta05_Labyrinth
};

UENUM(BlueprintType)
enum class ERatingSection : uint8
{
	None = 0,
	Section1 = 1,
	Section2 = 2,
	Section3 = 3,
	Section4 = 4,
	Section5 = 5,
	Section6 = 6
};

USTRUCT(BlueprintType)
struct FOrderData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	int32 Payment = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	EClientMood Mood = EClientMood::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	EDimensionID DimensionID = EDimensionID::Delta05_Labyrinth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	float TimerSeconds = 185.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Order")
	float DimensionBonus = 0.0f;

	bool IsValid() const { return !ItemID.IsNone(); }
};

USTRUCT(BlueprintType)
struct FHotbarSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hotbar")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hotbar")
	int32 Quantity = 0;

	bool IsEmpty() const { return ItemID.IsNone() || Quantity <= 0; }
};

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

inline FName GetLevelNameForDimension(EDimensionID Dimension)
{
	switch (Dimension)
	{
	case EDimensionID::Delta01_Mirror:    return TEXT("L_Mirror");
	case EDimensionID::Delta02_ZeroG:     return TEXT("L_ZeroG");
	case EDimensionID::Delta03_Darkness:  return TEXT("L_Darkness");
	case EDimensionID::Delta04_Collapse:  return TEXT("L_Collapse");
	case EDimensionID::Delta05_Labyrinth: return TEXT("L_WorldA");
	default:                              return TEXT("L_WorldA");
	}
}

inline FName GetBaseLevelName()
{
	return TEXT("L_Laboratory");
}
