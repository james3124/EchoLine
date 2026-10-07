#include "EchoWeaponComponent.h"

#include "EchoAnchor.h"
#include "EchoAudio.h"
#include "EchoLineCharacter.h"
#include "EchoLinkComponent.h"
#include "EchoTracer.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

UEchoWeaponComponent::UEchoWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Firing and damage are resolved by the server; state is replicated to owners.
	SetIsReplicatedByDefault(true);
}

void UEchoWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		CurrentWeaponId = StartingWeapon;
	}
	RefreshStats();
	Ammo = CachedStats.MagazineSize;
}

void UEchoWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEchoWeaponComponent, CurrentWeaponId);
	DOREPLIFETIME(UEchoWeaponComponent, Ammo);
	DOREPLIFETIME(UEchoWeaponComponent, bReloading);
}

AEchoLineCharacter* UEchoWeaponComponent::GetEchoCharacter() const
{
	return Cast<AEchoLineCharacter>(GetOwner());
}

void UEchoWeaponComponent::RefreshStats()
{
	CachedStats = EchoWeapons::GetStats(CurrentWeaponId);
}

float UEchoWeaponComponent::GetCurrentSpreadDegrees() const
{
	const AEchoLineCharacter* Character = GetEchoCharacter();

	float Base = Character && Character->IsAiming() ? CachedStats.AdsSpreadDegrees : CachedStats.HipSpreadDegrees;

	if (Character)
	{
		const float Speed = Character->GetVelocity().Size2D();
		if (Speed > 50.0f)
		{
			Base *= CachedStats.MoveSpreadMultiplier;
		}
	}

	return FMath::Max(Base, LocalSpread);
}

void UEchoWeaponComponent::StartFire()
{
	AEchoLineCharacter* Character = GetEchoCharacter();
	if (!Character || Character->IsDead())
	{
		return;
	}

	bFireHeld = true;

	// Semi-automatic weapons fire once per press. Automatic weapons are driven
	// from TickComponent while the trigger is held.
	if (CachedStats.FireMode == EFireMode::Semi)
	{
		SendShot();
	}
}

void UEchoWeaponComponent::SendShot()
{
	AEchoLineCharacter* Character = GetEchoCharacter();
	if (!Character || Character->IsDead() || bReloading)
	{
		return;
	}

	// Local recoil and spread are applied immediately for responsiveness; the
	// server recomputes its own spread before resolving the shot.
	LocalSpread += CachedStats.HipSpreadDegrees * 0.35f;
	Character->AddRecoil(CachedStats.RecoilPitch, FMath::FRandRange(-CachedStats.RecoilYaw, CachedStats.RecoilYaw));

	// Immediate local gunshot so the trigger feels responsive.
	UEchoAudioSubsystem::Play(this, UEchoAudioSubsystem::FireSfxForWeapon(static_cast<uint8>(CurrentWeaponId)), FVector::ZeroVector, true, 0.06f);

	bool bAnchorShot = false;
	if (UEchoLinkComponent* Echo = Character->Echo)
	{
		bAnchorShot = Echo->IsEchoArmed();
		if (bAnchorShot)
		{
			// Disarm locally the moment the shot leaves; the server confirms placement.
			Echo->SetEchoArmed(false);
		}
	}

	ServerFire(Character->GetEyeLocation(), Character->GetAimRotation().Vector(), bAnchorShot);
}

void UEchoWeaponComponent::StopFire()
{
	bFireHeld = false;
}

void UEchoWeaponComponent::StartReload()
{
	const AEchoLineCharacter* Character = GetEchoCharacter();
	if (!Character || Character->IsDead())
	{
		return;
	}
	ServerReload();
}

void UEchoWeaponComponent::SwitchToWeapon(EWeaponId Id)
{
	ServerSwitchWeapon(Id);
}

void UEchoWeaponComponent::CycleWeapon()
{
	ServerSwitchWeapon(EchoWeapons::Next(CurrentWeaponId));
}

