// HotbarComponent.cpp
#include "HotbarComponent.h"
#include "ArtifactBase.h"
#include "GI_DimensionRunner.h"
#include "HealthComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "InputCoreTypes.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHotbar, Log, All);

UHotbarComponent::UHotbarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHotbarComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.SetNum(NumSlots);
	for (int32 i = 0; i < NumSlots; ++i)
		Slots[i] = FHotbarSlot();

	UE_LOG(LogHotbar, Log, TEXT("HotbarComponent: Initialized with %d slots."), NumSlots);
	SetupInputBindings();
}

void UHotbarComponent::SetupInputBindings()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	UInputComponent* InputComp = OwnerPawn->InputComponent;
	if (!InputComp)
	{
		// InputComponent ещё не готов — повторим на следующем тике
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UHotbarComponent::SetupInputBindings);
		return;
	}

	// Защита от двойного биндинга
	InputComp->RemoveActionBinding(TEXT("One"),         IE_Pressed);
	InputComp->RemoveActionBinding(TEXT("Two"),         IE_Pressed);
	InputComp->RemoveActionBinding(TEXT("Drop"),        IE_Pressed);
	InputComp->RemoveActionBinding(TEXT("ScrollUp"),    IE_Pressed);
	InputComp->RemoveActionBinding(TEXT("ScrollDown"),  IE_Pressed);
	InputComp->RemoveActionBinding(TEXT("Use"),         IE_Pressed);

	// Цифровые клавиши
	InputComp->BindKey(EKeys::One,              IE_Pressed, this, &UHotbarComponent::OnHotbar1Pressed);
	InputComp->BindKey(EKeys::Two,              IE_Pressed, this, &UHotbarComponent::OnHotbar2Pressed);

	// Скролл мыши
	InputComp->BindKey(EKeys::MouseScrollUp,    IE_Pressed, this, &UHotbarComponent::OnScrollUp);
	InputComp->BindKey(EKeys::MouseScrollDown,  IE_Pressed, this, &UHotbarComponent::OnScrollDown);

	// Выброс предмета (Q)
	InputComp->BindKey(EKeys::Q,                IE_Pressed, this, &UHotbarComponent::OnDropPressed);
	InputComp->BindKey(EKeys::G,                IE_Pressed, this, &UHotbarComponent::OnUsePressed);

	UE_LOG(LogHotbar, Log, TEXT("HotbarComponent: Keys bound (1-2, Scroll, Q=Drop, G=Use)."));
}

// ============================================================
// ВЫБОР СЛОТА
// ============================================================

void UHotbarComponent::SelectSlot(int32 Index)
{
	if (Index < -1 || Index >= Slots.Num())
	{
		UE_LOG(LogHotbar, Warning, TEXT("SelectSlot: Invalid index %d"), Index);
		return;
	}

	if (ActiveSlotIndex == Index)
	{
		ActiveSlotIndex = -1;
		UE_LOG(LogHotbar, Log, TEXT("Slot deselected."));
	}
	else
	{
		ActiveSlotIndex = Index;
		UE_LOG(LogHotbar, Log, TEXT("Slot %d selected. Item: '%s' x%d"),
			Index, *Slots[Index].ItemID.ToString(), Slots[Index].Quantity);

		PlaySoundAtOwner(SlotSelectSound);
	}

	OnSlotSelected.Broadcast(ActiveSlotIndex);
}

void UHotbarComponent::ScrollNext()
{
	if (Slots.Num() == 0) return;

	// Если ничего не выбрано — начинаем с 0
	const int32 NextIndex = (ActiveSlotIndex < 0) ? 0 : (ActiveSlotIndex + 1) % Slots.Num();
	SelectSlot(NextIndex);
}

void UHotbarComponent::ScrollPrev()
{
	if (Slots.Num() == 0) return;

	const int32 PrevIndex = (ActiveSlotIndex <= 0) ? (Slots.Num() - 1) : (ActiveSlotIndex - 1);
	SelectSlot(PrevIndex);
}

// ============================================================
// ДОБАВЛЕНИЕ / УДАЛЕНИЕ
// ============================================================

bool UHotbarComponent::AddItem(FName ItemID, int32 Quantity)
{
	if (ItemID.IsNone() || Quantity <= 0) return false;

	FHotbarSlot NewSlotData;
	BuildSlotDataFromItemID(ItemID, NewSlotData);
	NewSlotData.Quantity = Quantity;

	// Стакаем в существующий слот
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

	// В пустой слот
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].IsEmpty())
		{
			Slots[i] = NewSlotData;
			UE_LOG(LogHotbar, Log, TEXT("AddItem: '%s' x%d → slot %d"), *ItemID.ToString(), Quantity, i);
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

	FHotbarSlot SlotData;
	BuildSlotDataFromItemID(ItemID, SlotData);
	SlotData.Quantity = Quantity;
	Slots[SlotIndex] = SlotData;
	UE_LOG(LogHotbar, Log, TEXT("SetSlot: Slot %d = '%s' x%d"), SlotIndex, *ItemID.ToString(), Quantity);
	OnSlotsChanged.Broadcast();
}

