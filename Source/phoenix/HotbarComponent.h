// HotbarComponent.h — Хотбар игрока: 2 слота, выбор кнопкой, предметы.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DimensionRunnerTypes.h"
#include "HotbarComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotSelected, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSlotsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemUsed, FName, ItemID, int32, RemainingQty);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PHOENIX_API UHotbarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHotbarComponent();

	// ==================== ПАРАМЕТРЫ ====================

	/** Количество слотов (по умолчанию 2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hotbar", meta = (ClampMin = "1", ClampMax = "6"))
	int32 NumSlots = 2;

	// ==================== СОСТОЯНИЕ ====================

	/** Массив слотов */
	UPROPERTY(BlueprintReadOnly, Category = "Hotbar|State")
	TArray<FHotbarSlot> Slots;

	/** Индекс текущего выбранного слота (-1 = ничего) */
	UPROPERTY(BlueprintReadOnly, Category = "Hotbar|State")
	int32 ActiveSlotIndex = -1;

	// ==================== ДЕЛЕГАТЫ ====================

	/** Выбран слот (для подсветки в UI) */
	UPROPERTY(BlueprintAssignable, Category = "Hotbar|Events")
	FOnSlotSelected OnSlotSelected;

	/** Содержимое слотов изменилось (для обновления иконок) */
	UPROPERTY(BlueprintAssignable, Category = "Hotbar|Events")
	FOnSlotsChanged OnSlotsChanged;

	/** Предмет использован */
	UPROPERTY(BlueprintAssignable, Category = "Hotbar|Events")
	FOnItemUsed OnItemUsed;

	// ==================== ФУНКЦИИ ====================

	/** Выбрать слот по индексу (0-based). -1 = снять выбор */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void SelectSlot(int32 Index);

	/** Добавить предмет в первый свободный слот. Возвращает true если удалось */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool AddItem(FName ItemID, int32 Quantity = 1);

	/** Добавить предмет в конкретный слот (перезаписывает) */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void SetSlot(int32 SlotIndex, FName ItemID, int32 Quantity = 1);

	/** Убрать предмет из слота */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void RemoveItem(int32 SlotIndex);

	/** Использовать предмет из активного слота */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void UseActiveItem();

	/** Вернуть данные активного слота */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	FHotbarSlot GetActiveSlot() const;

	/** Вернуть данные любого слота */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	FHotbarSlot GetSlot(int32 Index) const;

	/** Есть ли предмет где-то в хотбаре? */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	bool HasItem(FName ItemID) const;

	/** Очистить все слоты */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void ClearAll();

protected:
	virtual void BeginPlay() override;

private:
	/** Привязка кнопок 1–N к слотам */
	void SetupInputBindings();

	void OnHotbar1Pressed() { SelectSlot(0); }
	void OnHotbar2Pressed() { SelectSlot(1); }
};
