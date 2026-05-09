// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HotbarWidget.generated.h"

class UHotbarComponent;
class UTexture2D;

UCLASS(Abstract)
class PHOENIX_API UHotbarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// ================= Ивенты для Blueprint =================

	/** Вызывается, когда нужно отрисовать предмет в слоте */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hotbar")
	void BP_UpdateSlot(int32 SlotIndex, FName ItemID, int32 Quantity, UTexture2D* ItemIcon);

	/** Вызывается для визуального выделения активного слота */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hotbar")
	void BP_HighlightSlot(int32 SlotIndex);

	/** Показывает или прячет подсказку использования предмета. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hotbar")
	void BP_SetUsePrompt(bool bVisible, const FText& PromptText);

	// ================= Функции обработки =================

	UFUNCTION()
	void OnSlotSelectedHandler(int32 SlotIndex);

	UFUNCTION()
	void OnSlotsChangedHandler();

	void RefreshUsePrompt();

private:
	TWeakObjectPtr<UHotbarComponent> HotbarComp;
};
