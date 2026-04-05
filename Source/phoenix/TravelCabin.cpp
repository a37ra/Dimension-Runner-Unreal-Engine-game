// TravelCabin.cpp — Полная логика кабины телепортации
#include "TravelCabin.h"
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

	// 5. Спрашиваем GI_Phoenix: где мы?
	bool bOnMission = GI_CheckCabinStatus();
	bool bOverheated = GI_CheckOverheatStatus();

	if (bOnMission)
	{
		// === ЭКСПЕДИЦИЯ: мы на L_WorldA ===
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

		// Сохраняем новый лут
		SavePlayerInventoryToGI();
		SaveCabinToStorage();
		
		// Снимаем статус миссии, ставим перегрев
		GI_SetCabinStatus(false);
		GI_SetOverheatStatus(true);
		
		// Назад в Лабораторию
		SetCabinState(ECabinState::Returning);
		TargetLevelName = TEXT("L_Laboratory");
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

	// Сохраняем инвентарь игрока в GI_Phoenix
	SavePlayerInventoryToGI();
	SaveCabinToStorage();

	// Ставим статус "На миссии" в GI
	GI_SetCabinStatus(true);

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
			UE_LOG(LogCabin, Log, TEXT("Cabin: TIME IS UP. Player inside: %s"),
				bIsPlayerInside ? TEXT("YES") : TEXT("NO"));

			if (bIsPlayerInside)
			{
				// === СЦЕНАРИЙ Б: СПАСЕНИЕ ===
				// Сохраняем новый лут
				SavePlayerInventoryToGI();
				SaveCabinToStorage();
				// Снимаем статус миссии, ставим перегрев
				GI_SetCabinStatus(false);
				GI_SetOverheatStatus(true);
				// Закрываем двери → телепорт домой
				SetCabinState(ECabinState::Returning);
				
				// ПРИНУДИТЕЛЬНО возвращаемся в Лабораторию!
				TargetLevelName = TEXT("L_Laboratory");
				
				PendingAction = EPendingAction::TeleportHome;
				CloseDoors();
			}
			else
			{
				// === СЦЕНАРИЙ А: ИГРОК НЕ УСПЕЛ ===
				// Даем команду закрыть двери и ставим смерть.
				GI_SetCabinStatus(false);
				GI_SetOverheatStatus(true); // кабина возвращается пустой и перегретой
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
			GI_SetOverheatStatus(false);

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
				SavePlayerInventoryToGI();
				SaveCabinToStorage();
				GI_SetCabinStatus(false);
				GI_SetOverheatStatus(true);
				SetCabinState(ECabinState::Returning);
				TargetLevelName = TEXT("L_Laboratory");
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
// GI_PHOENIX — взаимодействие через UE Reflection
// ============================================================

UObject* ATravelCabin::GetGIPhoenix() const
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	if (!GI) return nullptr;

	// GI_Phoenix — Blueprint класс. Его скомпилированный класс = "GI_Phoenix_C"
	if (GI->GetClass()->GetName().Contains(TEXT("GI_Phoenix")))
		return GI;

	UE_LOG(LogCabin, Warning, TEXT("Cabin: GameInstance is NOT GI_Phoenix! Class: %s"),
		*GI->GetClass()->GetName());
	return nullptr;
}

bool ATravelCabin::GI_CheckCabinStatus()
{
	UObject* GI = GetGIPhoenix();
	if (!GI) return false;

	UFunction* Func = GI->GetClass()->FindFunctionByName(TEXT("CheckCabinStatus"));
	if (Func)
	{
		struct { bool IsActive; } Params;
		Params.IsActive = false;
		GI->ProcessEvent(Func, &Params);
		return Params.IsActive;
	}

	// Фолбэк: ищем переменные напрямую (разные возможные имена)
	TArray<FName> PossibleNames = { TEXT("CabinStatus"), TEXT("bCabinStatus"), TEXT("bIsActivated"), TEXT("IsActivated") };
	for (FName PropName : PossibleNames)
	{
		FBoolProperty* Prop = FindFProperty<FBoolProperty>(GI->GetClass(), PropName);
		if (Prop)
		{
			return Prop->GetPropertyValue_InContainer(GI);
		}
	}

	UE_LOG(LogCabin, Warning, TEXT("Cabin: GI CabinStatus not found!"));
	return false;
}

bool ATravelCabin::GI_CheckOverheatStatus()
{
	UObject* GI = GetGIPhoenix();
	if (!GI) return false;

	UFunction* Func = GI->GetClass()->FindFunctionByName(TEXT("CheckOverheatStatus"));
	if (Func)
	{
		struct { bool IsOverheated; } Params;
		Params.IsOverheated = false;
		GI->ProcessEvent(Func, &Params);
		return Params.IsOverheated;
	}

	// Фолбэк
	TArray<FName> PossibleNames = { TEXT("OverheatStatus"), TEXT("bOverheatStatus"), TEXT("Overheat"), TEXT("bOverheat"), TEXT("bIsOverheated") };
	for (FName PropName : PossibleNames)
	{
		FBoolProperty* Prop = FindFProperty<FBoolProperty>(GI->GetClass(), PropName);
		if (Prop)
		{
			return Prop->GetPropertyValue_InContainer(GI);
		}
	}

	UE_LOG(LogCabin, Warning, TEXT("Cabin: GI OverheatStatus not found!"));
	return false;
}

void ATravelCabin::GI_SetCabinStatus(bool bActive)
{
	UObject* GI = GetGIPhoenix();
	if (!GI) return;

	UFunction* Func = GI->GetClass()->FindFunctionByName(TEXT("SetCabinStatus"));
	if (Func)
	{
		struct { bool bIsActive; } Params;
		Params.bIsActive = bActive;
		GI->ProcessEvent(Func, &Params);
		return;
	}

	// Фолбэк
	TArray<FName> PossibleNames = { TEXT("CabinStatus"), TEXT("bCabinStatus"), TEXT("bIsActivated"), TEXT("IsActivated") };
	for (FName PropName : PossibleNames)
	{
		FBoolProperty* Prop = FindFProperty<FBoolProperty>(GI->GetClass(), PropName);
		if (Prop)
		{
			Prop->SetPropertyValue_InContainer(GI, bActive);
			return;
		}
	}
	UE_LOG(LogCabin, Warning, TEXT("Cabin: Cannot set CabinStatus! No property found."));
}

void ATravelCabin::GI_SetOverheatStatus(bool bOverheated)
{
	UObject* GI = GetGIPhoenix();
	if (!GI) return;

	UFunction* Func = GI->GetClass()->FindFunctionByName(TEXT("SetOverheatStatus"));
	if (Func)
	{
		struct { bool bIsOverheated; } Params;
		Params.bIsOverheated = bOverheated;
		GI->ProcessEvent(Func, &Params);
		return;
	}

	// Фолбэк
	TArray<FName> PossibleNames = { TEXT("OverheatStatus"), TEXT("bOverheatStatus"), TEXT("Overheat"), TEXT("bOverheat"), TEXT("bIsOverheated") };
	for (FName PropName : PossibleNames)
	{
		FBoolProperty* Prop = FindFProperty<FBoolProperty>(GI->GetClass(), PropName);
		if (Prop)
		{
			Prop->SetPropertyValue_InContainer(GI, bOverheated);
			return;
		}
	}
	UE_LOG(LogCabin, Error, TEXT("Cabin: Cannot set overheat status! No function or property found."));
}

// ============================================================
// СОХРАНЕНИЕ ИНВЕНТАРЯ
// ============================================================

void ATravelCabin::SavePlayerInventoryToGI()
{
	UObject* GI = GetGIPhoenix();
	if (!GI) return;

	ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!PlayerChar) return;

	// Ищем BP_InventoryComponent на игроке
	UActorComponent* InvComp = nullptr;
	for (UActorComponent* Comp : PlayerChar->GetComponents())
	{
		if (Comp && (Comp->GetClass()->GetName().Contains(TEXT("Inventory")) || Comp->GetClass()->GetName().Contains(TEXT("Inv"))))
		{
			InvComp = Comp;
			break;
		}
	}

	if (!InvComp)
	{
		UE_LOG(LogCabin, Warning, TEXT("Cabin: Inventory Component NOT FOUND on player! Check if it has 'Inventory' in its name."));
		return;
	}

	// Копируем свойства через Reflection с перебором возможных имён
	int32 CopiedCount = 0;
	auto CopyProperty = [&](const TArray<FName>& SrcNames, const TArray<FName>& DstNames)
	{
		FProperty* Src = nullptr;
		FProperty* Dst = nullptr;
		FName FinalSrc, FinalDst;

		for (FName S : SrcNames) { Src = InvComp->GetClass()->FindPropertyByName(S); if (Src) { FinalSrc = S; break; } }
		for (FName D : DstNames) { Dst = GI->GetClass()->FindPropertyByName(D); if (Dst) { FinalDst = D; break; } }

		if (Src && Dst)
		{
			void* SrcData = Src->ContainerPtrToValuePtr<void>(InvComp);
			void* DstData = Dst->ContainerPtrToValuePtr<void>(GI);
			if (SrcData && DstData)
			{
				Src->CopyCompleteValue(DstData, SrcData);
				CopiedCount++;
				UE_LOG(LogCabin, Log, TEXT("Cabin: Copied %s → %s"), *FinalSrc.ToString(), *FinalDst.ToString());
			}
		}
		else
		{
			UE_LOG(LogCabin, Warning, TEXT("Cabin: Property copy failed! Checked sources (e.g. %s) -> Dests (e.g. %s)"),
				SrcNames.Num() > 0 ? *SrcNames[0].ToString() : TEXT("None"),
				DstNames.Num() > 0 ? *DstNames[0].ToString() : TEXT("None"));
		}
	};

	CopyProperty(
		{TEXT("InventorySlots"), TEXT("Inventory"), TEXT("Items")},
		{TEXT("SavedInventory"), TEXT("SavedInventorySlots"), TEXT("Inventory")}
	);
	CopyProperty(
		{TEXT("HotbarSlot1"), TEXT("HotbarSlot_1"), TEXT("Hotbar1")},
		{TEXT("Saved_HotbarSlot1"), TEXT("SavedHotbarSlot1"), TEXT("Saved_HotbarSlot_1")}
	);
	CopyProperty(
		{TEXT("HotbarCount1"), TEXT("HotbarCount_1"), TEXT("Hotbar1Count")},
		{TEXT("Saved_HotbarCount1"), TEXT("SavedHotbarCount1"), TEXT("Saved_HotbarCount_1")}
	);
	CopyProperty(
		{TEXT("HotbarSlot2"), TEXT("HotbarSlot_2"), TEXT("Hotbar2")},
		{TEXT("Saved_HotbarSlot2"), TEXT("SavedHotbarSlot2"), TEXT("Saved_HotbarSlot_2")}
	);
	CopyProperty(
		{TEXT("HotbarCount2"), TEXT("HotbarCount_2"), TEXT("Hotbar2Count")},
		{TEXT("Saved_HotbarCount2"), TEXT("SavedHotbarCount2"), TEXT("Saved_HotbarCount_2")}
	);

	// Вызываем SetInventorySaved() на GI (как в BP_Portal)
	UFunction* SetSavedFunc = GI->GetClass()->FindFunctionByName(TEXT("SetInventorySaved"));
	if (SetSavedFunc)
	{
		GI->ProcessEvent(SetSavedFunc, nullptr);
		UE_LOG(LogCabin, Log, TEXT("Cabin: Called SetInventorySaved() on GI."));
	}

	UE_LOG(LogCabin, Log, TEXT("Cabin: Inventory saved to GI_Phoenix! Copied %d properties."), CopiedCount);
}

