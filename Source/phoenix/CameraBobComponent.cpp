// CameraBobComponent.cpp — Juicy FPS Camera
#include "CameraBobComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

UCameraBobComponent::UCameraBobComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCameraBobComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	CameraComp = Owner->FindComponentByClass<UCameraComponent>();
	if (CameraComp)
	{
		BaseLocation = CameraComp->GetRelativeLocation();
		BaseFOV = CameraComp->FieldOfView;
		PrevYaw = Owner->GetActorRotation().Yaw;
		bReady = true;
	}
}

void UCameraBobComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bReady || !CameraComp || DeltaTime <= 0.0f) return;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character) return;

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp) return;

	// ===================== СОСТОЯНИЕ ПЕРСОНАЖА =====================
	const FVector Velocity = MoveComp->Velocity;
	const float Speed = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	const bool bMoving = Speed > 10.0f;
	const bool bOnGround = MoveComp->IsMovingOnGround();
	const bool bSprinting = Speed > SprintThreshold;
	const float SpeedMult = bSprinting ? SprintBobMultiplier : 1.0f;

	// ===================== ПОКАЧИВАНИЕ ПРИ ХОДЬБЕ =====================
	FVector LocationOffset = FVector::ZeroVector;
	float RollOffset = 0.0f;

	if (bMoving && bOnGround)
	{
		BobTimer += DeltaTime * BobFrequency * SpeedMult;

		// Вертикальное покачивание (синус — вверх-вниз при каждом шаге)
		LocationOffset.Z = FMath::Sin(BobTimer * 2.0f * PI) * BobVertical * SpeedMult;

		// Горизонтальное покачивание (косинус с половинной частотой — лево-право)
		LocationOffset.Y = FMath::Cos(BobTimer * PI) * BobHorizontal * SpeedMult;

		// Лёгкое покачивание Roll при ходьбе (имитация переноса веса с ноги на ногу)
		RollOffset += FMath::Sin(BobTimer * PI) * BobRoll * SpeedMult;
	}
	else if (bOnGround)
	{
		// ===================== ДЫХАНИЕ В ПОКОЕ =====================
		IdleTimer += DeltaTime * BreathSpeed;

		// Плавное покачивание по восьмёрке (дыхание)
		LocationOffset.Z = FMath::Sin(IdleTimer) * BreathAmplitude;
		LocationOffset.Y = FMath::Cos(IdleTimer * 0.7f) * (BreathAmplitude * 0.4f);

		// Сбрасываем BobTimer плавно, чтобы при остановке не было рывка
		BobTimer = FMath::FInterpTo(BobTimer, 0.0f, DeltaTime, 5.0f);
	}

	// ===================== НАКЛОН ПРИ СТРЕЙФЕ =====================
	if (bMoving)
	{
		const FVector LocalVel = Character->GetActorRotation().UnrotateVector(Velocity);
		const float StrafeRatio = FMath::Clamp(LocalVel.Y / FMath::Max(Speed, 1.0f), -1.0f, 1.0f);
		RollOffset += -StrafeRatio * StrafeTilt;
	}

	// ===================== НАКЛОН ПРИ ПОВОРОТЕ МЫШИ =====================
	const float CurrentYaw = Character->GetControlRotation().Yaw;
	float YawDelta = CurrentYaw - PrevYaw;

	// Нормализуем, чтобы при переходе через 360° не было прыжка
	if (YawDelta > 180.0f) YawDelta -= 360.0f;
	if (YawDelta < -180.0f) YawDelta += 360.0f;

	// Ограничиваем, чтобы при безумно быстром повороте камера не перевернулась
	YawDelta = FMath::Clamp(YawDelta, -10.0f, 10.0f);

	RollOffset += -YawDelta * TurnTilt;
	PrevYaw = CurrentYaw;

	// ===================== ПРИЗЕМЛЕНИЕ (DIP) =====================
	if (bOnGround && bWasInAir)
	{
		// Только что приземлились — добавляем просадку
		CurrentDip = LandingDip;
	}
	bWasInAir = !bOnGround;

	// Плавно восстанавливаем просадку
	CurrentDip = FMath::FInterpTo(CurrentDip, 0.0f, DeltaTime, LandingRecovery);
	LocationOffset.Z -= CurrentDip;

	// ===================== ДИНАМИЧЕСКИЙ FOV =====================
	const float TargetFOVAdd = bSprinting ? SprintFOVBoost : 0.0f;
	CurrentFOVAdd = FMath::FInterpTo(CurrentFOVAdd, TargetFOVAdd, DeltaTime, 6.0f);
	CameraComp->SetFieldOfView(BaseFOV + CurrentFOVAdd);

	// ===================== ПРИМЕНЕНИЕ ВСЕГО =====================
	// Плавно интерполируем Roll
	CurrentRoll = FMath::FInterpTo(CurrentRoll, RollOffset, DeltaTime, Smoothing);

	// Позиция камеры
	const FVector TargetLocation = BaseLocation + LocationOffset;
	CameraComp->SetRelativeLocation(
		FMath::VInterpTo(CameraComp->GetRelativeLocation(), TargetLocation, DeltaTime, Smoothing));

	// Поворот камеры (ТОЛЬКО Roll — Pitch и Yaw управляются движком!)
	FRotator CamRot = CameraComp->GetRelativeRotation();
	CamRot.Roll = CurrentRoll;
	CameraComp->SetRelativeRotation(CamRot);

	// ===================== ВЕС → ЧУВСТВИТЕЛЬНОСТЬ МЫШИ =====================
	if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		// Кэшируем базовые значения при первом тике
		if (!bScalesCached)
		{
			BaseYawScale = PC->GetDeprecatedInputYawScale();
			BasePitchScale = PC->GetDeprecatedInputPitchScale();
			bScalesCached = true;
		}

		// Плавная интерполяция чувствительности
		CurrentSensitivityMult = FMath::FInterpTo(
			CurrentSensitivityMult, TargetSensitivityMult, DeltaTime, 6.0f);

		const float TargetYaw = BaseYawScale * CurrentSensitivityMult;
		const float TargetPitch = BasePitchScale * CurrentSensitivityMult;

		PC->SetDeprecatedInputYawScale(
			FMath::FInterpTo(PC->GetDeprecatedInputYawScale(), TargetYaw, DeltaTime, 8.0f));
		PC->SetDeprecatedInputPitchScale(
			FMath::FInterpTo(PC->GetDeprecatedInputPitchScale(), TargetPitch, DeltaTime, 8.0f));
	}
}

void UCameraBobComponent::SetWeightSensitivity(float Multiplier)
{
	TargetSensitivityMult = FMath::Clamp(Multiplier, 0.3f, 1.0f);
	UE_LOG(LogTemp, Log, TEXT("CameraBob: Sensitivity target = %.2f"), TargetSensitivityMult);
}
