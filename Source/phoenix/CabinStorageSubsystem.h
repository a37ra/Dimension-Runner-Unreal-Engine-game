// CabinStorageSubsystem.h — Сохранение инвентаря кабины между уровнями
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CabinStorageSubsystem.generated.h"

/**
 * Данные одного сохранённого слота кабины.
 */
USTRUCT()
struct FSavedCabinSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<AActor> ArtifactClass;

	UPROPERTY()
	FName ItemID;

	UPROPERTY()
	FName ItemName;

	UPROPERTY()
	float ItemHP = 100.0f;

	UPROPERTY()
	bool bOccupied = false;
};

/**
 * Подсистема GameInstance — хранит инвентарь кабины между сменами уровней.
 * Создаётся автоматически движком, живёт пока запущена игра.
 */
UCLASS()
class PHOENIX_API UCabinStorageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FSavedCabinSlot> SavedSlots;

	UPROPERTY()
	int32 SavedActiveSlot = 0;

	bool HasSavedData() const { return SavedSlots.Num() > 0; }
	void ClearSavedData() { SavedSlots.Empty(); SavedActiveSlot = 0; }
};
