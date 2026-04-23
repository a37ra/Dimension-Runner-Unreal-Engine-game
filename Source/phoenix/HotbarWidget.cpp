// Fill out your copyright notice in the Description page of Project Settings.

#include "HotbarWidget.h"
#include "HotbarComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

void UHotbarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			HotbarComp = Pawn->FindComponentByClass<UHotbarComponent>();
		}
	}

	if (!HotbarComp.IsValid())
	{
		// Фоллбэк
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			HotbarComp = Pawn->FindComponentByClass<UHotbarComponent>();
		}
	}

	if (HotbarComp.IsValid())
	{
		HotbarComp->OnSlotSelected.AddDynamic(this, &UHotbarWidget::OnSlotSelectedHandler);
		HotbarComp->OnSlotsChanged.AddDynamic(this, &UHotbarWidget::OnSlotsChangedHandler);

		// Инициализируем отрисовку сразу
		OnSlotsChangedHandler();
		OnSlotSelectedHandler(HotbarComp->ActiveSlotIndex);
	}
}

void UHotbarWidget::OnSlotSelectedHandler(int32 SlotIndex)
{
	BP_HighlightSlot(SlotIndex);
}

void UHotbarWidget::OnSlotsChangedHandler()
{
	if (!HotbarComp.IsValid()) return;

	for (int32 i = 0; i < HotbarComp->NumSlots; ++i)
	{
		FHotbarSlot CurrentHotbarSlot = HotbarComp->GetSlot(i);
		BP_UpdateSlot(i, CurrentHotbarSlot.ItemID, CurrentHotbarSlot.Quantity);
	}
}
