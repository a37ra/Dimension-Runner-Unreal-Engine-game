// OrderDeliveryPoint.cpp — Реализация точки сдачи заказа

#include "OrderDeliveryPoint.h"
#include "GI_DimensionRunner.h"
#include "ArtifactBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDelivery, Log, All);

// ============================================================
// КОНСТРУКТОР
// ============================================================

AOrderDeliveryPoint::AOrderDeliveryPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// Меш (визуал дырки/контейнера)
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeliveryMesh"));
	RootComponent = MeshComp;
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Block);

	// Триггер приёма
	DeliveryTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("DeliveryTrigger"));
	DeliveryTrigger->SetupAttachment(RootComponent);
	DeliveryTrigger->SetBoxExtent(FVector(50.f, 50.f, 50.f));
	DeliveryTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DeliveryTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	DeliveryTrigger->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	DeliveryTrigger->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	DeliveryTrigger->SetGenerateOverlapEvents(true);

	// Относительное смещение триггера (настраивается в BP)
	DeliveryTrigger->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
}

// ============================================================
// BEGIN PLAY
// ============================================================

void AOrderDeliveryPoint::BeginPlay()
{
	Super::BeginPlay();

	DeliveryTrigger->OnComponentBeginOverlap.AddDynamic(this, &AOrderDeliveryPoint::OnTriggerOverlap);

	UE_LOG(LogDelivery, Log, TEXT("DeliveryPoint: Ready at %s"), *GetActorLocation().ToString());
}

// ============================================================
// OVERLAP
// ============================================================

void AOrderDeliveryPoint::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Принимаем только артефакты
	AArtifactBase* Artifact = Cast<AArtifactBase>(OtherActor);
	if (!Artifact)
	{
		return;
	}

	ProcessArtifact(Artifact);
}

// ============================================================
// ОБРАБОТКА АРТЕФАКТА
// ============================================================

void AOrderDeliveryPoint::ProcessArtifact(AArtifactBase* Artifact)
{
	if (!Artifact) return;

	const FName ItemID = Artifact->GetItemID();
	UE_LOG(LogDelivery, Log, TEXT("DeliveryPoint: Received artifact '%s' (ItemID: %s)"),
		*Artifact->GetItemName().ToString(), *ItemID.ToString());

	// Получаем GameInstance
	UGI_DimensionRunner* GI = Cast<UGI_DimensionRunner>(GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogDelivery, Warning, TEXT("DeliveryPoint: GameInstance is not UGI_DimensionRunner!"));
		return;
	}

	// Есть ли активный заказ?
	if (!GI->IsOrderActive())
	{
		UE_LOG(LogDelivery, Warning, TEXT("DeliveryPoint: No active order! Rejecting artifact."));
		OnDeliveryRejected();
		return;
	}

	// Сдаём заказ
	const FName OrderItemID = GI->GetCurrentOrder().ItemID;
	const bool bCorrectItem = (ItemID == OrderItemID);
	const int32 Payout = GI->CompleteOrder(ItemID);

	UE_LOG(LogDelivery, Log,
		TEXT("DeliveryPoint: Delivery %s! Item '%s' (expected '%s'). Payout: %d"),
		bCorrectItem ? TEXT("CORRECT") : TEXT("WRONG"),
		*ItemID.ToString(), *OrderItemID.ToString(), Payout);

	// Blueprint событие (VFX, звук, частицы)
	OnDeliveryComplete(Payout, bCorrectItem, ItemID);

	// === ЦИКЛ ДНЯ: сдал → только новый заказ (день тикает сам) ===
	GI->GenerateNextOrder();

	UE_LOG(LogDelivery, Log, TEXT("DeliveryPoint: Order completed. New order generated. Day stats updated."));

	// Уничтожить артефакт?
	if (bDestroyArtifactOnDelivery)
	{
		// Сразу прячем предмет визуально (мгновенно исчезает)
		Artifact->SetActorHiddenInGame(true);
		Artifact->SetActorEnableCollision(false);

		if (UPrimitiveComponent* ArtMesh = Artifact->GetGrabbableComponent())
		{
			ArtMesh->SetSimulatePhysics(false);
			ArtMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (DestroyDelay > 0.0f)
		{
			// Отложенное уничтожение (позволяет VFX доиграть)
			FTimerHandle DestroyHandle;
			TWeakObjectPtr<AArtifactBase> WeakArtifact = Artifact;
			GetWorldTimerManager().SetTimer(DestroyHandle, [WeakArtifact]()
			{
				if (WeakArtifact.IsValid())
				{
					WeakArtifact->Destroy();
				}
			}, DestroyDelay, false);
		}
		else
		{
			Artifact->Destroy();
		}
	}
}
