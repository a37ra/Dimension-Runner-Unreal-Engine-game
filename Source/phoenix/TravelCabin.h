// TravelCabin.h — C++ замена EventGraph для BP_TravelCabin
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "TravelCabin.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class UCurveFloat;
class USoundBase;
class UTextBlock;

/**
 * Состояние кабины
 */
UENUM(BlueprintType)
enum class ECabinState : uint8
{
	Ready       UMETA(DisplayName = "Готова"),
	Launching   UMETA(DisplayName = "Запуск"),
	Expedition  UMETA(DisplayName = "Экспедиция"),
	Returning   UMETA(DisplayName = "Возврат"),
	Overheat    UMETA(DisplayName = "Перегрев")
};

/**
 * ATravelCabin — логика кабины телепортации.
 * 
 * Используется как Parent Class для BP_TravelCabin.
 * Все компоненты (Door_L, Door_R, ButtonMesh, ScreenWidget, Trigger_Inside)
 * находятся В БЛЮПРИНТЕ — C++ находит их по имени при BeginPlay.
 */
UCLASS(Blueprintable)
class PHOENIX_API ATravelCabin : public AActor
{
	GENERATED_BODY()

public:
	ATravelCabin();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ==================== НАСТРОЙКИ ====================

	/** Время экспедиции (3:05 = 185 секунд) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	int32 ExpeditionTime = 185;

	/** Время перезагрузки/перегрева (2:00 = 120 секунд) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	int32 CooldownTime = 120;

	/** Уровень назначения (L_WorldA на базе, L_Laboratory на миссии) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	FName TargetLevelName;

	/** Задержка перед телепортом после закрытия дверей */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	float TeleportDelay = 3.0f;

	/** Задержка перед открытием дверей при загрузке уровня */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	float DoorOpenDelay = 2.0f;

	/** Кривая анимации дверей (Float Curve, 0→1 за ~1 сек) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	TObjectPtr<UCurveFloat> DoorAnimCurve;

	/** Звук сирены при 15 секундах */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	TObjectPtr<USoundBase> SirenSound;

	// ==================== ПОЗИЦИИ ДВЕРЕЙ ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Doors")
	FVector DoorLClosedPos = FVector(-75.0f, 150.0f, 130.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Doors")
	FVector DoorLOpenPos = FVector(-225.0f, 150.0f, 130.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Doors")
	FVector DoorRClosedPos = FVector(76.0f, 150.0f, 130.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Doors")
	FVector DoorROpenPos = FVector(224.0f, 150.0f, 130.0f);

	// Состояние (без UPROPERTY чтобы не конфликтовать с BP переменными)
	ECabinState CabinState = ECabinState::Ready;
	bool bIsPlayerInside = false;
	bool bIsActivated = false;
	int32 CurrentTime = 0;

	// ==================== ФУНКЦИИ ДЛЯ БЛЮПРИНТА ====================

	/** Вызывай из Event Interact в Blueprint */
	UFUNCTION(BlueprintCallable, Category = "Cabin")
	void OnInteract(AActor* Interactor);

	/** Вызывается когда игрок не успел вернуться (реализуй в BP для эффектов смерти) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cabin")
	void OnPlayerLeftBehind();

	/** Вызывается при смене состояния (для кастомных эффектов в BP) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cabin")
	void OnCabinStateChanged(ECabinState NewState);

protected:
	// Найдены автоматически из BP по имени
	TObjectPtr<UStaticMeshComponent> DoorL;
	TObjectPtr<UStaticMeshComponent> DoorR;

	TObjectPtr<UStaticMeshComponent> CachedButton;

	TObjectPtr<UPrimitiveComponent> CachedTrigger;

	TObjectPtr<UPrimitiveComponent> CachedBlocker;

	TObjectPtr<UWidgetComponent> CachedScreen;

	// ==================== ВНУТРЕННИЕ ====================

	FTimeline DoorTimeline;
	FTimerHandle CabinTimerHandle;

	// Что делать после закрытия дверей
	enum class EPendingAction : uint8
	{
		None,
		TeleportOut,       // Запуск: летим на миссию
		TeleportHome,      // Возврат: летим домой с лутом
		PlayerAbandoned    // Игрок не успел
	};
	EPendingAction PendingAction = EPendingAction::None;

	// Находит компоненты BP по имени
	void FindBlueprintComponents();

	// Настраивает FTimeline для дверей
	void SetupDoorTimeline();

	// Таймер: вызывается каждую секунду
	void TickCabinTimer();

	// Анимация дверей
	UFUNCTION()
	void OnDoorTimelineUpdate(float Alpha);
	UFUNCTION()
	void OnDoorTimelineFinished();

	void OpenDoors();
	void CloseDoors();

	// Overlap триггера
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// Экран кабины
	UTextBlock* GetTimerTextWidget() const;
	void UpdateScreenText(int32 Seconds);
	void SetScreenText(const FString& Text);
	void SetTimerTextColor(const FLinearColor& Color);

	// Таймер
	void StartCabinTimer();
	void StopCabinTimer();

	// Кнопка
	void EnableButton();
	void DisableButton();

	// Смена состояния
	void SetCabinState(ECabinState NewState);

	// GI_Phoenix (через Reflection)
	UObject* GetGIPhoenix() const;
	bool GI_CheckCabinStatus();
	bool GI_CheckOverheatStatus();
	void GI_SetCabinStatus(bool bActive);
	void GI_SetOverheatStatus(bool bOverheated);
	void SavePlayerInventoryToGI();

	// Телепорт
	void PerformTeleport();
};
