// TravelCabin.cpp — Полная логика кабины телепортации
#include "TravelCabin.h"
#include "GI_DimensionRunner.h"
#include "CabinInventoryComponent.h"
#include "CabinSlotWidget.h"
#include "CabinStorageSubsystem.h"
#include "ArtifactBase.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/TextBlock.h"
#include "Components/PostProcessComponent.h"
#include "Components/PointLightComponent.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Curves/CurveFloat.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogCabin, Log, All);

// ============================================================
// CONSTRUCTOR
// ============================================================

ATravelCabin::ATravelCabin()
{
	PrimaryActorTick.bCanEverTick = true;
}

// ============================================================
// BEGIN PLAY — Определяем состояние кабины
// ============================================================

void ATravelCabin::BeginPlay()
{
	Super::BeginPlay();

	// 1. Находим компоненты Blueprint по имени
	FindBlueprintComponents();

	// 2. Настраиваем Timeline для дверей
	SetupDoorTimeline();

	// 3. Привязываем Overlap-события триггера
	if (CachedTrigger)
	{
		CachedTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATravelCabin::OnTriggerBeginOverlap);
		CachedTrigger->OnComponentEndOverlap.AddDynamic(this, &ATravelCabin::OnTriggerEndOverlap);
	}

	// 4. Двери начинают закрытыми
	if (DoorL) DoorL->SetRelativeLocation(DoorLClosedPos);
	if (DoorR) DoorR->SetRelativeLocation(DoorRClosedPos);

	// 5. Спрашиваем GameInstance: где мы?
	UGI_DimensionRunner* GI = GetDimensionRunnerGI();
	const bool bOnMission = GI ? GI->CheckCabinStatus() : false;
	const bool bOverheated = GI ? GI->CheckOverheatStatus() : false;

	if (bOnMission)
	{
		// === ЭКСПЕДИЦИЯ: мы на L_WorldA ===
		if (GI && GI->IsOrderActive())
		{
			ExpeditionTime = FMath::RoundToInt32(GI->GetCurrentOrder().TimerSeconds);
		}
		UE_LOG(LogCabin, Log, TEXT("Cabin: Expedition mode. Timer %d sec."), ExpeditionTime);

		SetCabinState(ECabinState::Expedition);
		CurrentTime = ExpeditionTime;
		EnableButton(); // кнопка не нужна, но пусть будет

		// Пауза, потом двери открываются и таймер стартует
		FTimerHandle DelayHandle;
		GetWorldTimerManager().SetTimer(DelayHandle, [this]()
		{
			OpenDoors();
			UpdateScreenText(CurrentTime);
			SetTimerTextColor(FLinearColor::White);
			StartCabinTimer();
		}, DoorOpenDelay, false);
	}
	else if (bOverheated)
	{
		// === ПЕРЕГРЕВ: вернулись с миссии, кабина остывает ===
		UE_LOG(LogCabin, Log, TEXT("Cabin: Overheat cooldown. Timer %d sec."), CooldownTime);

		SetCabinState(ECabinState::Overheat);
		CurrentTime = CooldownTime;
		DisableButton(); // кнопка отключена!

		// Двери открываются (выпускаем игрока), таймер перезагрузки
		FTimerHandle DelayHandle;
		GetWorldTimerManager().SetTimer(DelayHandle, [this]()
		{
			OpenDoors();
			UpdateScreenText(CurrentTime);
			SetTimerTextColor(FLinearColor(1.0f, 0.7f, 0.0f, 1.0f)); // Жёлтый
			StartCabinTimer();
		}, DoorOpenDelay, false);
	}
	else
	{
		// === ГОТОВНОСТЬ: кабина ждёт приказа ===
		UE_LOG(LogCabin, Log, TEXT("Cabin: Ready."));

		SetCabinState(ECabinState::Ready);
		EnableButton();

		// Генерируем заказ если его нет (первый запуск / новая игра)
		if (GI)
		{
			if (!GI->IsOrderActive())
			{
				GI->GenerateNextOrder();
			}
		}

		// Двери сразу открыты, экран зелёный "ГОТОВО"
		FTimerHandle DelayHandle;
		GetWorldTimerManager().SetTimer(DelayHandle, [this]()
		{
			OpenDoors();
			SetScreenText(TEXT("\u0413\u041E\u0422\u041E\u0412\u041E"), 0.4f);
			SetTimerTextColor(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f)); // Зелёный
		}, 0.5f, false);
	}

	// 6. Инвентарь кабины
	CabinInventory = NewObject<UCabinInventoryComponent>(this);
	if (CabinInventory)
	{
		CabinInventory->RegisterComponent();
		CabinInventory->Initialize(CachedInventoryTrig);
		CabinInventory->OnInventoryChanged.AddDynamic(this, &ATravelCabin::OnInventoryChangedHandler);

		// Восстановить сохранённый инвентарь (после смены уровня)
		RestoreCabinFromStorage();
	}
}

