#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DimensionRunnerTypes.h"
#include "HotbarComponent.generated.h"

class USoundBase;
class AArtifactBase;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotSelected, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSlotsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemUsed, FName, ItemID, int32, RemainingQty);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemDropped, FName, ItemID);

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

	// --- Sounds ---

	/** Звук при переключении / выборе слота */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hotbar|Sounds")
	TObjectPtr<USoundBase> SlotSelectSound;

	/** Звук поднятия предмета */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hotbar|Sounds")
	TObjectPtr<USoundBase> ItemPickupSound;

	/** Звук опускания / выброса предмета */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hotbar|Sounds")
	TObjectPtr<USoundBase> ItemDropSound;

	/** На сколько юнитов вперёд игрока бросить выброшенный предмет */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hotbar")
	float DropForwardDistance = 150.0f;

	/** Импульс при выбросе (вверх-вперёд) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hotbar")
	float DropImpulse = 400.0f;

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

	/** Предмет выброшен (для удаления из мира/GI) */
	UPROPERTY(BlueprintAssignable, Category = "Hotbar|Events")
	FOnItemDropped OnItemDropped;

	// ==================== ФУНКЦИИ ====================

	/** Выбрать слот по индексу (0-based). -1 = снять выбор */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void SelectSlot(int32 Index);

	/** Прокрутить к следующему слоту (скролл вниз) */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void ScrollNext();

	/** Прокрутить к предыдущему слоту (скролл вверх) */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void ScrollPrev();

	/** Добавить предмет в первый свободный слот. Возвращает true если удалось */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool AddItem(FName ItemID, int32 Quantity = 1);

	/** Добавить предмет в конкретный слот (перезаписывает) */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void SetSlot(int32 SlotIndex, FName ItemID, int32 Quantity = 1);

	/** Убрать предмет из слота */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void RemoveItem(int32 SlotIndex);

	/** Выбросить предмет из активного слота в мир */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void DropActiveItem();

	/** Использовать предмет из активного слота */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void UseActiveItem();

	/** Попробовать подобрать физический предмет напрямую в хотбар. */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool TryPickupArtifact(AArtifactBase* Artifact);

	/** Вернуть данные активного слота */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	FHotbarSlot GetActiveSlot() const;

	/** Вернуть данные любого слота */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	FHotbarSlot GetSlot(int32 Index) const;

	/** Есть ли предмет где-то в хотбаре? */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	bool HasItem(FName ItemID) const;

	/** Можно ли показывать подсказку использования для активного слота? */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	bool HasUsableActiveItem() const;

	/** Текст подсказки использования для UI. */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	FText GetActiveUsePrompt() const;

	/** Очистить все слоты */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void ClearAll();

	/** Воспроизвести звук поднятия (вызывается снаружи при подборе предмета) */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void PlayPickupSound();

	/** Списать предмет из конкретного слота без применения эффекта (например, при загрузке в кабину). */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool ConsumeItemInSlot(int32 SlotIndex, int32 Quantity = 1);

	/** Проверка: этот физический предмет надо подбирать в хотбар, а не лучом? */
	bool CanPickupArtifactToHotbar(const AArtifactBase* Artifact) const;

protected:
	virtual void BeginPlay() override;

private:
	void SetupInputBindings();
	void PlaySoundAtOwner(USoundBase* Sound) const;
	const FItemData* FindItemData(FName ItemID) const;
	bool BuildSlotDataFromItemID(FName ItemID, FHotbarSlot& OutSlotData) const;

	void OnHotbar1Pressed() { SelectSlot(0); }
	void OnHotbar2Pressed() { SelectSlot(1); }
	void OnScrollUp()       { ScrollPrev(); }
	void OnScrollDown()     { ScrollNext(); }
	void OnDropPressed()    { DropActiveItem(); }
	void OnUsePressed()     { UseActiveItem(); }
};
