// CabinStatusWidget.h — Физический экран рейтинга (6-секционный бар)
// Размещается как Actor с WidgetComponent на базе (L_Laboratory).
// Обновляется реактивно через делегаты GI_DimensionRunner.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DimensionRunnerTypes.h"
#include "CabinStatusWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UGI_DimensionRunner;

/**
 * UCabinStatusWidget — виджет для физического экрана статуса.
 *
 * В Blueprint создай WBP_CabinStatus (Parent = CabinStatusWidget) с:
 *   6× ProgressBar: RatingBar_1 ... RatingBar_6
 *   6× TextBlock:   EmojiText_1 ... EmojiText_6
 *   1× TextBlock:   CreditsText
 *   1× TextBlock:   DayText
 *   1× TextBlock:   SectionText
 */
UCLASS(Abstract)
class PHOENIX_API UCabinStatusWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ================= BindWidget =================

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> RatingBar_1;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> RatingBar_2;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> RatingBar_3;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> RatingBar_4;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> RatingBar_5;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> RatingBar_6;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmojiText_1;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmojiText_2;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmojiText_3;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmojiText_4;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmojiText_5;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmojiText_6;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CreditsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DayText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SectionText;

public:
	/** Обновить всё из GI */
	UFUNCTION(BlueprintCallable, Category = "CabinStatus")
	void RefreshFromGI();

private:
	// Делегаты
	UFUNCTION()
	void OnRatingUpdated(float NewRating);
	UFUNCTION()
	void OnCreditsUpdated(int32 NewCredits);
	UFUNCTION()
	void OnDayUpdated(int32 NewDay);

	void UpdateRatingBar(float Rating);
	void UpdateCredits(int32 Credits);
	void UpdateDay(int32 Day);

	UGI_DimensionRunner* GetGI() const;

	// Цвета секций
	static FLinearColor GetSectionColor(int32 Section, bool bActive);

	// Текстовые метки секций (вместо emoji)
	static FString GetSectionLabel(ERatingSection Section);
};