// ============================================================
// TICK — обновляем Timeline
// ============================================================

void ATravelCabin::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	DoorTimeline.TickTimeline(DeltaTime);
	if (bSingularityActive)
		TickSingularity(DeltaTime);
}

// ============================================================
// INTERACT — игрок нажал кнопку
// ============================================================

void ATravelCabin::OnInteract(AActor* Interactor)
{
	// Проверка 0: Кнопки инвентаря?
	if (TryHandleButtonInteract(Interactor))
		return;

	// Проверка 0.5: Игрок смотрит на ОСНОВНУЮ кнопку запуска?
	if (!IsLookingAtMainButton(Interactor))
	{
		UE_LOG(LogCabin, Log, TEXT("Cabin: Interact rejected \u2014 not looking at main button."));
		return;
	}

	// Проверка 1: Игрок внутри?
	if (CachedTrigger)
	{
		ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0);
		bIsPlayerInside = CachedTrigger->IsOverlappingActor(PlayerChar);
	}

	if (!bIsPlayerInside)
	{
		UE_LOG(LogCabin, Warning, TEXT("Cabin: Interact rejected \u2014 player not inside."));
		return;
	}

	// Проверка 2: Кабина уже запущена?
	if (bIsActivated)
	{
		UE_LOG(LogCabin, Warning, TEXT("Cabin: Interact rejected \u2014 already activated."));
		return;
	}

	// === ДОСРОЧНОЕ ВОЗВРАЩЕНИЕ ИЗ ЭКСПЕДИЦИИ ===
	if (CabinState == ECabinState::Expedition)
	{
		UE_LOG(LogCabin, Log, TEXT("Cabin: Manual early return from expedition triggered!"));
		
		bIsActivated = true;
		DisableButton();
		StopCabinTimer();

		// Сохраняем новый лут кабины
		SaveCabinToStorage();
		
		if (UGI_DimensionRunner* GI = GetDimensionRunnerGI())
		{
			GI->SetCabinStatus(false);
			GI->SetOverheatStatus(true);
		}
		
		// Назад в Лабораторию
		SetCabinState(ECabinState::Returning);
		TargetLevelName = GetBaseLevelName();
		PendingAction = EPendingAction::TeleportHome;
		CloseDoors();
		return;
	}

	// Проверка 3: Кабина перегрета?
	if (CabinState == ECabinState::Overheat)
	{
		UE_LOG(LogCabin, Warning, TEXT("Cabin: Interact rejected \u2014 overheated."));
		return;
	}

	// Проверка 4: Кабина готова?
	if (CabinState != ECabinState::Ready)
	{
		UE_LOG(LogCabin, Warning, TEXT("Cabin: Interact rejected \u2014 state is %d."), (int32)CabinState);
		return;
	}

	// === ЗАПУСК ===
	UE_LOG(LogCabin, Log, TEXT("Cabin: LAUNCHING! Saving inventory, closing doors..."));

	bIsActivated = true;
	SetCabinState(ECabinState::Launching);
	DisableButton();

	// Сохраняем инвентарь кабины
	SaveCabinToStorage();

	// Ставим статус "На миссии" в GI + берём уровень из текущего заказа
	if (UGI_DimensionRunner* GI = GetDimensionRunnerGI())
	{
		GI->SetCabinStatus(true);
		// Уровень назначения из текущего заказа (если есть), иначе используем TargetLevelName из Блюпринта
		if (GI->IsOrderActive())
		{
			TargetLevelName = GI->GetCurrentMissionLevel();
			ExpeditionTime = FMath::RoundToInt32(GI->GetCurrentOrder().TimerSeconds);
			UE_LOG(LogCabin, Log, TEXT("Cabin: Mission level from order: %s (Timer: %d sec)"),
				*TargetLevelName.ToString(), ExpeditionTime);
		}
	}

	// Закрываем двери → после закрытия телепортируемся
	PendingAction = EPendingAction::TeleportOut;
	CloseDoors();
}

// ============================================================
// TIMER TICK — каждая секунда
// ============================================================

