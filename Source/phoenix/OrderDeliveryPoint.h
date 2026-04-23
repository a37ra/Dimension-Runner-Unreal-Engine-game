// OrderDeliveryPoint.h — Точка сдачи заказа (физическая "дырка")
// Игрок кидает артефакт в триггер-зону → проверка ItemID → CompleteOrder.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrderDeliveryPoint.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class AArtifactBase;

/**
 * AOrderDeliveryPoint — актор на базе игрока.
 * 
 * Игрок несёт артефакт → кидает/кладёт в дырку → Overlap → CompleteOrder.
 * 
 * В Blueprint:
 *   1. Создай BP от OrderDeliveryPoint
 *   2. Настрой меш (визуал контейнера/дырки)
 *   3. Подстрой размер DeliveryTrigger
 *   4. Реализуй OnDeliveryComplete для VFX/звуков
 */
UCLASS(Blueprintable)
class PHOENIX_API AOrderDeliveryPoint : public AActor
{
	GENERATED_BODY()

public:
	AOrderDeliveryPoint();

	// ==================== КОМПОНЕНТЫ ====================

	/** Визуал контейнера/дырки (назначь меш в BP) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	/** Триггер-зона для приёма артефактов */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery")
	TObjectPtr<UBoxComponent> DeliveryTrigger;

	// ==================== НАСТРОЙКИ ====================

	/** Уничтожить артефакт после успешной сдачи? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery|Settings")
	bool bDestroyArtifactOnDelivery = true;

	/** Задержка перед уничтожением артефакта (для VFX) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery|Settings")
	float DestroyDelay = 0.5f;

	// ==================== СОБЫТИЯ ДЛЯ BLUEPRINT ====================

	/** Заказ выполнен — артефакт принят */
	UFUNCTION(BlueprintImplementableEvent, Category = "Delivery")
	void OnDeliveryComplete(int32 Payout, bool bCorrectItem, const FName& DeliveredItemID);

	/** Нет активного заказа — артефакт отклонён */
	UFUNCTION(BlueprintImplementableEvent, Category = "Delivery")
	void OnDeliveryRejected();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	/** Обрабатываем артефакт */
	void ProcessArtifact(AArtifactBase* Artifact);
};
