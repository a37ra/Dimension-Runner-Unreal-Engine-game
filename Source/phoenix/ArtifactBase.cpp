// ArtifactBase.cpp — Базовый класс подбираемого артефакта

#include "ArtifactBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"

AArtifactBase::AArtifactBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// Меш с физикой — основа артефакта
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArtifactMesh"));
	RootComponent = MeshComponent;

	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MeshComponent->SetNotifyRigidBodyCollision(true); // Для OnHit

	// Привязка события столкновения
	MeshComponent->OnComponentHit.AddDynamic(this, &AArtifactBase::OnHit);
}

void AArtifactBase::BeginPlay()
{
	Super::BeginPlay();
}

FText AArtifactBase::GetInteractHintText() const
{
	return FText::Format(
		NSLOCTEXT("Artifact", "CarryHint", "E: Нести {0}"),
		FText::FromName(ItemName)
	);
}

void AArtifactBase::OnThrown()
{
	bWasThrown = true;
	bPlacedGently = false;
	OnThrownEvent(); // Blueprint event
}

void AArtifactBase::OnPlacedGently()
{
	bPlacedGently = true;
	bWasThrown = false;
}

void AArtifactBase::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Если аккуратно положили — без урона
	if (bPlacedGently)
	{
		bPlacedGently = false;
		return;
	}

	// Хрупкие предметы получают урон от сильных ударов
	if (bIsFragile)
	{
		float ImpactForce = NormalImpulse.Size();
		float DamageThreshold = 5000.0f; // Минимальная сила для урона

		if (ImpactForce > DamageThreshold)
		{
			float Damage = (ImpactForce - DamageThreshold) / 1000.0f;
			ItemHP = FMath::Max(0.0f, ItemHP - Damage);

			UE_LOG(LogTemp, Warning, TEXT("Artifact %s took %.1f damage (HP: %.1f/%.1f)"),
				*ItemName.ToString(), Damage, ItemHP, MaxItemHP);

			if (ItemHP <= 0.0f)
			{
				UE_LOG(LogTemp, Error, TEXT("Artifact %s is BROKEN!"), *ItemName.ToString());
				OnBroken(); // Blueprint event
			}
		}
	}

	bWasThrown = false;
}