void ATravelCabin::TickCabinTimer()
{
	CurrentTime -= 1;
	UpdateScreenText(CurrentTime);

	if (CabinState == ECabinState::Expedition)
	{
		// --- Экспедиция ---

		if (CurrentTime == 15)
		{
			// Красный текст + сирена
			SetTimerTextColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
			if (SirenSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, SirenSound, GetActorLocation());
			}
			UE_LOG(LogCabin, Warning, TEXT("Cabin: 15 seconds remaining! SIREN!"));
		}

		if (CurrentTime <= 0)
		{
			// Время вышло!
			StopCabinTimer();

			if (CachedTrigger)
			{
				ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0);
				bIsPlayerInside = CachedTrigger->IsOverlappingActor(PlayerChar);
			}

			UE_LOG(LogCabin, Log, TEXT("Cabin: TIME IS UP. Player inside: %s"),
				bIsPlayerInside ? TEXT("YES") : TEXT("NO"));

			if (bIsPlayerInside)
			{
				// === СЦЕНАРИЙ Б: СПАСЕНИЕ ===
				// Сохраняем новый лут
				SaveCabinToStorage();
				if (UGI_DimensionRunner* GI = GetDimensionRunnerGI())
				{
					GI->SetCabinStatus(false);
					GI->SetOverheatStatus(true);
				}
				// Закрываем двери → телепорт домой
				SetCabinState(ECabinState::Returning);
				
				TargetLevelName = GetBaseLevelName();
				
				PendingAction = EPendingAction::TeleportHome;
				CloseDoors();
			}
			else
			{
				// === СЦЕНАРИЙ А: ИГРОК НЕ УСПЕЛ ===
				if (UGI_DimensionRunner* GI = GetDimensionRunnerGI())
				{
					GI->SetCabinStatus(false);
					GI->SetOverheatStatus(true);
					GI->FailOrder(); // Штраф -12% денег, -1 секция
				}
				PendingAction = EPendingAction::PlayerAbandoned;
				CloseDoors();
			}
		}
	}
	else if (CabinState == ECabinState::Overheat)
	{
		// --- Перегрев / Перезагрузка ---

		if (CurrentTime <= 0)
		{
			// Перезагрузка завершена!
			StopCabinTimer();
			UE_LOG(LogCabin, Log, TEXT("Cabin: Cooldown complete. Ready!"));

			// Снимаем перегрев
			if (UGI_DimensionRunner* GI = GetDimensionRunnerGI())
			{
				GI->SetOverheatStatus(false);
				// День и заказ НЕ генерируем здесь —
				// это делает DeliveryPoint (при сдаче) или FailOrder (при смерти/таймере)
			}

			// Включаем кнопку, зелёный экран
			EnableButton();
			SetCabinState(ECabinState::Ready);
			SetScreenText(TEXT("\u0413\u041E\u0422\u041E\u0412\u041E"), 0.4f);
			SetTimerTextColor(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f));
		}
	}
}

// ============================================================
// ДВЕРНАЯ АНИМАЦИЯ
// ============================================================

void ATravelCabin::SetupDoorTimeline()
{
	if (!DoorAnimCurve)
	{
		UE_LOG(LogCabin, Warning, TEXT("Cabin: DoorAnimCurve is not set! Door animation will not work."));
		return;
	}

	FOnTimelineFloat UpdateDelegate;
	UpdateDelegate.BindDynamic(this, &ATravelCabin::OnDoorTimelineUpdate);
	DoorTimeline.AddInterpFloat(DoorAnimCurve, UpdateDelegate);

	FOnTimelineEvent FinishedDelegate;
	FinishedDelegate.BindDynamic(this, &ATravelCabin::OnDoorTimelineFinished);
	DoorTimeline.SetTimelineFinishedFunc(FinishedDelegate);
}

void ATravelCabin::OnDoorTimelineUpdate(float Alpha)
{
	// Alpha: 0 = закрыты, 1 = открыты
	if (DoorL)
		DoorL->SetRelativeLocation(FMath::Lerp(DoorLClosedPos, DoorLOpenPos, Alpha));
	if (DoorR)
		DoorR->SetRelativeLocation(FMath::Lerp(DoorRClosedPos, DoorROpenPos, Alpha));
}

