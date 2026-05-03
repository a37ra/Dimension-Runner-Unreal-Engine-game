#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "DimensionRunnerTypes.h"
#include "TravelCabin.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class UCurveFloat;
class USoundBase;
class UAudioComponent;
class UTextBlock;
class UPostProcessComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UCabinInventoryComponent;
class UCabinSlotWidget;
class UGI_DimensionRunner;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class ECabinState : uint8
{
	Ready,
	Launching,
	Expedition,
	Returning,
	Overheat
};

UENUM(BlueprintType)
enum class EShakeMode : uint8
{
	None,
	Departing,
	Arriving,
	DoorBump
};

// Action deferred until doors finish closing
enum class EPendingAction : uint8
{
	None,
	TeleportOut,
	TeleportHome,
	PlayerAbandoned
};

UCLASS(Blueprintable)
class PHOENIX_API ATravelCabin : public AActor
{
	GENERATED_BODY()

public:
	ATravelCabin();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ============================================================
	// Settings (editable in Blueprint)
	// ============================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	int32 ExpeditionTime = 185;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	int32 CooldownTime = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	FName TargetLevelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	float TeleportDelay = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	float DoorOpenDelay = 2.0f;

	// --- Sounds ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Sounds")
	TObjectPtr<USoundBase> DoorOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Sounds")
	TObjectPtr<USoundBase> DoorCloseSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Sounds")
	TObjectPtr<USoundBase> DepartureSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Sounds")
	TObjectPtr<USoundBase> ArrivalSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Sounds")
	TObjectPtr<USoundBase> SirenSound;

	// --- Camera Shake ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Shake")
	float DepartureShakeMax = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Shake")
	float DepartureShakeRampUp = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Shake")
	float ArrivalShakeRampDown = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Shake")
	float DoorShakeIntensity = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Shake")
	float DoorShakeDuration = 0.5f;

	// --- Door Animation ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Doors")
	TObjectPtr<UCurveFloat> DoorAnimCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Doors")
	FVector DoorLOpenPos = FVector(0.f, -150.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Doors")
	FVector DoorROpenPos = FVector(0.f, 150.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Doors")
	FVector DoorLClosedPos = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Doors")
	FVector DoorRClosedPos = FVector::ZeroVector;

	// --- Singularity ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Singularity")
	TObjectPtr<UMaterialInterface> SingularityMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Singularity")
	float SingularityDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Singularity")
	float SingularityMaxIntensity = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Singularity")
	float ExpandFraction = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Singularity")
	float ExpandScale = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Singularity")
	float FlashMaxIntensity = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Singularity")
	FLinearColor FlashColor = FLinearColor(0.6f, 0.3f, 1.0f, 1.0f);

	// ============================================================
	// Runtime State
	// ============================================================

	UPROPERTY(BlueprintReadOnly, Category = "Cabin|State")
	ECabinState CabinState = ECabinState::Ready;

	UPROPERTY(BlueprintReadOnly, Category = "Cabin|State")
	int32 CurrentTime = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cabin|State")
	bool bIsPlayerInside = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cabin|State")
	bool bIsActivated = false;

	// ============================================================
	// Public Functions
	// ============================================================

	UFUNCTION(BlueprintCallable, Category = "Cabin")
	void StartExpedition();

	UFUNCTION(BlueprintCallable, Category = "Cabin")
	void StartReturn();

	UFUNCTION(BlueprintImplementableEvent, Category = "Cabin")
	void OnPlayerLeftBehind();

	UFUNCTION(BlueprintImplementableEvent, Category = "Cabin")
	void OnCabinStateChanged(ECabinState NewState);

	UFUNCTION(BlueprintCallable, Category = "Cabin")
	bool IsLookingAtAnyInteractable(AActor* Interactor) const;

	// Interact entry point
	UFUNCTION(BlueprintCallable, Category = "Cabin")
	void OnInteract(AActor* Interactor);

	// Inventory
	void UpdateSlotScreen();
	UCabinSlotWidget* GetSlotWidget() const;
	void SaveCabinToStorage();
	void RestoreCabinFromStorage();

	UFUNCTION()
	void OnInventoryChangedHandler();

	// ============================================================
	// Camera Shake
	// ============================================================

	EShakeMode CurrentShakeMode = EShakeMode::None;
	float ShakeElapsed = 0.0f;
	float ShakeCurrentIntensity = 0.0f;

	FVector LastShakeOffset = FVector::ZeroVector;
	float LastShakeRoll = 0.0f;

	void StartShake(EShakeMode Mode);
	void TickShake(float DeltaTime);
	void ApplyShakeToCamera(float Intensity);

private:
	// ============================================================
	// Internal Helpers
	// ============================================================

	void FindBlueprintComponents();
	void SetupDoorTimeline();

	void SetCabinState(ECabinState NewState);

	void OpenDoors();
	void CloseDoors();

	void EnableButton();
	void DisableButton();

	void StartCabinTimer();
	void StopCabinTimer();
	void TickCabinTimer();

	void PerformTeleport();

	void UpdateScreenText(int32 Seconds);
	void SetScreenText(const FString& Text, float FontScale = 1.0f);
	void SetTimerTextColor(const FLinearColor& Color);

	UTextBlock* GetTimerTextWidget() const;
	UGI_DimensionRunner* GetDimensionRunnerGI() const;

	bool IsLookingAtMainButton(AActor* Interactor) const;
	bool TryHandleButtonInteract(AActor* Interactor);

	// Singularity
	void StartSingularityAndShrink();
	void TickSingularity(float DeltaTime);
	void FinishSingularity();

	// Overlap callbacks
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// Door timeline callbacks
	UFUNCTION()
	void OnDoorTimelineUpdate(float Alpha);

	UFUNCTION()
	void OnDoorTimelineFinished();

	// ============================================================
	// Cached Blueprint Components
	// ============================================================

	UPROPERTY()
	UStaticMeshComponent* DoorL = nullptr;

	UPROPERTY()
	UStaticMeshComponent* DoorR = nullptr;

	UPROPERTY()
	UStaticMeshComponent* CachedButton = nullptr;

	UPROPERTY()
	UPrimitiveComponent* CachedTrigger = nullptr;

	UPROPERTY()
	UPrimitiveComponent* CachedBlocker = nullptr;

	UPROPERTY()
	UPrimitiveComponent* CachedInventoryTrig = nullptr;

	UPROPERTY()
	UPrimitiveComponent* CachedButtonLoad = nullptr;

	UPROPERTY()
	UPrimitiveComponent* CachedButtonSelect = nullptr;

	UPROPERTY()
	UPrimitiveComponent* CachedButtonUnload = nullptr;

	UPROPERTY()
	UWidgetComponent* CachedScreen = nullptr;

	UPROPERTY()
	UWidgetComponent* CachedSlotScreen = nullptr;

	UPROPERTY()
	UPostProcessComponent* CachedSingularityPP = nullptr;

	// ============================================================
	// Runtime Data
	// ============================================================

	UPROPERTY()
	UCabinInventoryComponent* CabinInventory = nullptr;

	UPROPERTY()
	UAudioComponent* DepartureSoundComp = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* SingularityDMI = nullptr;

	UPROPERTY()
	UPointLightComponent* FlashLight = nullptr;

	FTimeline DoorTimeline;
	FTimerHandle CabinTimerHandle;

	EPendingAction PendingAction = EPendingAction::None;

	int32 CachedOriginalFontSize = 0;

	// Singularity runtime
	bool bSingularityActive = false;
	float SingularityElapsed = 0.0f;
	FVector OriginalScale = FVector::OneVector;
	int32 UVUpdateCounter = 0;
};
