// CabinInventoryComponent.h — Инвентарь кабины (слоты для артефактов)
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CabinInventoryComponent.generated.h"

class AArtifactBase;
class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct FCabinSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CabinSlot")
	TObjectPtr<AArtifactBase> Artifact = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "CabinSlot")
	bool bOccupied = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCabinInventoryChanged);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PHOENIX_API UCabinInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCabinInventoryComponent();

	// ==================== НАСТРОЙКИ ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CabinInventory|Settings")
	int32 StartSlots = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CabinInventory|Settings")
	int32 MaxSlots = 12;

	/** Смещение от центра кабины для выгрузки предмета */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CabinInventory|Settings")
	FVector UnloadOffset = FVector(0.f, 0.f, 50.f);

	// ==================== ДЕЛЕГАТ ====================

	UPROPERTY(BlueprintAssignable, Category = "CabinInventory")
	FOnCabinInventoryChanged OnInventoryChanged;

	// ==================== КОМАНДЫ ====================

	/** Инициализация слотов и привязка триггера */
	void Initialize(UPrimitiveComponent* InTrigger);

	/** Загрузить ближайший 1 предмет из триггер-зоны */
	UFUNCTION(BlueprintCallable, Category = "CabinInventory")
	bool LoadItem();

	/** Листать активный слот: 0→1→2→0 */
	UFUNCTION(BlueprintCallable, Category = "CabinInventory")
	void SelectNextSlot();

	/** Выгрузить предмет из активного слота */
	UFUNCTION(BlueprintCallable, Category = "CabinInventory")
	bool UnloadItem();

	// ==================== СОСТОЯНИЕ ====================

	UFUNCTION(BlueprintPure, Category = "CabinInventory|State")
	FCabinSlot GetSlotInfo(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "CabinInventory|State")
	int32 GetActiveSlotIndex() const { return ActiveSlotIndex; }

	UFUNCTION(BlueprintPure, Category = "CabinInventory|State")
	int32 GetSlotCount() const { return Slots.Num(); }

	UFUNCTION(BlueprintPure, Category = "CabinInventory|State")
	int32 GetOccupiedCount() const;

	// ==================== ПРЯМОЙ ДОСТУП К СЛОТАМ ====================

	/** Принудительно загрузить артефакт в слот (для восстановления из сохранения) */
	void StoreArtifact(int32 SlotIndex, AArtifactBase* Artifact);

	/** Восстановить (выгрузить) артефакт из слота */
	void RestoreArtifact(int32 SlotIndex);

	/** Установить активный слот */
	void SetActiveSlotIndex(int32 Index) { ActiveSlotIndex = FMath::Clamp(Index, 0, Slots.Num() - 1); }

private:
	UPROPERTY()
	TArray<FCabinSlot> Slots;

	int32 ActiveSlotIndex = 0;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> InventoryTrig;
};
