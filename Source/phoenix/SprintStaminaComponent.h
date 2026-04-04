// SprintStaminaComponent.h — Спринт на Shift + стамина
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SprintStaminaComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PHOENIX_API USprintStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USprintStaminaComponent();

	// ==================== СКОРОСТЬ ====================

	/** Обычная скорость ходьбы */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint|Speed")
	float WalkSpeed = 300.0f;

	/** Скорость бега */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint|Speed")
	float SprintSpeed = 600.0f;

	// ==================== СТАМИНА ====================

	/** Максимальная стамина */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint|Stamina")
	float MaxStamina = 100.0f;

	/** Скорость расхода стамины (единиц в секунду) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint|Stamina")
	float StaminaDrainRate = 20.0f;

	/** Скорость восстановления стамины (единиц в секунду) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint|Stamina")
	float StaminaRegenRate = 15.0f;

	/** Задержка перед началом восстановления (секунды) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint|Stamina")
	float RegenDelay = 1.0f;

	/** Минимальная стамина для начала спринта (защита от мигания) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint|Stamina")
	float MinStaminaToSprint = 15.0f;

	// ==================== СОСТОЯНИЕ (для HUD) ====================

	/** Текущая стамина (0 — MaxStamina). Читай из HUD! */
	UPROPERTY(BlueprintReadOnly, Category = "Sprint|State")
	float CurrentStamina;

	/** Текущая стамина в процентах (0.0 — 1.0). Читай из HUD! */
	UPROPERTY(BlueprintReadOnly, Category = "Sprint|State")
	float StaminaPercent = 1.0f;

	/** Сейчас бежит? */
	UPROPERTY(BlueprintReadOnly, Category = "Sprint|State")
	bool bIsSprinting = false;

	/** Внешний множитель скорости (от веса предмета, дебаффов и т.д.). 1.0 = норма */
	UPROPERTY(BlueprintReadOnly, Category = "Sprint|State")
	float ExternalSpeedMultiplier = 1.0f;

	/** Установить множитель скорости (вызывается из CarryComponent и т.д.) */
	UFUNCTION(BlueprintCallable, Category = "Sprint|Speed")
	void SetExternalSpeedMultiplier(float Multiplier) { ExternalSpeedMultiplier = FMath::Clamp(Multiplier, 0.1f, 1.0f); }

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Получить время, прошедшее с момента остановки бега */
	UFUNCTION(BlueprintCallable, Category = "Sprint|State")
	float GetTimeSinceStoppedSprinting() const { return TimeSinceStoppedSprinting; }

private:
	UPROPERTY()
	TObjectPtr<class UCharacterMovementComponent> MoveComp;

	float TimeSinceStoppedSprinting = 0.0f;
	bool bShiftHeld = false;
	bool bStaminaDepleted = false;
};
