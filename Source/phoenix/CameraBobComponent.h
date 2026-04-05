// CameraBobComponent.h — Juicy FPS Camera (покачивание, наклоны, дыхание, FOV)
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CameraBobComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PHOENIX_API UCameraBobComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCameraBobComponent();

	// ==================== ПОКАЧИВАНИЕ ПРИ ХОДЬБЕ ====================

	/** Частота шагов (чем больше — тем быстрее качается) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Walk Bob")
	float BobFrequency = 7.0f;

	/** Амплитуда покачивания вверх-вниз (см) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Walk Bob")
	float BobVertical = 2.0f;

	/** Амплитуда покачивания влево-вправо (см) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Walk Bob")
	float BobHorizontal = 1.0f;

	/** Лёгкий наклон (Roll) при каждом шаге (градусы) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Walk Bob")
	float BobRoll = 0.3f;

	// ==================== БЕГ ====================

	/** Множитель покачивания при беге */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Sprint")
	float SprintBobMultiplier = 1.6f;

	/** Порог скорости для бега */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Sprint")
	float SprintThreshold = 450.0f;

	/** FOV при беге (обычный FOV берётся автоматически) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Sprint")
	float SprintFOVBoost = 5.0f;

	// ==================== НАКЛОНЫ (TILT) ====================

	/** Наклон камеры при стрейфе (градусы) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Tilt")
	float StrafeTilt = 1.2f;

	/** Наклон камеры при резком повороте мыши (градусы) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Tilt")
	float TurnTilt = 0.6f;

	// ==================== ДЫХАНИЕ В ПОКОЕ ====================

	/** Скорость дыхания */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Idle")
	float BreathSpeed = 1.2f;

	/** Амплитуда дыхания (см) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Idle")
	float BreathAmplitude = 0.3f;

	// ==================== ПРИЗЕМЛЕНИЕ ====================

	/** Просадка камеры при приземлении (см) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Landing")
	float LandingDip = 6.0f;

	/** Скорость восстановления после просадки */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|Landing")
	float LandingRecovery = 8.0f;

	// ==================== ОБЩЕЕ ====================

	/** Общая скорость интерполяции (плавность переходов) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juicy Camera|General")
	float Smoothing = 12.0f;

	// ==================== ВЕС → КАМЕРА ====================

	/** Установить множитель чувствительности от веса (0.3–1.0) */
	UFUNCTION(BlueprintCallable, Category = "Juicy Camera|Weight")
	void SetWeightSensitivity(float Multiplier);

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<class UCameraComponent> CameraComp;

	FVector BaseLocation;       // Начальная позиция камеры
	float BaseFOV;              // Начальный FOV

	float BobTimer = 0.0f;      // Таймер покачивания
	float IdleTimer = 0.0f;     // Таймер дыхания

	float CurrentRoll = 0.0f;   // Текущий наклон (Roll)
	float CurrentDip = 0.0f;    // Текущая просадка от приземления
	float CurrentFOVAdd = 0.0f; // Текущая добавка к FOV

	float PrevYaw = 0.0f;       // Yaw предыдущего кадра (для расчёта скорости поворота)
	bool bWasInAir = false;     // Был ли в воздухе (для приземления)
	bool bReady = false;

	// Вес → чувствительность мыши
	float TargetSensitivityMult = 1.0f;
	float CurrentSensitivityMult = 1.0f;
	float BaseYawScale = 1.0f;
	float BasePitchScale = -1.0f;
	bool bScalesCached = false;
};
