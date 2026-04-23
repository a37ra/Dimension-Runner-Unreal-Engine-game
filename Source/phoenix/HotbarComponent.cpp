// HotbarComponent.cpp — Хотбар: слоты, предметы, ввод с клавиатуры
#include "HotbarComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"

DEFINE_LOG_CATEGORY_STATIC(LogHotbar, Log, All);

UHotbarComponent::UHotbarComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // Хотбар не нуждается в Tick
}

void UHotbarComponent::BeginPlay()
{
	Super::BeginPlay();

	// Инициализируем слоты
	Slots.SetNum(NumSlots);
	for (int32 i = 0; i < NumSlots; ++i)
	{
		Slots[i] = FHotbarSlot();
	}

	UE_LOG(LogHotbar, Log, TEXT("HotbarComponent: Initialized with %d slots."), NumSlots);

	// Привязка клавиш
	SetupInputBindings();
}

void UHotbarComponent::SetupInputBindings()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) return;

	UInputComponent* InputComp = OwnerPawn->InputComponent;
	if (!InputComp) return;

	// Привязка клавиш 1, 2
	InputComp->BindKey(EKeys::One, IE_Pressed, this, &UHotbarComponent::OnHotbar1Pressed);
	InputComp->BindKey(EKeys::Two, IE_Pressed, this, &UHotbarComponent::OnHotbar2Pressed);

	UE_LOG(LogHotbar, Log, TEXT("HotbarComponent: Keys 1-%d bound."), FMath::Min(NumSlots, 2));
}

void UHotbarComponent::SelectSlot(int32 Index)
{
	if (Index < -1 || Index >= Slots.Num())
	{
		UE_LOG(LogHotbar, Warning, TEXT("SelectSlot: Invalid index %d (max %d)"), Index, Slots.Num() - 1);
		return;
	}

	// Повторное нажатие снимает выбор
	if (ActiveSlotIndex == Index)
	{
		ActiveSlotIndex = -1;
		UE_LOG(LogHotbar, Log, TEXT("Slot deselected."));
	}
	else
	{
		ActiveSlotIndex = Index;
		UE_LOG(LogHotbar, Log, TEXT("Slot %d selected. Item: '%s' x%d"),
			Index,
			*Slots[Index].ItemID.ToString(),
			Slots[Index].Quantity);
	}

	OnSlotSelected.Broadcast(ActiveSlotIndex);
}

bool UHotbarComponent::AddItem(FName ItemID, int32 Quantity)
{
	if (ItemID.IsNone() || Quantity <= 0) return false;

	// Сначала ищем существующий стак
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].ItemID == ItemID)
		{
			Slots[i].Quantity += Quantity;
			UE_LOG(LogHotbar, Log, TEXT("AddItem: Stacked '%s' in slot %d. Total: %d"),
				*ItemID.ToString(), i, Slots[i].Quantity);
			OnSlotsChanged.Broadcast();
			return true;
		}
	}

	// Ищем пустой слот
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].IsEmpty())
		{
			Slots[i].ItemID = ItemID;
			Slots[i].Quantity = Quantity;
			UE_LOG(LogHotbar, Log, TEXT("AddItem: '%s' x%d → slot %d"),
				*ItemID.ToString(), Quantity, i);
			OnSlotsChanged.Broadcast();
			return true;
		}
	}

	UE_LOG(LogHotbar, Warning, TEXT("AddItem: No empty slot for '%s'!"), *ItemID.ToString());
	return false;
}

void UHotbarComponent::SetSlot(int32 SlotIndex, FName ItemID, int32 Quantity)
{
	if (!Slots.IsValidIndex(SlotIndex)) return;

	Slots[SlotIndex].ItemID = ItemID;
	Slots[SlotIndex].Quantity = Quantity;

	UE_LOG(LogHotbar, Log, TEXT("SetSlot: Slot %d = '%s' x%d"), SlotIndex, *ItemID.ToString(), Quantity);
	OnSlotsChanged.Broadcast();
}

void UHotbarComponent::RemoveItem(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return;

	UE_LOG(LogHotbar, Log, TEXT("RemoveItem: Cleared slot %d (was '%s')"),
		SlotIndex, *Slots[SlotIndex].ItemID.ToString());

	Slots[SlotIndex] = FHotbarSlot();

	// Если убрали активный слот — снимаем выбор
	if (ActiveSlotIndex == SlotIndex)
	{
		ActiveSlotIndex = -1;
		OnSlotSelected.Broadcast(-1);
	}

	OnSlotsChanged.Broadcast();
}

void UHotbarComponent::UseActiveItem()
{
	if (ActiveSlotIndex < 0 || !Slots.IsValidIndex(ActiveSlotIndex))
	{
		UE_LOG(LogHotbar, Warning, TEXT("UseActiveItem: No active slot!"));
		return;
	}

	FHotbarSlot& Slot = Slots[ActiveSlotIndex];
	if (Slot.IsEmpty())
	{
		UE_LOG(LogHotbar, Warning, TEXT("UseActiveItem: Active slot %d is empty!"), ActiveSlotIndex);
		return;
	}

	// Расходуем 1 штуку
	Slot.Quantity -= 1;
	const FName UsedID = Slot.ItemID;

	UE_LOG(LogHotbar, Log, TEXT("UseActiveItem: Used '%s'. Remaining: %d"),
		*UsedID.ToString(), Slot.Quantity);

	OnItemUsed.Broadcast(UsedID, Slot.Quantity);

	// Если кончился — очищаем слот
	if (Slot.Quantity <= 0)
	{
		Slot = FHotbarSlot();
		OnSlotsChanged.Broadcast();
	}
}

FHotbarSlot UHotbarComponent::GetActiveSlot() const
{
	if (ActiveSlotIndex >= 0 && Slots.IsValidIndex(ActiveSlotIndex))
	{
		return Slots[ActiveSlotIndex];
	}
	return FHotbarSlot();
}

FHotbarSlot UHotbarComponent::GetSlot(int32 Index) const
{
	if (Slots.IsValidIndex(Index))
	{
		return Slots[Index];
	}
	return FHotbarSlot();
}

bool UHotbarComponent::HasItem(FName ItemID) const
{
	for (const FHotbarSlot& Slot : Slots)
	{
		if (Slot.ItemID == ItemID && Slot.Quantity > 0)
		{
			return true;
		}
	}
	return false;
}

void UHotbarComponent::ClearAll()
{
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		Slots[i] = FHotbarSlot();
	}
	ActiveSlotIndex = -1;

	UE_LOG(LogHotbar, Log, TEXT("ClearAll: All slots cleared."));
	OnSlotSelected.Broadcast(-1);
	OnSlotsChanged.Broadcast();
}
