#include "EchoLinkComponent.h"

#include "EchoAnchor.h"
#include "EchoAudio.h"
#include "EchoLineCharacter.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

UEchoLinkComponent::UEchoLinkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// The counters are replicated so every client can show anchor status.
	SetIsReplicatedByDefault(true);
}

void UEchoLinkComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEchoLinkComponent, AnchorCount);
	DOREPLIFETIME(UEchoLinkComponent, bLinked);
}

void UEchoLinkComponent::RefreshReplicatedState()
{
	AnchorCount = Anchors.Num();
	bLinked = Anchors.Num() >= 2;
}

void UEchoLinkComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UEchoLinkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyAllAnchors();
	Super::EndPlay(EndPlayReason);
}

AEchoLineCharacter* UEchoLinkComponent::GetEchoCharacter() const
{
	return Cast<AEchoLineCharacter>(GetOwner());
}

void UEchoLinkComponent::SetEchoArmed(bool bArmed)
{
	bEchoArmed = bArmed;
}

float UEchoLinkComponent::GetCooldownRemaining() const
{
	if (!GetWorld())
	{
		return 0.0f;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	return FMath::Max(0.0f, static_cast<float>(PlaceCooldown - (Now - LastPlaceTime)));
}

bool UEchoLinkComponent::ServerTryPlaceAnchor(const FVector& Origin, const FVector& AimDir)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !GetWorld())
	{
		return false;
	}

	if (GetCooldownRemaining() > 0.0f)
	{
		return false;
	}

	const FVector End = Origin + AimDir.GetSafeNormal() * MaxPlaceDistance;

	FCollisionQueryParams Params(FName(TEXT("EchoPlaceTrace")), false, Owner);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params);

	// Only actors tagged EchoPanel accept anchors. The builder tags its panels.
	if (!bHit || !Hit.GetActor() || !Hit.GetActor()->ActorHasTag(TEXT("EchoPanel")))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEchoAnchor* Anchor = GetWorld()->SpawnActor<AEchoAnchor>(
		AEchoAnchor::StaticClass(),
		Hit.ImpactPoint + Hit.ImpactNormal * 8.0f,
		Hit.ImpactNormal.Rotation(),
		SpawnParams);

	if (!Anchor)
	{
		return false;
	}

	int32 OwnerId = -1;
	if (const APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		if (const APlayerState* PS = OwnerPawn->GetPlayerState())
		{
			OwnerId = PS->GetPlayerId();
		}
	}
	Anchor->InitAnchor(OwnerId, Hit.ImpactNormal, FLinearColor(0.10f, 0.55f, 1.00f));
	Anchor->SetLifeSpan(AnchorLifetime);
	Anchor->OnDestroyed.AddDynamic(this, &UEchoLinkComponent::HandleAnchorDestroyed);

	// Anchor placement cue, heard by everyone nearby.
	UEchoAudioSubsystem::Play(this, EEchoSfx::EchoPlace, Anchor->GetActorLocation(), false);

	// Recycle the oldest anchor when at capacity.
	while (Anchors.Num() >= MaxAnchors)
	{
		if (AEchoAnchor* Oldest = Anchors[0])
		{
			Anchors.RemoveAt(0);
			Oldest->SetPartner(nullptr);
			Oldest->Destroy();
		}
		else
		{
			Anchors.RemoveAt(0);
		}
	}

	Anchors.Add(Anchor);

	// Once two anchors exist, link them together.
	if (Anchors.Num() >= 2)
	{
		AEchoAnchor* A = Anchors[Anchors.Num() - 2];
		AEchoAnchor* B = Anchors[Anchors.Num() - 1];
		if (A)
		{
			A->SetPartner(B);
		}
		if (B)
		{
			B->SetPartner(A);
		}
	}

	LastPlaceTime = GetWorld()->GetTimeSeconds();
	RefreshReplicatedState();
	return true;
}

bool UEchoLinkComponent::TryGetRedirect(const FVector& Origin, const FVector& Dir, FVector& OutOrigin, FVector& OutDir) const
{
	if (Anchors.Num() < 2)
	{
		return false;
	}

	const FVector Direction = Dir.GetSafeNormal();

	for (const TObjectPtr<AEchoAnchor>& Anchor : Anchors)
	{
		if (!Anchor || !Anchor->IsLinked())
		{
			continue;
		}

		// Ray/sphere test against the anchor position.
		const FVector ToCenter = Anchor->GetActorLocation() - Origin;
		const float Along = FVector::DotProduct(ToCenter, Direction);
		if (Along < 0.0f || Along > MaxPlaceDistance)
		{
			continue;
		}

		const FVector Closest = Origin + Direction * Along;
		const float DistSq = FVector::DistSquared(Closest, Anchor->GetActorLocation());
		if (DistSq > FMath::Square(AnchorHitRadius))
		{
			continue;
		}

		// Entering this anchor: exit from its partner, keeping the shot direction.
		AEchoAnchor* Partner = Anchor->GetPartner();
		if (Partner)
		{
			OutOrigin = Partner->GetActorLocation() + Partner->GetSurfaceNormal() * 12.0f;
			OutDir = Direction;
			return true;
		}
	}

	return false;
}

void UEchoLinkComponent::NotifyAnchorDestroyed(AEchoAnchor* Anchor)
{
	Anchors.Remove(Anchor);
	RefreshReplicatedState();
}

void UEchoLinkComponent::HandleAnchorDestroyed(AActor* DestroyedActor)
{
	if (AEchoAnchor* Anchor = Cast<AEchoAnchor>(DestroyedActor))
	{
		// Break the link on the surviving partner.
		if (AEchoAnchor* Partner = Anchor->GetPartner())
		{
			Partner->SetPartner(nullptr);
		}
		Anchors.Remove(Anchor);
		RefreshReplicatedState();

		UEchoAudioSubsystem::Play(this, EEchoSfx::EchoDestroy, Anchor->GetActorLocation(), false);
	}
}

void UEchoLinkComponent::DestroyAllAnchors()
{
	for (const TObjectPtr<AEchoAnchor>& Anchor : Anchors)
	{
		if (Anchor)
		{
			Anchor->SetPartner(nullptr);
			Anchor->Destroy();
		}
	}
	Anchors.Reset();
}
