// SprintStaminaComponent.cpp — Финальная надежная версия
#include "SprintStaminaComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

USprintStaminaComponent::USprintStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USprintStaminaComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentStamina = MaxStamina;
	StaminaPercent = 1.0f;
	
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetCharacterMovement())
	{
		MoveComp = Character->GetCharacterMovement();
		MoveComp->MaxWalkSpeed = WalkSpeed; // Стартуем с ходьбы
	}
}

void USprintStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!MoveComp || DeltaTime <= 0.0f) return;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!PC) return;

	// ===================== SHIFT DETECTION =====================
	// Прямая проверка клавиши через контроллер (игнорирует баги Input событий)
	bShiftHeld = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);

	// ===================== SPRINT LOGIC =====================
	const float HorizSpeed = MoveComp->Velocity.Size2D();
	const bool bMovingEnough = HorizSpeed > 50.0f; // Должен реально идти
	const bool bOnGround = MoveComp->IsMovingOnGround();

	// Можно ли бежать?
	bool bCanSprint = !bStaminaDepleted && CurrentStamina > 0.0f;
	
	// Если стамина кончилась, нужно подождать до порога MinStaminaToSprint
	if (bStaminaDepleted)
	{
		if (CurrentStamina >= MinStaminaToSprint) bStaminaDepleted = false;
	}

	bIsSprinting = bShiftHeld && bMovingEnough && bOnGround && bCanSprint;

	// ===================== STAMINA DRAIN/REGEN =====================
	if (bIsSprinting)
	{
		CurrentStamina = FMath::Max(CurrentStamina - StaminaDrainRate * DeltaTime, 0.0f);
		if (CurrentStamina <= 0.0f) bStaminaDepleted = true;
		TimeSinceStoppedSprinting = 0.0f;
	}
	else
	{
		TimeSinceStoppedSprinting += DeltaTime;
		if (TimeSinceStoppedSprinting >= RegenDelay)
		{
			CurrentStamina = FMath::Min(CurrentStamina + StaminaRegenRate * DeltaTime, MaxStamina);
		}
	}

	StaminaPercent = CurrentStamina / MaxStamina;

	// ===================== APPLY SPEED =====================
	const float TargetSpeed = (bIsSprinting ? SprintSpeed : WalkSpeed) * ExternalSpeedMultiplier;
	MoveComp->MaxWalkSpeed = FMath::FInterpTo(MoveComp->MaxWalkSpeed, TargetSpeed, DeltaTime, 10.0f);
}
