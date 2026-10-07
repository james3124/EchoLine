// EchoLine - firearm handling. Server authoritative for damage and hit resolution.
// The owning client keeps a local spread model purely for crosshair feedback.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchoTypes.h"
#include "EchoWeapons.h"
#include "EchoWeaponComponent.generated.h"

class AEchoLineCharacter;

UCLASS(ClassGroup = (EchoLine), meta = (BlueprintSpawnableComponent))
class ECHOLINE_API UEchoWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEchoWeaponComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	EWeaponId StartingWeapon = EWeaponId::Meridian;

	// --- Replicated state (owner sees these for the HUD) ---
	UPROPERTY(ReplicatedUsing = OnRep_WeaponState, BlueprintReadOnly)
	EWeaponId CurrentWeaponId = EWeaponId::Meridian;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponState, BlueprintReadOnly)
	int32 Ammo = 30;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponState, BlueprintReadOnly)
	bool bReloading = false;

	// --- Queries ---
	const FWeaponStats& GetStats() const { return CachedStats; }
	EWeaponId GetWeaponId() const { return CurrentWeaponId; }
	int32 GetAmmo() const { return Ammo; }
	bool IsReloading() const { return bReloading; }

	/** Current cone half-angle in degrees, for the dynamic crosshair. */
	float GetCurrentSpreadDegrees() const;

	/** World time of the last confirmed hit; the HUD fades a hitmarker from this. */
	float GetLastHitTime() const { return LastHitTime; }
	bool WasLastHitKill() const { return bLastHitKill; }
	void ClearHitFeedback() { LastHitTime = -100.0f; bLastHitKill = false; }

	// --- Input entry points ---
	void StartFire();
	void StopFire();
	void StartReload();
	void SwitchToWeapon(EWeaponId Id);
	void CycleWeapon();
	void RefillAmmo();

	/** Server helper used by bots: fires one authoritative shot in the given direction. */
	void BotFire(const FVector& AimDir);

	// --- Server RPCs ---
	// Fire is high-rate and each shot is independent, so it is sent unreliably.
	// Reload and weapon switching must not be dropped, so they stay reliable.
	UFUNCTION(Server, Unreliable)
	void ServerFire(FVector Origin, FVector AimDir, bool bAnchorShot);

	UFUNCTION(Server, Reliable)
	void ServerReload();

	UFUNCTION(Server, Reliable)
	void ServerSwitchWeapon(EWeaponId Id);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotFX(FVector Start, FVector End, bool bHit, bool bKill);

	/** Tells the shooter's own client to show a hitmarker. */
	UFUNCTION(Client, Reliable)
	void ClientHitFeedback(bool bKill);

	/** Plays the reload sound on the owning client. */
	UFUNCTION(Client, Unreliable)
	void ClientPlayReload();

	UFUNCTION()
	void OnRep_WeaponState();

protected:
	/** Performs the authoritative shot on the server. */
	void FireOnce(const FVector& Origin, const FVector& AimDir, bool bAnchorShot);

	/** Sends a shot request to the server with local feedback applied first. */
	void SendShot();

	bool IsOwnerLocallyControlled() const;

	void FinishReload();
	void RefreshStats();

	AEchoLineCharacter* GetEchoCharacter() const;

	FWeaponStats CachedStats;

	bool bFireHeld = false;
	double LastFireTime = -1000.0;
	double LastAutoShotTime = -1000.0;
	double ReloadEndTime = 0.0;
	float LocalSpread = 0.0f;
	float LastHitTime = -100.0f;
	bool bLastHitKill = false;
};
