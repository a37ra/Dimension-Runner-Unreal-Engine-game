// CarryComponent.cpp — Физическая переноска + луч + частицы (оптимизировано)

#include "CarryComponent.h"
#include "ArtifactBase.h"
#include "SprintStaminaComponent.h"
#include "CameraBobComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "CableComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UCarryComponent::UCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCarryComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHoldDistance = DefaultHoldDistance;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Кэш камеры
	CachedCamManager = UGameplayStatics::GetPlayerCameraManager(this, 0);

	// PhysicsHandle
	PhysicsHandle = NewObject<UPhysicsHandleComponent>(Owner);
	if (PhysicsHandle)
	{
		PhysicsHandle->RegisterComponent();
		PhysicsHandle->LinearDamping = 200.0f;
		PhysicsHandle->LinearStiffness = 750.0f;
		PhysicsHandle->AngularDamping = 200.0f;
		PhysicsHandle->AngularStiffness = 750.0f;
		PhysicsHandle->InterpolationSpeed = 15.0f;
	}

	// Луч (CableComponent)
	BeamCable = NewObject<UCableComponent>(Owner);
	if (BeamCable)
	{
		BeamCable->RegisterComponent();
		BeamCable->AttachToComponent(Owner->GetRootComponent(),
			FAttachmentTransformRules::KeepRelativeTransform);
		BeamCable->SetRelativeLocation(BeamAttachOffset);
		BeamCable->CableWidth = BeamWidth;
		BeamCable->CableLength = 0.0f;
		BeamCable->NumSegments = 8;
		BeamCable->SolverIterations = 4;
		BeamCable->bEnableStiffness = true;
		BeamCable->bAttachEnd = true;
		BeamCable->EndLocation = FVector::ZeroVector;
		BeamCable->CableGravityScale = 0.3f;
		BeamCable->SetVisibility(false);
	}

	// Кэш StaminaComponent + CameraBobComponent
	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		StaminaComp = Character->FindComponentByClass<USprintStaminaComponent>();
		CameraBobComp = Character->FindComponentByClass<UCameraBobComponent>();
	}
}

// ==================== ТИКС ====================

void UCarryComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Трейс не нужен, если уже несём предмет
	if (!IsCarrying())
	{
		UpdateLookTrace();
	}
	else
	{
		UpdateHeldObjectPosition();
		UpdateBeam();
	}
}

// ==================== ТРЕЙС ====================

void UCarryComponent::UpdateLookTrace()
{
	LookedAtArtifact = nullptr;

	if (!CachedCamManager) return;

	const FVector Start = CachedCamManager->GetCameraLocation();
	const FVector End = Start + CachedCamManager->GetCameraRotation().Vector() * GrabTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		LookedAtArtifact = Cast<AArtifactBase>(Hit.GetActor());
	}
}

// ==================== КОМАНДЫ ====================

void UCarryComponent::BeginGrab()
{
	if (IsCarrying() || !LookedAtArtifact || !PhysicsHandle) return;

	if (GrabHoldTime > 0.0f)
	{
		// Запускаем таймер на удержание (в этот момент можно показывать UI-прогресс в будущем)
		GetWorld()->GetTimerManager().SetTimer(GrabTimerHandle, this, &UCarryComponent::ExecuteGrabTimerObj, GrabHoldTime, false);
	}
	else
	{
		TryGrab();
	}
}

void UCarryComponent::EndGrab()
{
	if (GetWorld()->GetTimerManager().IsTimerActive(GrabTimerHandle))
	{
		GetWorld()->GetTimerManager().ClearTimer(GrabTimerHandle);
	}
}

void UCarryComponent::ExecuteGrabTimerObj()
{
	TryGrab();
}

bool UCarryComponent::TryGrab()
{
	if (IsCarrying() || !LookedAtArtifact || !PhysicsHandle) return false;

	UPrimitiveComponent* MeshComp = LookedAtArtifact->GetGrabbableComponent();
	if (!MeshComp) return false;

	PhysicsHandle->GrabComponentAtLocationWithRotation(
		MeshComp, NAME_None,
		MeshComp->GetComponentLocation(),
		MeshComp->GetComponentRotation()
	);

	CarriedArtifact = LookedAtArtifact;
	CurrentHoldDistance = DefaultHoldDistance;

	ApplyWeightPenalty();
	ShowBeam();
	CarriedArtifact->OnPickedUp();

	return true;
}

void UCarryComponent::Release()
{
	if (!IsCarrying() || !PhysicsHandle) return;

	PhysicsHandle->ReleaseComponent();
	RemoveWeightPenalty();
	HideBeam();

	CarriedArtifact = nullptr;
}

void UCarryComponent::Throw()
{
	if (!IsCarrying()) return;

	AArtifactBase* Artifact = CarriedArtifact;
	UPrimitiveComponent* MeshComp = Artifact->GetGrabbableComponent();

	Release();

	if (MeshComp && CachedCamManager)
	{
		const FVector Dir = CachedCamManager->GetCameraRotation().Vector();
		const float AdjustedForce = ThrowForce / FMath::Max(Artifact->GetWeight(), 0.5f);
		MeshComp->AddImpulse(Dir * AdjustedForce, NAME_None, true);
	}

	Artifact->OnThrown();
}