void ATravelCabin::OnDoorTimelineFinished()
{
	// Двери закончили движение
	// Если двери закрывались (Reverse), проверяем PendingAction
	if (PendingAction == EPendingAction::None)
	{
		// Двери полностью открыты — выключаем блокер
		if (CachedBlocker) CachedBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	EPendingAction Action = PendingAction;
	PendingAction = EPendingAction::None;

	switch (Action)
	{
	case EPendingAction::TeleportOut:
	case EPendingAction::TeleportHome:
		{
			// Пауза → телепорт
			FTimerHandle TeleportHandle;
			GetWorldTimerManager().SetTimer(TeleportHandle, this,
				&ATravelCabin::PerformTeleport, TeleportDelay, false);
		}
		break;

	case EPendingAction::PlayerAbandoned:
		{
			// Двери закрылись, игрока нет внутри — запускаем эффект сингулярности
			if (bIsPlayerInside)
			{
				// На всякий случай: игрок всё-таки успел запрыгнуть
				UE_LOG(LogCabin, Warning, TEXT("Cabin: PlayerAbandoned but player IS inside! Doing rescue instead."));
				SaveCabinToStorage();
				if (UGI_DimensionRunner* GI = GetDimensionRunnerGI())
				{
					GI->SetCabinStatus(false);
					GI->SetOverheatStatus(true);
				}
				SetCabinState(ECabinState::Returning);
				TargetLevelName = GetBaseLevelName();
				PendingAction = EPendingAction::TeleportHome;
				// Двери уже закрыты, просто телепортируемся
				FTimerHandle TeleportHandle;
				GetWorldTimerManager().SetTimer(TeleportHandle, this,
					&ATravelCabin::PerformTeleport, TeleportDelay, false);
			}
			else
			{
				// Игрок не успел — C++ запускает эффект и сжатие
				UE_LOG(LogCabin, Warning, TEXT("Cabin: Player LEFT BEHIND! Starting singularity."));
				StartSingularityAndShrink();
				OnPlayerLeftBehind(); // Всё что ещё осталось в BP (PrintString DEFEAT и т.d.)
			}
		}
		break;

	default:
		break;
	}
}

void ATravelCabin::OpenDoors()
{
	UE_LOG(LogCabin, Log, TEXT("Cabin: Opening doors."));
	if (CachedBlocker) CachedBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DoorTimeline.Play(); // 0→1
}

void ATravelCabin::CloseDoors()
{
	UE_LOG(LogCabin, Log, TEXT("Cabin: Closing doors."));
	if (CachedBlocker) CachedBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DoorTimeline.Reverse(); // 1→0
}

// ============================================================
// OVERLAP ТРИГГЕРА
// ============================================================

void ATravelCabin::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (OtherActor && OtherActor == PlayerChar)
	{
		bIsPlayerInside = true;
		UE_LOG(LogCabin, Log, TEXT("Cabin: Player ENTERED trigger."));
	}
}

void ATravelCabin::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (OtherActor && OtherActor == PlayerChar)
	{
		bIsPlayerInside = false;
		UE_LOG(LogCabin, Log, TEXT("Cabin: Player LEFT trigger."));
	}
}

// ============================================================
// ПОИСК КОМПОНЕНТОВ BLUEPRINT ПО ИМЕНИ
// ============================================================

void ATravelCabin::FindBlueprintComponents()
{
	TArray<UActorComponent*> AllComps;
	GetComponents(AllComps);

	for (UActorComponent* Comp : AllComps)
	{
		FString Name = Comp->GetName();

		if (Name.Contains(TEXT("Door_L")) || Name.Contains(TEXT("DoorL")))
			DoorL = Cast<UStaticMeshComponent>(Comp);
		else if (Name.Contains(TEXT("Door_R")) || Name.Contains(TEXT("DoorR")))
			DoorR = Cast<UStaticMeshComponent>(Comp);
		else if (Name.Contains(TEXT("Button_Load")) || Name.Contains(TEXT("ButtonLoad")))
			CachedButtonLoad = Cast<UPrimitiveComponent>(Comp);
		else if (Name.Contains(TEXT("Button_Select")) || Name.Contains(TEXT("ButtonSelect")))
			CachedButtonSelect = Cast<UPrimitiveComponent>(Comp);
		else if (Name.Contains(TEXT("Button_Unload")) || Name.Contains(TEXT("ButtonUnload")))
			CachedButtonUnload = Cast<UPrimitiveComponent>(Comp);
		else if (Name.Contains(TEXT("Button")))
			CachedButton = Cast<UStaticMeshComponent>(Comp);
		else if (Name.Contains(TEXT("InventoryTrig")))
			CachedInventoryTrig = Cast<UPrimitiveComponent>(Comp);
		else if (Name.Contains(TEXT("Trigger")))
			CachedTrigger = Cast<UPrimitiveComponent>(Comp);
		else if (Name.Contains(TEXT("SlotScreen")))
			CachedSlotScreen = Cast<UWidgetComponent>(Comp);
		else if (Name.Contains(TEXT("Screen")))
			CachedScreen = Cast<UWidgetComponent>(Comp);
		else if (Name.Contains(TEXT("Blocker")))
			CachedBlocker = Cast<UPrimitiveComponent>(Comp);
		else if (Name.Contains(TEXT("Singularity")))
		{
			CachedSingularityPP = Cast<UPostProcessComponent>(Comp);
			if (CachedSingularityPP)
				CachedSingularityPP->SetVisibility(false); // выключаем до срабатывания эффекта
		}
	}

	// Логируем результат
	UE_LOG(LogCabin, Log, TEXT("Cabin: DoorL=%s DoorR=%s Button=%s Trigger=%s Screen=%s"),
		DoorL ? TEXT("OK") : TEXT("MISSING"),
		DoorR ? TEXT("OK") : TEXT("MISSING"),
		CachedButton ? TEXT("OK") : TEXT("MISSING"),
		CachedTrigger ? TEXT("OK") : TEXT("MISSING"),
		CachedScreen ? TEXT("OK") : TEXT("MISSING"));
}

