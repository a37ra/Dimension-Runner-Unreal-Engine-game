// CabinOrderWidget.h — Физический экран карточки заказа
// Размещается как Actor с WidgetComponent на базе (L_Laboratory).
// Обновляется реактивно через делегат OnOrderChanged.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DimensionRunnerTypes.h"
#include "CabinOrderWidget.generated.h"

class UTextBlock;
class UGI_DimensionRunner;

/**
 * UCabinOrderWidget — виджет карточки текущего заказа.
 *
 * В Blueprint создай WBP_CabinOrder (Parent = CabinOrderWidget) с:
 *   OrderTitleText   — "ЗАКАЗ" / "НЕТ ЗАКАЗА"
 *   ItemNameText     — "Железный лом"
 *   MoodEmojiText    — "😡" / "😐" / "😊"
 *   PaymentText      — "₪ 80"
 *   TimerText        — "3:05"
 *   DimensionText    — "Δ-05 Лабиринт"
 */
UCLASS(Abstract)
class PHOENIX_API UCabinOrderWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ================= BindWidget =================

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OrderTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MoodEmojiText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PaymentText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TimerText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DimensionText;

public:
	/** Показать карточку заказа */
	UFUNCTION(BlueprintCallable, Category = "CabinOrder")
	void ShowOrder(const FOrderData& Order);

	/** Показать "нет заказа" */
	UFUNCTION(BlueprintCallable, Category = "CabinOrder")
	void ShowNoOrder();

	/** CRT-глитч при смене заказа */
	UFUNCTION(BlueprintCallable, Category = "CabinOrder")
	void PlayGlitchTransition();

	/** Обновить из GI */
	UFUNCTION(BlueprintCallable, Category = "CabinOrder")
	void RefreshFromGI();

private:
	UFUNCTION()
	void OnOrderUpdated(const FOrderData& NewOrder);

	void ApplyOrderData(const FOrderData& Order);

	UGI_DimensionRunner* GetGI() const;

	// CRT-глитч
	bool bGlitching = false;
	float GlitchTimer = 0.0f;
	float GlitchDuration = 0.2f;

	FOrderData PendingOrder;
	bool bHasPendingData = false;
	bool bPendingIsEmpty = false;
};