// ============================================================
// ТЕЛЕПОРТ
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

	// Повторный трейс чтобы определить какую кнопку нажали
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Interactor);

	// Используем SweepMulti со сферой, чтобы попадать по мелким кнопкам было легко
	// даже если луч чуть-чуть задевает стену кабины
	TArray<FHitResult> Hits;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(15.0f); // 15см радиус "толстого луча"

	if (!GetWorld()->SweepMultiByChannel(Hits, CamLoc, CamLoc + CamRot.Vector() * 300.0f,
		FQuat::Identity, ECC_Visibility, Sphere, Params))
	{
		return false;
	}

	for (const FHitResult& HitResult : Hits)
	{
		UPrimitiveComponent* HitComp = HitResult.GetComponent();
		if (!HitComp) continue;

		if (HitComp == CachedButtonLoad)
		{
			CabinInventory->LoadItem();
			UE_LOG(LogCabin, Log, TEXT("Cabin: Button LOAD pressed."));
			return true;
		}
		if (HitComp == CachedButtonSelect)
		{
			CabinInventory->SelectNextSlot();
			UE_LOG(LogCabin, Log, TEXT("Cabin: Button SELECT pressed."));
			return true;
		}
		if (HitComp == CachedButtonUnload)
		{
			CabinInventory->UnloadItem();
			UE_LOG(LogCabin, Log, TEXT("Cabin: Button UNLOAD pressed."));
			return true;
		}
	}

	// Ни одна инвентарная кнопка не совпала, но трейс попал на компонент кабины —
	// НЕ прокидываем в основную логику, чтобы не закрыть кабину
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
			// но нам нужно восстановить HP
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