// ============================================================
// ЭКРАН КАБИНЫ
// ============================================================

UTextBlock* ATravelCabin::GetTimerTextWidget() const
{
	if (!CachedScreen) return nullptr;

	UUserWidget* Widget = CachedScreen->GetUserWidgetObject();
	if (!Widget) return nullptr;

	// Ищем свойство TimerText на WBP_CabinScreen через Reflection
	FProperty* Prop = Widget->GetClass()->FindPropertyByName(TEXT("TimerText"));
	if (!Prop) return nullptr;

	FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop);
	if (!ObjProp) return nullptr;

	UObject* TextObj = ObjProp->GetObjectPropertyValue(
		ObjProp->ContainerPtrToValuePtr<void>(Widget));
	return Cast<UTextBlock>(TextObj);
}

void ATravelCabin::UpdateScreenText(int32 Seconds)
{
	int32 Minutes = FMath::Max(0, Seconds) / 60;
	int32 Secs = FMath::Max(0, Seconds) % 60;
	FString TimeStr = FString::Printf(TEXT("%02d:%02d"), Minutes, Secs);
	SetScreenText(TimeStr);
}

void ATravelCabin::SetScreenText(const FString& Text, float FontScale)
{
	UTextBlock* TimerText = GetTimerTextWidget();
	if (TimerText)
	{
		// Кэшируем оригинальный размер шрифта при первом вызове
		if (CachedOriginalFontSize == 0)
		{
			CachedOriginalFontSize = TimerText->GetFont().Size;
		}

		TimerText->SetText(FText::FromString(Text));
		TimerText->SetJustification(ETextJustify::Center);
		TimerText->SetAutoWrapText(true);

		// Масштабируем шрифт
		FSlateFontInfo FontInfo = TimerText->GetFont();
		FontInfo.Size = FMath::RoundToInt32(CachedOriginalFontSize * FontScale);
		TimerText->SetFont(FontInfo);
	}
}

void ATravelCabin::SetTimerTextColor(const FLinearColor& Color)
{
	UTextBlock* TimerText = GetTimerTextWidget();
	if (TimerText)
	{
		TimerText->SetColorAndOpacity(FSlateColor(Color));
	}
}

// ============================================================
// ТАЙМЕР
// ============================================================

void ATravelCabin::StartCabinTimer()
{
	GetWorldTimerManager().SetTimer(CabinTimerHandle, this,
		&ATravelCabin::TickCabinTimer, 1.0f, true);
}

void ATravelCabin::StopCabinTimer()
{
	GetWorldTimerManager().ClearTimer(CabinTimerHandle);
}

// ============================================================
// КНОПКА (ButtonMesh collision toggle)
// ============================================================

void ATravelCabin::EnableButton()
{
	if (CachedButton)
	{
		CachedButton->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		UE_LOG(LogCabin, Log, TEXT("Cabin: Button ENABLED."));
	}
}

void ATravelCabin::DisableButton()
{
	if (CachedButton)
	{
		CachedButton->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UE_LOG(LogCabin, Log, TEXT("Cabin: Button DISABLED."));
	}
}

// ============================================================
// СОСТОЯНИЕ
// ============================================================

void ATravelCabin::SetCabinState(ECabinState NewState)
{
	CabinState = NewState;
	OnCabinStateChanged(NewState);
	UE_LOG(LogCabin, Log, TEXT("Cabin: State → %d"), (int32)NewState);
}

// ============================================================
// GameInstance — прямой доступ (C++ вместо Reflection)
// ============================================================

UGI_DimensionRunner* ATravelCabin::GetDimensionRunnerGI() const
{
	UGameInstance* GI = GetGameInstance();
	UGI_DimensionRunner* DRGI = Cast<UGI_DimensionRunner>(GI);

	if (!DRGI && GI)
	{
		UE_LOG(LogCabin, Warning,
			TEXT("Cabin: GameInstance is NOT UGI_DimensionRunner! Class: %s. Set it in Project Settings."),
			*GI->GetClass()->GetName());
	}
	return DRGI;
}

// ============================================================
void ATravelCabin::PerformTeleport()
{
	UE_LOG(LogCabin, Log, TEXT("Cabin: TELEPORTING to %s"), *TargetLevelName.ToString());
	
	// Финальное сохранение инвентаря перед прыжком (на всякий случай)
	SaveCabinToStorage();
	
	UGameplayStatics::OpenLevel(this, TargetLevelName);
}

