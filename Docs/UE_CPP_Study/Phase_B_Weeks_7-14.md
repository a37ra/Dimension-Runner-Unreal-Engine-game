# Фаза B — UE + C++ геймплей (недели 7–14)

**Бюджет:** ~3 ч/день.  
**Сквозная практика:** с **недели 9** — проект [Training](Training_Project_Checklist.md), каждую неделю одна маленькая фича.

---

## Неделя 7 — Первый Actor, модуль, логирование

| День | Тема |
|------|------|
| 1 | Структура проекта: `Source/Module`, `Build.cs`; зачем модуль. |
| 2 | Создать пустой `AActor`, разместить в уровне; `UE_LOG`. |
| 3 | `BeginPlay` vs `Tick`: когда не использовать `Tick`. |
| 4 | `Super::`, порядок вызовов (поверхностно). |
| 5 | Категории логов, `Verbose`; вывод `FString`. |
| 6 | Editor: запуск PIE, breakpoints в VS. |
| 7 | Итог: актор считает секунды в `Tick` раз в секунду через накопитель `DeltaTime` или таймер (если уже удобно). |

---

## Неделя 8 — UObject, рефлексия, UPROPERTY

| День | Тема |
|------|------|
| 1 | Что такое `UObject`, зачем `GENERATED_BODY()`. |
| 2 | `UCLASS`, `USTRUCT`, `UENUM`; specifiers `BlueprintType`. |
| 3 | `UPROPERTY`: `EditAnywhere`, `VisibleAnywhere`, `Category`; `meta`. |
| 4 | Сборщик мусора: что должно быть в `UPROPERTY`; `TObjectPtr` (обзорно под вашу версию UE). |
| 5 | Копирование vs ссылки на UObject (поверхностно). |
| 6 | `UFUNCTION`: `BlueprintCallable`, `BlueprintPure`. |
| 7 | Итог: структура данных предмета в `USTRUCT` + актор с массивом в редакторе. |

---

## Неделя 9 — Компоненты

| День | Тема |
|------|------|
| 1 | `UActorComponent`: зачем выносить логику из актора. |
| 2 | Создать компонент вручную в C++; добавить в конструктор актора `CreateDefaultSubobject`. |
| 3 | `TickComponent` vs акторский `Tick`. |
| 4 | Обмен данными: актор держит ссылку на компонент; weak pointers — вводное. |
| 5 | Настройки компонента в `EditDefaultsOnly` vs `EditAnywhere`. |
| 6 | Один BP-наследник: вызвать функцию компонента из BP. |
| 7 | Итог: компонент «вращатель» или «пульсатор» с параметром скорости в деталях. |

---

## Неделя 10 — Ввод и камера (база геймплея)

| День | Тема |
|------|------|
| 1 | `APawn`/`ACharacter` обзорно; PlayerController vs Pawn. |
| 2 | Enhanced Input или legacy `BindAction` (что в вашем шаблоне проекта — то и учить). |
| 3 | Line trace от камеры: `GetPlayerViewPoint`, `LineTraceSingle`. |
| 4 | Channels, `ECC_Visibility`; `DrawDebugLine` на один кадр. |
| 5 | Простое взаимодействие: trace попал в актор — вызвать `UFUNCTION`. |
| 6 | Collision: preset vs channel (поверхностно). |
| 7 | Итог: «нажал E — луч нашёл объект с тегом / интерфейсом» (интерфейс можно отложить на сл. неделю). |

---

## Неделя 11 — Делегаты и события

| День | Тема |
|------|------|
| 1 | `DECLARE_DELEGATE` семейство vs dynamic multicast для BP. |
| 2 | Подписка в C++, отписка в `EndPlay`/`Destroy`. |
| 3 | `Broadcast`, передача параметров в делегате. |
| 4 | `UPROPERTY` для multicast delegate `BlueprintAssignable`. |
| 5 | Паттерн: компонент шлёт событие, UI/актор слушает. |
| 6 | Сравнение с `std::function` (идея, не смешивать без нужды). |
| 7 | Итог: компонент «здоровье» с `OnDead`; UI или лог реагирует. |

---

## Неделя 12 — Таймеры и таймлайны

| День | Тема |
|------|------|
| 1 | `FTimerHandle`, `SetTimer`, `ClearTimer`. |
| 2 | Таймер с lambda vs методом класса (UMemberFunctionDelegate). |
| 3 | Пауза таймера при паузе игры (`FTimerManager` flags). |
| 4 | `FTimeline` + `UCurveFloat`: привязка к одному float-параметру. |
| 5 | Update vs Finished callbacks. |
| 6 | Когда timeline, когда таймер, когда `Tick`. |
| 7 | Итог: дверь или слайдер, открывается по кривой за N секунд. |

---

## Неделя 13 — Подсистемы и хранение данных

| День | Тема |
|------|------|
| 1 | `UGameInstanceSubsystem`: жизненный цикл, `Initialize`. |
| 2 | Получение: `GetGameInstance()->GetSubsystem<UYourSubsystem>()`. |
| 3 | Хранение простых данных между уровнями (без SaveGame). |
| 4 | `WorldSubsystem` vs `GameInstanceSubsystem` — когда что. |
| 5 | `SaveGame` — обзорно (опционально один вечер). |
| 6 | Анти-паттерн: глобальные синглтоны вне UE — зачем не надо. |
| 7 | Итог: подсистема хранит «очки/валюту» между двумя уровнями в Training-проекте. |

---

## Неделя 14 — Интерфейсы и слабые ссылки

| День | Тема |
|------|------|
| 1 | `UINTERFACE` / `IYourInterface`: зачем вместо жёсткого cast к классу. |
| 2 | `DoesImplementInterface`, вызов метода интерфейса. |
| 3 | `TScriptInterface` кратко (если нужно к BP). |
| 4 | `TWeakObjectPtr`: зачем не держать сильные ссылки на всё подряд. |
| 5 | Soft references: `TSoftObjectPtr` — загрузка по требованию (обзорно). |
| 6 | Практика: interact по интерфейсу «Usable». |
| 7 | Итог: три разных актора реализуют один интерфейс, trace вызывает общий метод. |

---

## Ссылки

- Epic: *Actor Lifecycle*, *UObject*, *Gameplay Framework*
- Epic: *Enhanced Input* (если используете)
