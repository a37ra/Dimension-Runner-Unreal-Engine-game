// TravelCabin.cpp — Полная логика кабины телепортации
#include "TravelCabin.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Curves/CurveFloat.h"
#include "Sound/SoundBase.h"

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
			SetScreenText(TEXT("\u0413\u041E\u0422\u041E\u0412\u041E"));
			SetTimerTextColor(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f)); // Зелёный
		}, 0.5f, false); // маленькая задержка для инициализации виджета
	}
}

// ============================================================
// TICK — обновляем Timeline
// ============================================================

void ATravelCabin::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	DoorTimeline.TickTimeline(DeltaTime);
}

// ============================================================
// INTERACT — игрок нажал кнопку
// ============================================================

void ATravelCabin::OnInteract(AActor* Interactor)
{
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
			SetScreenText(TEXT("\u0413\u041E\u0422\u041E\u0412\u041E"));
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
			// Игрок не успел — вызываем BP-событие
			UE_LOG(LogCabin, Warning, TEXT("Cabin: Player LEFT BEHIND!"));
			OnPlayerLeftBehind();
			// ВНИМАНИЕ: ЗДЕСЬ НЕТ ТЕЛЕПОРТА (OpenLevel). 
			// Кабина просто закроется, и BP скрипт OnPlayerLeftBehind должен убить персонажа
			// или показать экран Game Over. Мы никуда не тепаем текущий уровень,
			// иначе Unreal перезагрузит WorldA вместе с игроком.
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
		else if (Name.Contains(TEXT("Button")))
			CachedButton = Cast<UStaticMeshComponent>(Comp);
		else if (Name.Contains(TEXT("Trigger")))
			CachedTrigger = Cast<UPrimitiveComponent>(Comp);
		else if (Name.Contains(TEXT("Screen")))
			CachedScreen = Cast<UWidgetComponent>(Comp);
		else if (Name.Contains(TEXT("Blocker")))
			CachedBlocker = Cast<UPrimitiveComponent>(Comp);
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

void ATravelCabin::SetScreenText(const FString& Text)
{
	UTextBlock* TimerText = GetTimerTextWidget();
	if (TimerText)
	{
		TimerText->SetText(FText::FromString(Text));
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
	UGameplayStatics::OpenLevel(this, TargetLevelName);
}