// ============================================================
// СИНГУЛЯРНОСТЬ + СЖАТИЕ КАБИНЫ
// ============================================================

void ATravelCabin::StartSingularityAndShrink()
{
	if (!CachedSingularityPP)
	{
		UE_LOG(LogCabin, Warning, TEXT("Cabin: SingularityPP NOT FOUND in BP components!"));
		// Даже без PostProcess — запускаем сжатие
	}

	if (SingularityMaterial && CachedSingularityPP)
	{
		CachedSingularityPP->SetVisibility(true);

		SingularityDMI = UMaterialInstanceDynamic::Create(SingularityMaterial, this);
		if (SingularityDMI)
		{
			SingularityDMI->SetScalarParameterValue(TEXT("Intensity"), 0.0f);
			CachedSingularityPP->AddOrUpdateBlendable(SingularityDMI, 1.0f);
		}
	}
	else
	{
		UE_LOG(LogCabin, Warning, TEXT("Cabin: SingularityMaterial not set in Class Defaults > Cabin|Singularity!"));
	}

	OriginalScale = GetActorScale3D();

	// Вспышка — радиус в 3x от размера кабины чтобы покрывать её целиком
	if (FlashMaxIntensity > 0.f)
	{
		FlashLight = NewObject<UPointLightComponent>(this, TEXT("SingularityFlash"));
		FlashLight->RegisterComponent();
		FlashLight->AttachToComponent(GetRootComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		FlashLight->SetIntensity(0.f);
		FlashLight->SetLightColor(FlashColor);
		FlashLight->bUseInverseSquaredFalloff = false; // линейное затухание = плавная вспышка
		// Берём макс. компонент масштаба * ориентировочный радиус кабины * 3
		const float CabinRadius = OriginalScale.GetMax() * 300.f;
		FlashLight->SetAttenuationRadius(CabinRadius * 3.f);
		FlashLight->SetCastShadows(false); // тени от временного света не нужны
	}

	SingularityElapsed = 0.0f;
	UVUpdateCounter = 0;
	bSingularityActive = true;
}

void ATravelCabin::TickSingularity(float DeltaTime)
{
	SingularityElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(SingularityElapsed / FMath::Max(SingularityDuration, 0.01f), 0.0f, 1.0f);

	// ─── ДВЕ ФАЗЫ ─────────────────────────────────────────────────────────────
	// Фаза 1 (0 → ExpandFraction): кабина СЛЕГКА РАСТЁТ, искажение слабое
	// Фаза 2 (ExpandFraction → 1): кабина РЕЗКО СЖИМАЕТСЯ, искажение взрывается
	// ──────────────────────────────────────────────────────────────────────────

	const float SafeExpand = FMath::Clamp(ExpandFraction, 0.01f, 0.99f);
	FVector TargetScale;
	float IntensityAlpha; // отдельный прогресс для PP-искажения

	if (Alpha < SafeExpand)
	{
		// --- Фаза 1: плавное расширение ---
		const float ExpandAlpha = Alpha / SafeExpand; // 0 → 1
		
		// Ease In-Out: мягко начинает расти, мягко останавливается
		const float EasedExpand = FMath::InterpEaseInOut(0.0f, 1.0f, ExpandAlpha, 2.0f);
		
		TargetScale = OriginalScale * FMath::Lerp(1.0f, ExpandScale, EasedExpand);
		
		// Искажение очень слабое: растёт плавно до 10%
		IntensityAlpha = EasedExpand * 0.1f;
	}
	else
	{
		// --- Фаза 2: экспоненциальное сжатие (взрыв внутрь) ---
		const float ShrinkAlpha = (Alpha - SafeExpand) / (1.0f - SafeExpand); // 0 → 1
		
		// Ease In (степень 4): долго удерживает пиковую форму, затем РЕЗКО схлопывается
		const float EasedShrink = FMath::InterpEaseIn(0.0f, 1.0f, ShrinkAlpha, 4.0f);
		
		const FVector PeakScale = OriginalScale * ExpandScale;
		TargetScale = FMath::Lerp(PeakScale, FVector::ZeroVector, EasedShrink);
		
		// Искажение (и вспышка) взрываются по той же крутой кривой
		IntensityAlpha = FMath::Lerp(0.1f, 1.0f, EasedShrink);
	}

	SetActorScale3D(TargetScale);

	// Вспышка следует за той же кривой
	if (FlashLight)
		FlashLight->SetIntensity(IntensityAlpha * FlashMaxIntensity);

	// PostProcess искажение
	if (SingularityDMI)
	{
		SingularityDMI->SetScalarParameterValue(TEXT("Intensity"), IntensityAlpha * SingularityMaxIntensity);

		// CabinUV — обновляем не каждый кадр (ProjectWorldToScreen дорогой вызов)
		++UVUpdateCounter;
		if (UVUpdateCounter >= 3)
		{
			UVUpdateCounter = 0;
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				FVector2D ScreenPos;
				int32 W, H;
				PC->GetViewportSize(W, H);
				if (W > 0 && H > 0 &&
					PC->ProjectWorldLocationToScreen(GetActorLocation(), ScreenPos, false))
				{
					SingularityDMI->SetVectorParameterValue(TEXT("CabinUV"),
						FLinearColor(ScreenPos.X / W, ScreenPos.Y / H, 0.f, 0.f));
				}
			}
		}
	}

	if (Alpha >= 1.0f)
		FinishSingularity();
}

