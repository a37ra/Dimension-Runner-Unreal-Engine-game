// CarryComponent.h — Физическая переноска предметов + луч + частицы
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CarryComponent.generated.h"

class UPhysicsHandleComponent;
class UCableComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class AArtifactBase;
class USprintStaminaComponent;
class UCameraBobComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PHOENIX_API UCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCarryComponent();

	// ==================== НАСТРОЙКИ ПЕРЕНОСКИ ====================

	/** Расстояние трейса для захвата */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Settings")
	float GrabTraceDistance = 350.0f;

	/** Мин/Макс расстояние удержания от камеры */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Settings")
	float MinHoldDistance = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Settings")
	float MaxHoldDistance = 250.0f;

	/** Стартовое расстояние удержания */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Settings")
	float DefaultHoldDistance = 150.0f;

	/** Шаг изменения расстояния (колёсико) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Settings")
	float DistanceStep = 20.0f;

	/** Сила броска (базовая, делится на вес) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Settings")
	float ThrowForce = 800.0f;

	// ==================== НАСТРОЙКИ ВЕСА ====================

	/** До какого веса предмет считается "лёгким" (множитель = 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Weight")
	float LightWeightThreshold = 2.0f;

	/** Максимальный вес, при котором скорость минимальна */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Weight")
	float MaxWeightForCalc = 100.0f;

	/** Минимальный множитель скорости (при максимальном весе) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Weight")
	float MinSpeedMultiplier = 0.25f;

	/** Мин. множитель чувствительности мыши (при макс. весе) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Weight")
	float MinMouseSensitivity = 0.4f;

	/** Мин. множитель скорости колёсика (при макс. весе) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Weight")
	float MinScrollSpeed = 0.3f;

	// ==================== НАСТРОЙКИ ЛУЧА ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Beam")
	float BeamWidth = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Beam")
	FVector BeamAttachOffset = FVector(30.0f, 15.0f, -20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Beam")
	TObjectPtr<UMaterialInterface> BeamMaterial;

	// ==================== ЧАСТИЦЫ ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|FX")
	TObjectPtr<UNiagaraSystem> BeamEndParticles;

	// ==================== КОМАНДЫ ====================

	UFUNCTION(BlueprintCallable, Category = "Carry")
	void BeginGrab();

	UFUNCTION(BlueprintCallable, Category = "Carry")
	void EndGrab();

	UFUNCTION(BlueprintCallable, Category = "Carry")
	bool TryGrab();

	UFUNCTION(BlueprintCallable, Category = "Carry")
	void Release();

	UFUNCTION(BlueprintCallable, Category = "Carry")
	void Throw();

	UFUNCTION(BlueprintCallable, Category = "Carry")
	void PlaceGently();

	UFUNCTION(BlueprintCallable, Category = "Carry")
	void AdjustDistance(float Delta);

	// ==================== СОСТОЯНИЕ ====================

	UFUNCTION(BlueprintPure, Category = "Carry|State")
	bool IsCarrying() const { return CarriedArtifact != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Carry|State")
	AArtifactBase* GetCarriedArtifact() const { return CarriedArtifact; }

	UFUNCTION(BlueprintPure, Category = "Carry|State")
	AArtifactBase* GetLookedAtArtifact() const { return LookedAtArtifact; }

	/** Текущий множитель скорости от веса (0.25–1.0) */
	UFUNCTION(BlueprintPure, Category = "Carry|State")
	float GetCurrentSpeedMultiplier() const;

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void UpdateLookTrace();
	void UpdateHeldObjectPosition();
	void UpdateBeam();
	void ApplyWeightPenalty();
	void RemoveWeightPenalty();
	void ShowBeam();
	void HideBeam();

	// ==================== GRAB SETTINGS ====================
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Settings", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float GrabHoldTime = 0.5f;

	FTimerHandle GrabTimerHandle;
	void ExecuteGrabTimerObj();

	/** Рассчитать множитель скорости из веса (пропорционально) */
	float CalcSpeedMultiplier(float Weight) const;

	/** Обобщённый расчёт множителя от веса */
	float CalcWeightFactor(float Weight, float MinFactor) const;

	UPROPERTY()
	TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;

	UPROPERTY()
	TObjectPtr<UCableComponent> BeamCable;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> StartFXComp;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> EndFXComp;

	UPROPERTY()
	TObjectPtr<AArtifactBase> CarriedArtifact;

	UPROPERTY()
	TObjectPtr<AArtifactBase> LookedAtArtifact;

	UPROPERTY()
	TObjectPtr<USprintStaminaComponent> StaminaComp;

	UPROPERTY()
	TObjectPtr<UCameraBobComponent> CameraBobComp;

	// Кэш камеры
	UPROPERTY()
	TObjectPtr<APlayerCameraManager> CachedCamManager;

	float CurrentHoldDistance = 150.0f;
};
