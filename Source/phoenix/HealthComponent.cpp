// HealthComponent.cpp — Реализация HP: урон, лечение, авто-регенерация
#include "HealthComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogHealth, Log, All);

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f; // ~20 Hz, достаточно для плавного регена
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	HealthPercent = 1.0f;
	bIsDead = false;
	TimeSinceLastDamage = RegenDelay; // Чтобы реген не ждал на старте
	UE_LOG(LogHealth, Log, TEXT("HealthComponent: Initialized. HP=%.0f/%.0f"), CurrentHealth, MaxHealth);
}

void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsDead) return;

	// Авто-регенерация
	if (bCanRegenerate && CurrentHealth < MaxHealth)
	{
		TimeSinceLastDamage += DeltaTime;

		if (TimeSinceLastDamage >= RegenDelay)
		{
			CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + RegenRate * DeltaTime);
			UpdateHealthState();
		}
	}
}

void UHealthComponent::TakeDamage(float Amount)
{
	if (bIsDead || Amount <= 0.0f) return;

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Amount);
	TimeSinceLastDamage = 0.0f; // Сбросить таймер регена

	UE_LOG(LogHealth, Log, TEXT("TakeDamage: -%.0f → HP=%.0f/%.0f"), Amount, CurrentHealth, MaxHealth);

	UpdateHealthState();

	if (CurrentHealth <= 0.0f)
	{
		Kill();
	}
}

void UHealthComponent::Heal(float Amount)
{
	if (bIsDead || Amount <= 0.0f) return;

	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);

	UE_LOG(LogHealth, Log, TEXT("Heal: +%.0f → HP=%.0f/%.0f"), Amount, CurrentHealth, MaxHealth);

	UpdateHealthState();
}

void UHealthComponent::Kill()
{
	if (bIsDead) return;

	bIsDead = true;
	CurrentHealth = 0.0f;
	HealthPercent = 0.0f;

	UE_LOG(LogHealth, Warning, TEXT("PLAYER DIED!"));

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	OnDeath.Broadcast();
}

void UHealthComponent::ResetHealth()
{
	bIsDead = false;
	CurrentHealth = MaxHealth;
	TimeSinceLastDamage = RegenDelay;

	UE_LOG(LogHealth, Log, TEXT("ResetHealth: HP=%.0f/%.0f"), CurrentHealth, MaxHealth);

	UpdateHealthState();
}

void UHealthComponent::UpdateHealthState()
{
	HealthPercent = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}