void UHotbarComponent::RemoveItem(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return;

	UE_LOG(LogHotbar, Log, TEXT("RemoveItem: Cleared slot %d (was '%s')"),
		SlotIndex, *Slots[SlotIndex].ItemID.ToString());

	Slots[SlotIndex] = FHotbarSlot();

	if (ActiveSlotIndex == SlotIndex)
	{
		ActiveSlotIndex = -1;
		OnSlotSelected.Broadcast(-1);
	}

	OnSlotsChanged.Broadcast();
}

// ============================================================
// ВЫБРОС ПРЕДМЕТА
// ============================================================

void UHotbarComponent::DropActiveItem()
{
	if (ActiveSlotIndex < 0 || !Slots.IsValidIndex(ActiveSlotIndex)) return;

	FHotbarSlot& Slot = Slots[ActiveSlotIndex];
	if (Slot.IsEmpty()) return;

	const FName DroppedID = Slot.ItemID;
	UE_LOG(LogHotbar, Log, TEXT("DropActiveItem: Dropping '%s' from slot %d"), *DroppedID.ToString(), ActiveSlotIndex);

	// Пытаемся заспawnить актор предмета в мир
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (OwnerChar && GetWorld())
	{
		const FVector Forward  = OwnerChar->GetActorForwardVector();
		const FVector SpawnPos = OwnerChar->GetActorLocation()
		                       + Forward * DropForwardDistance
		                       + FVector(0.f, 0.f, 20.f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		// Ищем класс предмета через Asset Registry / просто broadcast-им делегат
		// чтобы Blueprint/GI обработал создание актора
		// (если у тебя есть AArtifactBase — здесь можно сделать spawn)
	}

	// Звук выброса
	PlaySoundAtOwner(ItemDropSound);

	// Очищаем слот и уведомляем
	Slot = FHotbarSlot();
	OnItemDropped.Broadcast(DroppedID);

	if (ActiveSlotIndex >= 0)
	{
		ActiveSlotIndex = -1;
		OnSlotSelected.Broadcast(-1);
	}

	OnSlotsChanged.Broadcast();
}

// ============================================================
// ИСПОЛЬЗОВАНИЕ ПРЕДМЕТА
// ============================================================

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

	if (!Slot.bCanUseFromHotbar)
	{
		UE_LOG(LogHotbar, Warning, TEXT("UseActiveItem: Item '%s' is not usable from hotbar."), *Slot.ItemID.ToString());
		return;
	}

	const FName UsedID = Slot.ItemID;
	const FItemData* ItemData = FindItemData(UsedID);
	bool bAppliedEffect = false;

	if (ItemData && ItemData->HealAmount > 0.0f)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			if (UHealthComponent* HealthComp = OwnerActor->FindComponentByClass<UHealthComponent>())
			{
				HealthComp->Heal(ItemData->HealAmount);
				bAppliedEffect = true;
			}
			else
			{
				UE_LOG(LogHotbar, Warning, TEXT("UseActiveItem: No HealthComponent on owner to use '%s'."), *UsedID.ToString());
				return;
			}
		}
	}
	else
	{
		bAppliedEffect = true;
	}

	if (!bAppliedEffect)
	{
		return;
	}

	if (!ConsumeItemInSlot(ActiveSlotIndex, 1))
	{
		return;
	}

	const int32 RemainingQty = Slots.IsValidIndex(ActiveSlotIndex) ? Slots[ActiveSlotIndex].Quantity : 0;
	UE_LOG(LogHotbar, Log, TEXT("UseActiveItem: Used '%s'. Remaining: %d"), *UsedID.ToString(), RemainingQty);
	OnItemUsed.Broadcast(UsedID, RemainingQty);
}

// ============================================================
// ГЕТТЕРЫ
// ============================================================

FHotbarSlot UHotbarComponent::GetActiveSlot() const
{
	if (ActiveSlotIndex >= 0 && Slots.IsValidIndex(ActiveSlotIndex))
		return Slots[ActiveSlotIndex];
	return FHotbarSlot();
}

FHotbarSlot UHotbarComponent::GetSlot(int32 Index) const
{
	if (Slots.IsValidIndex(Index))
		return Slots[Index];
	return FHotbarSlot();
}