void UEchoWeaponComponent::RefillAmmo()
{
	if (GetOwner() && GetOwner()->HasAuthority() && !bReloading)
	{
		Ammo = CachedStats.MagazineSize;
	}
}

void UEchoWeaponComponent::BotFire(const FVector& AimDir)
{
	AEchoLineCharacter* Character = GetEchoCharacter();
	if (!Character || !Character->HasAuthority())
	{
		return;
	}
	FireOnce(Character->GetEyeLocation(), AimDir, false);
}

void UEchoWeaponComponent::ServerReload_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (bReloading || Ammo >= CachedStats.MagazineSize)
	{
		return;
	}
	bReloading = true;
	ReloadEndTime = GetWorld()->GetTimeSeconds() + CachedStats.ReloadSeconds;

	// Reload audio plays on the owning client.
	ClientPlayReload();
}

void UEchoWeaponComponent::ClientPlayReload_Implementation()
{
	UEchoAudioSubsystem::Play(this, EEchoSfx::Reload, FVector::ZeroVector, true);
}

void UEchoWeaponComponent::ServerSwitchWeapon_Implementation(EWeaponId Id)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	CurrentWeaponId = Id;
	RefreshStats();
	Ammo = CachedStats.MagazineSize;
	bReloading = false;
}

void UEchoWeaponComponent::ServerFire_Implementation(FVector Origin, FVector AimDir, bool bAnchorShot)
{
	FireOnce(Origin, AimDir, bAnchorShot);
}

