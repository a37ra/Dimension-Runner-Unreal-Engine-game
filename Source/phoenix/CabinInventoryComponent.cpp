// CabinInventoryComponent.cpp — Инвентарь кабины

#include "CabinInventoryComponent.h"
#include "ArtifactBase.h"
#include "Components/PrimitiveComponent.h"

UCabinInventoryComponent::UCabinInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCabinInventoryComponent::Initialize(UPrimitiveComponent* InTrigger)
{
	InventoryTrig = InTrigger;

	Slots.SetNum(StartSlots);
	for (auto& Slot : Slots)
	{
		Slot.Artifact = nullptr;
		Slot.bOccupied = false;
	}

	UE_LOG(LogTemp, Log, TEXT("CabinInventory: Initialized %d slots. Trigger=%s"),
		StartSlots, InventoryTrig ? TEXT("OK") : TEXT("MISSING"));
}

// ==================== КОМАНДЫ ====================

bool UCabinInventoryComponent::LoadItem()
{
	if (!InventoryTrig)
	{
		UE_LOG(LogTemp, Warning, TEXT("CabinInventory: LoadItem — no trigger!"));
		return false;
	}

	// Найти свободный слот
	int32 FreeSlot = INDEX_NONE;
	for (int32 i = 0; i < Slots.Num(); i++)
	{
		if (!Slots[i].bOccupied) { FreeSlot = i; break; }
	}
	if (FreeSlot == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("CabinInventory: All slots occupied!"));
		return false;
	}

	// Overlap-запрос: все артефакты в триггере
	TArray<AActor*> Overlapping;
	InventoryTrig->GetOverlappingActors(Overlapping, AArtifactBase::StaticClass());

	// Исключить уже загруженные
	Overlapping.RemoveAll([this](AActor* Actor)
	{
		for (const FCabinSlot& Slot : Slots)
		{
			if (Slot.bOccupied && Slot.Artifact == Actor) return true;
		}
		return false;
	});

	if (Overlapping.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("CabinInventory: No free artifacts in trigger."));
		return false;
	}

	// Ближайший к центру триггера — загружаем только 1
	FVector Center = InventoryTrig->GetComponentLocation();
	Overlapping.Sort([&Center](const AActor& A, const AActor& B)
	{
		return FVector::DistSquared(A.GetActorLocation(), Center)
			 < FVector::DistSquared(B.GetActorLocation(), Center);
	});

	AArtifactBase* Artifact = Cast<AArtifactBase>(Overlapping[0]);
	if (!Artifact) return false;

	StoreArtifact(FreeSlot, Artifact);

	UE_LOG(LogTemp, Log, TEXT("CabinInventory: Loaded '%s' → slot %d"),
		*Artifact->GetItemName().ToString(), FreeSlot);

	OnInventoryChanged.Broadcast();
	return true;
}

void UCabinInventoryComponent::SelectNextSlot()
{
	if (Slots.Num() == 0) return;
	ActiveSlotIndex = (ActiveSlotIndex + 1) % Slots.Num();

	UE_LOG(LogTemp, Log, TEXT("CabinInventory: Slot %d/%d"),
		ActiveSlotIndex + 1, Slots.Num());

	OnInventoryChanged.Broadcast();
}

bool UCabinInventoryComponent::UnloadItem()
{
	if (!Slots.IsValidIndex(ActiveSlotIndex) || !Slots[ActiveSlotIndex].bOccupied)
	{
		UE_LOG(LogTemp, Log, TEXT("CabinInventory: Slot %d empty."), ActiveSlotIndex);
		return false;
	}

	AArtifactBase* Artifact = Slots[ActiveSlotIndex].Artifact;
	UE_LOG(LogTemp, Log, TEXT("CabinInventory: Unloading '%s' from slot %d"),
		Artifact ? *Artifact->GetItemName().ToString() : TEXT("null"), ActiveSlotIndex);

	RestoreArtifact(ActiveSlotIndex);
	OnInventoryChanged.Broadcast();
	return true;
}

// ==================== СОСТОЯНИЕ ====================

FCabinSlot UCabinInventoryComponent::GetSlotInfo(int32 Index) const
{
	if (Slots.IsValidIndex(Index)) return Slots[Index];
	return FCabinSlot();
}

int32 UCabinInventoryComponent::GetOccupiedCount() const
{
	int32 Count = 0;
	for (const FCabinSlot& Slot : Slots)
	{
		if (Slot.bOccupied) Count++;
	}
	return Count;
}

// ==================== ВНУТРЕННИЕ ====================

void UCabinInventoryComponent::StoreArtifact(int32 SlotIndex, AArtifactBase* Artifact)
{
	if (!Slots.IsValidIndex(SlotIndex) || !Artifact) return;

	Slots[SlotIndex].Artifact = Artifact;
	Slots[SlotIndex].bOccupied = true;

	// Скрываем предмет и отключаем физику
	Artifact->SetActorHiddenInGame(true);
	Artifact->SetActorEnableCollision(false);

	if (UPrimitiveComponent* Mesh = Artifact->GetGrabbableComponent())
	{
		Mesh->SetSimulatePhysics(false);
	}
}

void UCabinInventoryComponent::RestoreArtifact(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || !Slots[SlotIndex].bOccupied) return;

	AArtifactBase* Artifact = Slots[SlotIndex].Artifact;
	if (!Artifact) return;

	// Телепортируем в кабину
	FVector SpawnLoc = GetOwner()->GetActorLocation()
		+ GetOwner()->GetActorRotation().RotateVector(UnloadOffset);

	// Настраиваем коллизию до перемещения
	Artifact->SetActorHiddenInGame(false);
	Artifact->SetActorEnableCollision(true);

	// Используем TeleportPhysics, чтобы физический движок корректно обновил положение без применения гигантских сил из-за возможных пересечений
	Artifact->SetActorLocationAndRotation(SpawnLoc, GetOwner()->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);

	if (UPrimitiveComponent* Mesh = Artifact->GetGrabbableComponent())
	{
		Mesh->SetSimulatePhysics(true);
		// Сбрасываем скорости, чтобы предмет не улетел при спавне
		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Mesh->WakeAllRigidBodies();
	}

	Slots[SlotIndex].Artifact = nullptr;
	Slots[SlotIndex].bOccupied = false;
}
