// CabinSlotWidget.h — Виджет CRT-экрана слотов кабины
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CabinInventoryComponent.h"
#include "CabinSlotWidget.generated.h"

class UTextBlock;
class UImage;

UCLASS(Abstract)
class PHOENIX_API UCabinSlotWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ================= BindWidget (BP создаёт виджеты с этими именами) =================

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SlotNumberText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemIconImage;

public:
	/** Обновить отображение слота */
	UFUNCTION(BlueprintCallable, Category = "CabinSlots")
	void RefreshSlotDisplay(FCabinSlot InSlot, int32 Index, int32 Total);

	/** Запустить CRT-глитч при переключении */
	UFUNCTION(BlueprintCallable, Category = "CabinSlots")
	void PlayGlitchTransition();

private:
	// Глитч-эффект
	bool bGlitching = false;
	float GlitchTimer = 0.0f;
	float GlitchDuration = 0.15f;

	// Данные для отложенного применения (после глитча)
	FCabinSlot PendingSlot;
	int32 PendingIndex = 0;
	int32 PendingTotal = 0;
	bool bHasPendingData = false;

	void ApplySlotData(FCabinSlot InSlot, int32 Index, int32 Total);
};