void UEchoWeaponComponent::FireOnce(const FVector& Origin, const FVector& AimDir, bool bAnchorShot)
{
	UWorld* World = GetWorld();
	AEchoLineCharacter* Character = GetEchoCharacter();
	if (!World || !Character || !Character->HasAuthority() || Character->IsDead())
	{
		return;
	}

	if (bReloading)
	{
		return;
	}

	// Armed Echo shots place an anchor instead of dealing damage.
	if (bAnchorShot)
	{
		if (UEchoLinkComponent* Echo = Character->Echo)
		{
			if (Echo->ServerTryPlaceAnchor(Origin, AimDir))
			{
				Echo->SetEchoArmed(false);
			}
		}
		return;
	}

	if (Ammo <= 0)
	{
		ServerReload_Implementation();
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (Now - LastFireTime < CachedStats.GetShotInterval() * 0.9)
	{
		return;
	}
	LastFireTime = Now;
	Ammo = FMath::Max(0, Ammo - 1);

	// Server-side spread cone.
	float Spread = Character->IsAiming() ? CachedStats.AdsSpreadDegrees : CachedStats.HipSpreadDegrees;
	if (Character->GetVelocity().Size2D() > 50.0f)
	{
		Spread *= CachedStats.MoveSpreadMultiplier;
	}
	const FVector ShotDir = FMath::VRandCone(AimDir.GetSafeNormal(), FMath::DegreesToRadians(FMath::Max(Spread, 0.05f)));

	// Echo Link redirection: if the shot enters one of my linked anchors, exit from the partner.
	FVector TraceOrigin = Origin;
	FVector TraceDir = ShotDir;

	if (UEchoLinkComponent* Echo = Character->Echo)
	{
		FVector ROrigin, RDir;
		if (Echo->TryGetRedirect(TraceOrigin, TraceDir, ROrigin, RDir))
		{
			TraceOrigin = ROrigin;
			TraceDir = RDir;
		}
	}

	// Build the ignore list: never let a player's own body or own anchors block
	// their shots (this also prevents the exit anchor from blocking a redirected shot).
	FCollisionQueryParams Params(FName(TEXT("EchoWeaponTrace")), false, Character);
	if (const UEchoLinkComponent* Echo = Character->Echo)
	{
		for (const TObjectPtr<AEchoAnchor>& Owned : Echo->GetAnchors())
		{
			if (Owned)
			{
				Params.AddIgnoredActor(Owned);
			}
		}
	}

	FHitResult Hit;
	const FVector TraceEnd = TraceOrigin + TraceDir * CachedStats.MaxRange;
	const bool bHit = World->LineTraceSingleByChannel(Hit, TraceOrigin, TraceEnd, ECC_Visibility, Params);

	const FVector FXEnd = bHit ? Hit.ImpactPoint : TraceEnd;
	bool bHitEnemy = false;
	bool bKill = false;

	if (bHit && Hit.GetActor())
	{
		if (AEchoAnchor* HitAnchor = Cast<AEchoAnchor>(Hit.GetActor()))
		{
			// Shooting an enemy anchor destroys it. Your own anchors are protected.
			bool bOwnAnchor = false;
			if (const UEchoLinkComponent* Echo = Character->Echo)
			{
				for (const TObjectPtr<AEchoAnchor>& Owned : Echo->GetAnchors())
				{
					if (Owned == HitAnchor)
					{
						bOwnAnchor = true;
						break;
					}
				}
			}

			if (!bOwnAnchor)
			{
				HitAnchor->SetPartner(nullptr);
				HitAnchor->Destroy();
			}
		}
		else if (AEchoLineCharacter* Victim = Cast<AEchoLineCharacter>(Hit.GetActor()))
		{
			if (Victim != Character && !Victim->IsDead())
			{
				const float Distance = FVector::Dist(Origin, Hit.ImpactPoint);
				const float Damage = CachedStats.GetDamageAtDistance(Distance);
				const bool bWasDead = Victim->IsDead();

				UGameplayStatics::ApplyDamage(Victim, Damage, Character->GetController(), Character, UDamageType::StaticClass());

				bHitEnemy = true;
				bKill = !bWasDead && Victim->IsDead();
			}
		}
	}

	// Tracer and hit feedback for every client.
	MulticastShotFX(TraceOrigin, FXEnd, bHitEnemy, bKill);

	if (bHitEnemy)
	{
		ClientHitFeedback(bKill);
	}

	if (Ammo <= 0)
	{
		ServerReload_Implementation();
	}
}

void UEchoWeaponComponent::MulticastShotFX_Implementation(FVector Start, FVector End, bool bHit, bool bKill)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (AEchoTracer* Tracer = World->SpawnActor<AEchoTracer>(AEchoTracer::StaticClass()))
	{
		Tracer->SetSegment(Start, End);
	}

	// The owning client already played its shot locally for immediacy; others hear it here.
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const bool bLocallyControlled = OwnerPawn && OwnerPawn->IsLocallyControlled();
	if (!bLocallyControlled)
	{
		UEchoAudioSubsystem::Play(this, UEchoAudioSubsystem::FireSfxForWeapon(static_cast<uint8>(CurrentWeaponId)), Start, false, 0.06f);
	}
}

void UEchoWeaponComponent::ClientHitFeedback_Implementation(bool bKill)
{
	LastHitTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	bLastHitKill = bKill;

	UEchoAudioSubsystem::Play(this, bKill ? EEchoSfx::Kill : EEchoSfx::Hitmarker, FVector::ZeroVector, true);
}

void UEchoWeaponComponent::OnRep_WeaponState()
{
	RefreshStats();
}

void UEchoWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Spread decays back toward its base value.
	LocalSpread = FMath::Max(0.0f, LocalSpread - DeltaTime * (CachedStats.HipSpreadDegrees * 2.0f + 2.0f));

	// Automatic fire repeats while the trigger is held. The owning client paces
	// the requests; the server enforces the fire rate again in FireOnce.
	if (bFireHeld && CachedStats.FireMode == EFireMode::Auto && !bReloading && IsOwnerLocallyControlled())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now - LastAutoShotTime >= CachedStats.GetShotInterval())
		{
			LastAutoShotTime = Now;
			SendShot();
		}
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// Finish an in-progress reload on the server.
	if (bReloading && GetWorld()->GetTimeSeconds() >= ReloadEndTime)
	{
		FinishReload();
	}
}

bool UEchoWeaponComponent::IsOwnerLocallyControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

void UEchoWeaponComponent::FinishReload()
{
	bReloading = false;
	Ammo = CachedStats.MagazineSize;
}
