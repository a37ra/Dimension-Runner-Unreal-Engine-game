// HealthComponent.h — HP игрока: урон, лечение, авто-регенерация, смерть.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDeath);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PHOENIX_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	// ==================== ПАРАМЕТРЫ ====================

	/** Максимальное здоровье */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100.0f;

	/** Включена ли авто-регенерация? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regen")
	bool bCanRegenerate = true;

	/** Скорость регенерации (HP в секунду) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regen")
	float RegenRate = 5.0f;

	/** Задержка перед началом регенерации после урона (секунды) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regen")
	float RegenDelay = 3.0f;

	// ==================== СОСТОЯНИЕ (для UI) ====================

	/** Текущее здоровье. Читай из HUD! */
	UPROPERTY(BlueprintReadOnly, Category = "Health|State")
	float CurrentHealth;

	/** HP в процентах (0.0 — 1.0). Для прогресс-баров. */
	UPROPERTY(BlueprintReadOnly, Category = "Health|State")
	float HealthPercent = 1.0f;

	/** Мертв? */
	UPROPERTY(BlueprintReadOnly, Category = "Health|State")
	bool bIsDead = false;

	// ==================== ДЕЛЕГАТЫ ====================

	/** Вызывается при любом изменении HP (для UI) */
	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnHealthChanged OnHealthChanged;

	/** Вызывается при смерти */
	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnPlayerDeath OnDeath;

	// ==================== ФУНКЦИИ ====================

	/** Получить урон. Amount > 0 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void TakeDamage(float Amount);

	/** Восстановить HP. Amount > 0 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);

	/** Мгновенная смерть */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Kill();

	/** Полное восстановление HP (респавн, новый день) */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void ResetHealth();

	/** Жив ли персонаж? */
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsAlive() const { return !bIsDead; }

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Время с момента последнего урона (для задержки регена) */
	float TimeSinceLastDamage = 0.0f;

	/** Пересчитать процент и вызвать делегат */
	void UpdateHealthState();
};