bool UHotbarComponent::HasItem(FName ItemID) const
{
	for (const FHotbarSlot& Slot : Slots)
	{
		if (Slot.ItemID == ItemID && Slot.Quantity > 0)
			return true;
	}
	return false;
}

bool UHotbarComponent::HasUsableActiveItem() const
{
	if (ActiveSlotIndex < 0 || !Slots.IsValidIndex(ActiveSlotIndex))
	{
		return false;
	}

	const FHotbarSlot& Slot = Slots[ActiveSlotIndex];
	return !Slot.IsEmpty() && Slot.bCanUseFromHotbar;
}

FText UHotbarComponent::GetActiveUsePrompt() const
{
	return HasUsableActiveItem()
		? FText::FromString(TEXT("G - использовать предмет"))
		: FText::GetEmpty();
}

void UHotbarComponent::ClearAll()
{
	for (int32 i = 0; i < Slots.Num(); ++i)
		Slots[i] = FHotbarSlot();

	ActiveSlotIndex = -1;
	UE_LOG(LogHotbar, Log, TEXT("ClearAll: All slots cleared."));
	OnSlotSelected.Broadcast(-1);
	OnSlotsChanged.Broadcast();
}

// ============================================================
// ЗВУКИ
// ============================================================

void UHotbarComponent::PlayPickupSound()
{
	PlaySoundAtOwner(ItemPickupSound);
}

bool UHotbarComponent::ConsumeItemInSlot(int32 SlotIndex, int32 Quantity)
{
	if (!Slots.IsValidIndex(SlotIndex) || Quantity <= 0)
	{
		return false;
	}

	FHotbarSlot& Slot = Slots[SlotIndex];
	if (Slot.IsEmpty() || Slot.Quantity < Quantity)
	{
		return false;
	}

	Slot.Quantity -= Quantity;
	if (Slot.Quantity <= 0)
	{
		Slot = FHotbarSlot();

		if (ActiveSlotIndex == SlotIndex)
		{
			ActiveSlotIndex = -1;
			OnSlotSelected.Broadcast(-1);
		}
	}
	else if (ActiveSlotIndex == SlotIndex)
	{
		OnSlotSelected.Broadcast(ActiveSlotIndex);
	}

	OnSlotsChanged.Broadcast();
	return true;
}

bool UHotbarComponent::CanPickupArtifactToHotbar(const AArtifactBase* Artifact) const
{
	if (!Artifact)
	{
		return false;
	}

	if (const FItemData* ItemData = FindItemData(Artifact->GetItemID()))
	{
		return ItemData->bCanPickupToHotbar;
	}

	return false;
}

bool UHotbarComponent::TryPickupArtifact(AArtifactBase* Artifact)
{
	if (!Artifact)
	{
		return false;
	}

	const FItemData* ItemData = FindItemData(Artifact->GetItemID());
	if (!ItemData || !ItemData->bCanPickupToHotbar)
	{
		return false;
	}

	if (!AddItem(Artifact->GetItemID(), 1))
	{
		UE_LOG(LogHotbar, Warning, TEXT("TryPickupArtifact: Hotbar full for '%s'."), *Artifact->GetItemID().ToString());
		return false;
	}

	PlayPickupSound();

	Artifact->SetActorHiddenInGame(true);
	Artifact->SetActorEnableCollision(false);
	Artifact->bIsHeld = false;

	if (UPrimitiveComponent* MeshComp = Artifact->GetGrabbableComponent())
	{
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Artifact->Destroy();
	return true;
}

const FItemData* UHotbarComponent::FindItemData(FName ItemID) const
{
	if (const UGI_DimensionRunner* GI = Cast<UGI_DimensionRunner>(GetWorld() ? GetWorld()->GetGameInstance() : nullptr))
	{
		return GI->FindItemData(ItemID);
	}

	return nullptr;
}

bool UHotbarComponent::BuildSlotDataFromItemID(FName ItemID, FHotbarSlot& OutSlotData) const
{
	OutSlotData = FHotbarSlot();
	OutSlotData.ItemID = ItemID;

	if (const FItemData* ItemData = FindItemData(ItemID))
	{
		OutSlotData.ItemIcon = ItemData->ItemIcon;
		OutSlotData.ItemType = ItemData->ItemType;
		OutSlotData.bCanUseFromHotbar = ItemData->bCanUseFromHotbar;
		return true;
	}

	return false;
}

void UHotbarComponent::PlaySoundAtOwner(USoundBase* Sound) const
{
	if (!Sound) return;
	AActor* Owner = GetOwner();
	if (!Owner) return;
	UGameplayStatics::PlaySoundAtLocation(Owner, Sound, Owner->GetActorLocation());
}