void ATravelCabin::FinishSingularity()
{
	bSingularityActive = false;
	Destroy();
}

// ============================================================
// ИНВЕНТАРЬ КАБИНЫ — КНОПКИ + ЭКРАН
// ============================================================

bool ATravelCabin::TryHandleButtonInteract(AActor* Interactor)
{
	if (!CabinInventory) return false;
	if (!CachedButtonLoad && !CachedButtonSelect && !CachedButtonUnload) return false;

	ACharacter* Char = Cast<ACharacter>(Interactor);
	if (!Char) return false;

	APlayerController* PC = Cast<APlayerController>(Char->GetController());
	if (!PC) return false;

	// Повторный трейс (геометрический)
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	FVector TraceEnd = CamLoc + CamRot.Vector() * 300.0f;
	UPrimitiveComponent* BestButton = nullptr;
	float BestDist = 15.0f; // Прицеливание: прощает промах до 15 см от центра луча (уточнили для точности)

	UPrimitiveComponent* Buttons[] = { CachedButtonLoad, CachedButtonSelect, CachedButtonUnload };

	for (UPrimitiveComponent* Btn : Buttons)
	{
		if (!Btn) continue;
		FVector BtnLoc = Btn->GetComponentLocation();
		
		if (FVector::Dist(CamLoc, BtnLoc) > 350.0f) continue;
		
		FVector PointOnLine = FMath::ClosestPointOnSegment(BtnLoc, CamLoc, TraceEnd);
		float DistFromLine = FVector::Dist(PointOnLine, BtnLoc);

		if (DistFromLine < BestDist)
		{
			BestDist = DistFromLine;
			BestButton = Btn;
		}
	}

	if (BestButton == CachedButtonLoad)
	{
		CabinInventory->LoadItem();
		UE_LOG(LogCabin, Log, TEXT("Cabin: Button LOAD pressed."));
		return true;
	}
	if (BestButton == CachedButtonSelect)
	{
		CabinInventory->SelectNextSlot();
		UE_LOG(LogCabin, Log, TEXT("Cabin: Button SELECT pressed."));
		return true;
	}
	if (BestButton == CachedButtonUnload)
	{
		CabinInventory->UnloadItem();
		UE_LOG(LogCabin, Log, TEXT("Cabin: Button UNLOAD pressed."));
		return true;
	}

	return false;
}

bool ATravelCabin::IsLookingAtMainButton(AActor* Interactor) const
{
	if (!CachedButton) return true; // Нет кнопки — разрешаем (обратная совместимость)

	const ACharacter* Char = Cast<ACharacter>(Interactor);
	if (!Char) return false;

	APlayerController* PC = Cast<APlayerController>(Char->GetController());
	if (!PC) return false;

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Interactor);

	TArray<FHitResult> Hits;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(15.0f);

	if (GetWorld()->SweepMultiByChannel(Hits, CamLoc, CamLoc + CamRot.Vector() * 300.0f,
		FQuat::Identity, ECC_Visibility, Sphere, Params))
	{
		for (const FHitResult& HitResult : Hits)
		{
			if (HitResult.GetComponent() == CachedButton)
			{
				return true;
			}
		}
	}

	return false;
}