void UCarryComponent::PlaceGently()
{
	if (!IsCarrying()) return;

	AArtifactBase* Artifact = CarriedArtifact;
	Release();
	Artifact->OnPlacedGently();
}

void UCarryComponent::AdjustDistance(float Delta)
{
	if (!IsCarrying()) return;

	// Чем тяжелее — тем медленнее колёсико
	const float ScrollMult = CalcWeightFactor(
		CarriedArtifact->GetWeight(), MinScrollSpeed);

	CurrentHoldDistance = FMath::Clamp(
		CurrentHoldDistance + Delta * DistanceStep * ScrollMult,
		MinHoldDistance, MaxHoldDistance
	);
}

// ==================== ПОЗИЦИЯ ПРЕДМЕТА ====================

void UCarryComponent::UpdateHeldObjectPosition()
{
	if (!PhysicsHandle || !CachedCamManager) return;

	const FVector CamLoc = CachedCamManager->GetCameraLocation();
	const FRotator CamRot = CachedCamManager->GetCameraRotation();
	const FVector Target = CamLoc + CamRot.Vector() * CurrentHoldDistance;

	PhysicsHandle->SetTargetLocationAndRotation(Target, CamRot);
}

// ==================== ЛУЧ + FX ====================

void UCarryComponent::ShowBeam()
{
	if (!BeamCable || !CarriedArtifact) return;

	if (BeamMaterial)
	{
		BeamCable->SetMaterial(0, BeamMaterial);
	}

	UPrimitiveComponent* MeshComp = CarriedArtifact->GetGrabbableComponent();
	if (MeshComp)
	{
		BeamCable->SetAttachEndToComponent(MeshComp);
	}

	BeamCable->SetVisibility(true);

	// Частицы на концах луча
	if (BeamEndParticles && MeshComp)
	{
		StartFXComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			BeamEndParticles,
			GetOwner()->GetRootComponent(), NAME_None,
			BeamAttachOffset, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true
		);

		EndFXComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			BeamEndParticles,
			MeshComp, NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true
		);
	}
}

void UCarryComponent::HideBeam()
{
	if (BeamCable)
	{
		BeamCable->SetAttachEndToComponent(nullptr);
		BeamCable->SetVisibility(false);
	}

	if (StartFXComp) { StartFXComp->Deactivate(); StartFXComp->DestroyComponent(); StartFXComp = nullptr; }
	if (EndFXComp)   { EndFXComp->Deactivate();   EndFXComp->DestroyComponent();   EndFXComp = nullptr;   }
}

void UCarryComponent::UpdateBeam()
{
	if (!BeamCable || !CarriedArtifact) return;

	const FVector OwnerLoc = GetOwner()->GetActorLocation()
		+ GetOwner()->GetActorRotation().RotateVector(BeamAttachOffset);
	const float Distance = FVector::Dist(OwnerLoc, CarriedArtifact->GetActorLocation());
	BeamCable->CableLength = Distance * 1.15f;
}

// ==================== ВЕС → СКОРОСТЬ ====================

float UCarryComponent::CalcSpeedMultiplier(float Weight) const
{
	// Лёгкие предметы (Weight <= LightWeightThreshold) не замедляют
	if (Weight <= LightWeightThreshold) return 1.0f;

	// Пропорциональное замедление: Weight 2→100 → Multiplier 1.0→0.25
	const float T = FMath::Clamp(
		(Weight - LightWeightThreshold) / (MaxWeightForCalc - LightWeightThreshold),
		0.0f, 1.0f
	);

	return FMath::Lerp(1.0f, MinSpeedMultiplier, T);
}

float UCarryComponent::GetCurrentSpeedMultiplier() const
{
	if (!CarriedArtifact) return 1.0f;
	return CalcSpeedMultiplier(CarriedArtifact->GetWeight());
}

void UCarryComponent::ApplyWeightPenalty()
{
	if (!CarriedArtifact) return;

	const float Weight = CarriedArtifact->GetWeight();

	// Скорость движения
	if (StaminaComp)
	{
		const float SpeedMult = CalcSpeedMultiplier(Weight);
		StaminaComp->SetExternalSpeedMultiplier(SpeedMult);
		UE_LOG(LogTemp, Log, TEXT("Carry: Weight %.1f → Speed x%.2f"), Weight, SpeedMult);
	}

	// Чувствительность мыши
	if (CameraBobComp)
	{
		const float SensMult = CalcWeightFactor(Weight, MinMouseSensitivity);
		CameraBobComp->SetWeightSensitivity(SensMult);
		UE_LOG(LogTemp, Log, TEXT("Carry: Weight %.1f → Sensitivity x%.2f"), Weight, SensMult);
	}
}

void UCarryComponent::RemoveWeightPenalty()
{
	if (StaminaComp)
		StaminaComp->SetExternalSpeedMultiplier(1.0f);

	if (CameraBobComp)
		CameraBobComp->SetWeightSensitivity(1.0f);
}

float UCarryComponent::CalcWeightFactor(float Weight, float MinFactor) const
{
	if (Weight <= LightWeightThreshold) return 1.0f;

	const float T = FMath::Clamp(
		(Weight - LightWeightThreshold) / (MaxWeightForCalc - LightWeightThreshold),
		0.0f, 1.0f
	);

	return FMath::Lerp(1.0f, MinFactor, T);
}
