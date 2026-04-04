// ArtifactBase.h — Базовый класс подбираемого артефакта
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArtifactBase.generated.h"

/**
 * AArtifactBase — предмет, который можно нести, бросать, загружать в кабину.
 * Наследуй в Blueprint для конкретных артефактов (BP_Artifact_Tesseract и т.д.)
 */
UCLASS(Blueprintable)
class PHOENIX_API AArtifactBase : public AActor
{
	GENERATED_BODY()

public:
	AArtifactBase();

	// ==================== ДАННЫЕ АРТЕФАКТА ====================

	/** Название предмета (для заказов и UI) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
	FName ItemName = TEXT("Unknown Artifact");

	/** Описание (для документов/заметок) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
	FText Description;

	/** Вес предмета. >5 = тяжёлый (замедляет сильнее) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
	float Weight = 1.0f;

	/** Хрупкий? Получает урон от бросков и падений */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
	bool bIsFragile = false;

	/** Тяжёлый? (в соло нужна тележка/крюк/спрей) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
	bool bIsHeavy = false;

	/** Текущее HP предмета (хрупкие ломаются при 0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
	float ItemHP = 100.0f;

	/** Максимальное HP */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
	float MaxItemHP = 100.0f;

	// ==================== ГЕТТЕРЫ ====================

	UFUNCTION(BlueprintPure, Category = "Artifact")
	FName GetItemName() const { return ItemName; }

	UFUNCTION(BlueprintPure, Category = "Artifact")
	float GetWeight() const { return Weight; }

	UFUNCTION(BlueprintPure, Category = "Artifact")
	bool IsHeavy() const { return bIsHeavy; }

	UFUNCTION(BlueprintPure, Category = "Artifact")
	bool IsFragile() const { return bIsFragile; }

	UFUNCTION(BlueprintPure, Category = "Artifact")
	bool IsBroken() const { return bIsFragile && ItemHP <= 0.0f; }

	/** Текст подсказки для UI ("E: Нести Тессеракт") */
	UFUNCTION(BlueprintPure, Category = "Artifact")
	FText GetInteractHintText() const;

	/** Компонент, за который физически хватаем */
	UFUNCTION(BlueprintCallable, Category = "Artifact")
	UPrimitiveComponent* GetGrabbableComponent() const { return MeshComponent; }

	// ==================== СОБЫТИЯ ====================

	/** Вызывается когда предмет бросили (может получить урон при столкновении) */
	void OnThrown();

	/** Вызывается когда предмет аккуратно положили (без урона) */
	void OnPlacedGently();

	/** Событие в Blueprint — предмет сломался */
	UFUNCTION(BlueprintImplementableEvent, Category = "Artifact")
	void OnBroken();

	/** Событие в Blueprint — предмет подняли */
	UFUNCTION(BlueprintImplementableEvent, Category = "Artifact")
	void OnPickedUp();

	/** Событие в Blueprint — предмет бросили */
	UFUNCTION(BlueprintImplementableEvent, Category = "Artifact")
	void OnThrownEvent();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Artifact|Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

private:
	/** Обработка столкновений (урон хрупким предметам) */
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	bool bWasThrown = false;
	bool bPlacedGently = false;
};
