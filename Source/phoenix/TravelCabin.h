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

UCLASS(Blueprintable)
class PHOENIX_API ATravelCabin : public AActor
{
	GENERATED_BODY()

public:
	ATravelCabin();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	TObjectPtr<USoundBase> DoorOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	TObjectPtr<USoundBase> DoorCloseSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	TObjectPtr<USoundBase> DepartureSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	TObjectPtr<USoundBase> ArrivalSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	float DepartureShakeMax = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	float DepartureShakeRampUp = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	float ArrivalShakeRampDown = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	float DoorShakeIntensity = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Settings")
	float DoorShakeDuration = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Cabin|State")
	ECabinState CabinState = ECabinState::Ready;

	UPROPERTY(BlueprintReadOnly, Category = "Cabin|State")
	int32 CurrentTime = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cabin|State")
	bool bIsPlayerInside = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cabin|State")
	bool bIsActivated = false;

	UFUNCTION(BlueprintCallable, Category = "Cabin")
	void StartExpedition();

	UFUNCTION(BlueprintCallable, Category = "Cabin")
	void StartReturn();

	UFUNCTION(BlueprintImplementableEvent, Category = "Cabin")
	void OnPlayerLeftBehind();

	UFUNCTION(BlueprintCallable, Category = "Cabin")
	bool IsLookingAtAnyInteractable(AActor* Interactor) const;

	void UpdateSlotScreen();
	UCabinSlotWidget* GetSlotWidget() const;
	void SaveCabinToStorage();
	void RestoreCabinFromStorage();

	UFUNCTION()
	void OnInventoryChangedHandler();

	EShakeMode CurrentShakeMode = EShakeMode::None;
	float ShakeElapsed = 0.0f;
	float ShakeCurrentIntensity = 0.0f;

	FVector LastShakeOffset = FVector::ZeroVector;
	float LastShakeRoll = 0.0f;

	void StartShake(EShakeMode Mode);
	void TickShake(float DeltaTime);
	void ApplyShakeToCamera(float Intensity);

	UPROPERTY()
	class UAudioComponent* DepartureSoundComp;
};
