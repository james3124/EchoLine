// EchoLine - the operator pawn. First-person camera, health, movement, Echo Link.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EchoTypes.h"
#include "EchoLineCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UEchoWeaponComponent;
class UEchoLinkComponent;

UCLASS()
class ECHOLINE_API AEchoLineCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEchoLineCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Echo")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Echo|Visual")
	TObjectPtr<UStaticMeshComponent> Harness;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Echo|Visual")
	TObjectPtr<UStaticMeshComponent> Overcoat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Echo|Visual")
	TObjectPtr<UStaticMeshComponent> Antenna;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Echo|Visual")
	TObjectPtr<UStaticMeshComponent> GunBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Echo|Visual")
	TObjectPtr<UStaticMeshComponent> GunBarrel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Echo|Visual")
	TObjectPtr<UStaticMeshComponent> GunMag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Echo|Visual")
	TObjectPtr<UStaticMeshComponent> GunStock;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Echo|Visual")
	TObjectPtr<UStaticMeshComponent> GunOptic;

	/** Applies the operator's colours and role-specific gear. */
	void ApplyOperatorVisuals();

	/** Reconfigures the first-person weapon parts for a weapon. */
	void ApplyWeaponVisuals(EWeaponId Id);

	/** Body parts tinted with the operator's primary colour. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PrimaryParts;

	/** Body parts tinted with the operator's secondary colour. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SecondaryParts;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Echo")
	TObjectPtr<UEchoWeaponComponent> Weapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Echo")
	TObjectPtr<UEchoLinkComponent> Echo;

	// --- Health ---
	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Echo|Health")
	float HealthPoints = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Echo|Health")
	float MaxHealthPoints = 100.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo|Health")
	bool bDead = false;

	// --- Appearance / aim (replicated for remote players) ---
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo")
	EOperatorId Operator = EOperatorId::Rook;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo|Aim")
	float AimPitch = 0.0f;

	/** Aiming down sights. Replicated so the server can apply ADS spread. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo|Aim")
	bool bIsAiming = false;

	// --- Movement tuning ---
	UPROPERTY(EditDefaultsOnly, Category = "Echo|Movement")
	float WalkSpeed = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Echo|Movement")
	float CrouchSpeed = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Echo|Aim")
	float BaseFieldOfView = 90.0f;

	/** Ads camera scale applied to the saved FOV is handled by the controller. */
	void SetBaseFieldOfView(float NewFOV) { BaseFieldOfView = NewFOV; }

	// --- Queries used by weapons and the HUD ---
	bool IsDead() const { return bDead; }
	float GetHealth() const { return HealthPoints; }
	FVector GetEyeLocation() const;
	FRotator GetAimRotation() const;

	bool IsAiming() const { return bIsAiming; }
	void SetAiming(bool bAiming);

	/** True when gameplay input should be processed (alive and past any menu). */
	bool IsInputAllowed() const;
	/** Applies camera recoil. Called by the weapon on the owning client. */
	void AddRecoil(float PitchDeg, float YawDeg);

	/** Resets the pawn for a new life and moves it to a spawn transform. */
	void RespawnAt(const FTransform& SpawnTransform);

	/** Restores health, clamped to the maximum. Server only. */
	void Heal(float Amount);

	/** True when this pawn is driven by a bot rather than a player. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo")
	bool bBot = false;

	/** Display name used for bots (they have no PlayerState). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo")
	FString BotName;

	/** Name shown in the kill feed and scoreboard. */
	FString GetDisplayName() const;

	// --- Input (shared by touch controls and keyboard/mouse) ---
	void MoveForward(float Value);
	void MoveRight(float Value);
	void InputFireStart();
	void InputFireStop();
	void InputReload();
	void InputEcho();
	void InputAimStart();
	void InputAimStop();
	void InputJump();
	void InputToggleCrouch();
	void InputSwitchWeapon();

	UFUNCTION(Server, Unreliable)
	void ServerUpdateAim(FRotator ControlRotation);

	/** Server notification that aiming changed. */
	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bAiming);

protected:
	UFUNCTION()
	void OnRep_Health();

	void ApplyAiming(bool bAiming);
	void UpdateFOV(float DeltaSeconds);

	float CurrentFOV = 90.0f;
	float AimUpdateAccumulator = 0.0f;
	float FootstepTimer = 0.0f;
	float FootstepInterval = 0.45f;

	/** Last visuals applied, so we only rebuild when something actually changes. */
	EOperatorId LastAppliedOperator = EOperatorId::MAX;
	EWeaponId LastAppliedWeapon = EWeaponId::MAX;
};