bool ATravelCabin::IsLookingAtAnyInteractable(AActor* Interactor) const
{
	if (IsLookingAtMainButton(Interactor)) return true;

	if (!CabinInventory) return false;
	if (!CachedButtonLoad && !CachedButtonSelect && !CachedButtonUnload) return false;

	const ACharacter* Char = Cast<ACharacter>(Interactor);
	if (!Char) return false;

	const APlayerController* PC = Cast<APlayerController>(Char->GetController());
	if (!PC) return false;

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	FVector TraceEnd = CamLoc + CamRot.Vector() * 300.0f;
	float BestDist = 15.0f; // Прицеливание: прощает промах до 15 см от центра луча

	UPrimitiveComponent* Buttons[] = { CachedButtonLoad, CachedButtonSelect, CachedButtonUnload };

	for (UPrimitiveComponent* Btn : Buttons)
	{
		if (!Btn) continue;
		FVector BtnLoc = Btn->GetComponentLocation();
		
		if (FVector::Dist(CamLoc, BtnLoc) > 350.0f) continue;
		
		FVector PointOnLine = FMath::ClosestPointOnSegment(BtnLoc, CamLoc, TraceEnd);
		float DistFromLine = FVector::Dist(PointOnLine, BtnLoc);

		if (DistFromLine < BestDist)
		{
			return true;
		}
	}

	return false;
}

void ATravelCabin::OnInventoryChangedHandler()
{
	UpdateSlotScreen();
	SaveCabinToStorage();
}

void ATravelCabin::UpdateSlotScreen()
{
	UCabinSlotWidget* SlotWidget = GetSlotWidget();
	if (!SlotWidget || !CabinInventory) return;

	const int32 Idx = CabinInventory->GetActiveSlotIndex();
	const FCabinSlot Slot = CabinInventory->GetSlotInfo(Idx);
	SlotWidget->RefreshSlotDisplay(Slot, Idx, CabinInventory->GetSlotCount());
}

UCabinSlotWidget* ATravelCabin::GetSlotWidget() const
{
	if (!CachedSlotScreen) return nullptr;
	return Cast<UCabinSlotWidget>(CachedSlotScreen->GetUserWidgetObject());
}

// ============================================================
// СОХРАНЕНИЕ / ВОССТАНОВЛЕНИЕ ИНВЕНТАРЯ КАБИНЫ
// ============================================================

void ATravelCabin::SaveCabinToStorage()
{
	if (!CabinInventory) return;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UCabinStorageSubsystem* Storage = GI->GetSubsystem<UCabinStorageSubsystem>();
	if (!Storage) return;

	Storage->SavedSlots.Empty();
	Storage->SavedActiveSlot = CabinInventory->GetActiveSlotIndex();

	for (int32 i = 0; i < CabinInventory->GetSlotCount(); i++)
	{
		FCabinSlot Slot = CabinInventory->GetSlotInfo(i);
		FSavedCabinSlot Saved;
		Saved.bOccupied = Slot.bOccupied;

		if (Slot.bOccupied && Slot.Artifact)
		{
			Saved.ArtifactClass = Slot.Artifact->GetClass();
			Saved.ItemID = Slot.Artifact->GetItemID();
			Saved.ItemName = Slot.Artifact->GetItemName();
			Saved.ItemHP = Slot.Artifact->ItemHP;
		}

		Storage->SavedSlots.Add(Saved);
	}

	UE_LOG(LogCabin, Log, TEXT("Cabin: Saved %d inventory slots to storage."),
		CabinInventory->GetOccupiedCount());
}

void ATravelCabin::RestoreCabinFromStorage()
{
	if (!CabinInventory) return;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UCabinStorageSubsystem* Storage = GI->GetSubsystem<UCabinStorageSubsystem>();
	if (!Storage || !Storage->HasSavedData()) return;

	int32 RestoredCount = 0;

	for (int32 i = 0; i < Storage->SavedSlots.Num() && i < CabinInventory->GetSlotCount(); i++)
	{
		const FSavedCabinSlot& Saved = Storage->SavedSlots[i];
		if (!Saved.bOccupied || !Saved.ArtifactClass) continue;

		// Спавним артефакт (над кабиной, он всё равно будет скрыт)
		FVector SpawnLoc = GetActorLocation() + FVector(0.f, 0.f, 500.f);
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AArtifactBase* NewArtifact = GetWorld()->SpawnActor<AArtifactBase>(
			Saved.ArtifactClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);

		if (NewArtifact)
		{
			// Данные из DataTable загрузятся автоматически в BeginPlay,
			// но нам нужно восстановить уникальные свойства
			NewArtifact->ItemID = Saved.ItemID;
			NewArtifact->ItemName = Saved.ItemName;
			NewArtifact->ItemHP = Saved.ItemHP;

			CabinInventory->StoreArtifact(i, NewArtifact);
			RestoredCount++;
		}
	}

	// Восстановить активный слот
	CabinInventory->SetActiveSlotIndex(Storage->SavedActiveSlot);

	// Очистить сохранение
	Storage->ClearSavedData();

	if (RestoredCount > 0)
	{
		UE_LOG(LogCabin, Log, TEXT("Cabin: Restored %d artifacts from storage."), RestoredCount);
		CabinInventory->OnInventoryChanged.Broadcast();
	}
}
