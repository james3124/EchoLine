#include "EchoBotController.h"

#include "EchoGameMode.h"
#include "EchoLineCharacter.h"
#include "EchoNavigationGrid.h"
#include "EchoPickup.h"
#include "EchoWeaponComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

AEchoBotController::AEchoBotController()
{
	PrimaryActorTick.bCanEverTick = true;

	// Deterministic-but-varied seed per instance.
	RandStream.Initialize(static_cast<int32>(GetUniqueID()) ^ FMath::Rand());
}

void AEchoBotController::BeginPlay()
{
	Super::BeginPlay();
}

void AEchoBotController::Possess(AEchoLineCharacter* InPawn)
{
	Pawn = InPawn;
	State = EBotState::Patrol;
	ReactionDelay = 0.15f;
}

void AEchoBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !Pawn)
	{
		return;
	}

	if (Pawn->IsDead())
	{
		State = EBotState::Dead;
		StopMoving();
		return;
	}

	// Reaction delay smooths the bot's decisions so it is not frame-perfect.
	TimeSinceSense += DeltaSeconds;
	if (TimeSinceSense >= 0.1f)
	{
		TimeSinceSense = 0.0f;
		SenseEnemies();
	}

	TimeSinceFire += DeltaSeconds;
	TimeSinceRepath += DeltaSeconds;
	StateTime += DeltaSeconds;

	Think(DeltaSeconds);
}

void AEchoBotController::SenseEnemies()
{
	Target = FindBestTarget();
	if (Target)
	{
		LastKnownTargetLocation = Target->GetActorLocation();
	}
}

AEchoLineCharacter* AEchoBotController::FindBestTarget() const
{
	if (!Pawn || !GetWorld())
	{
		return nullptr;
	}

	AEchoLineCharacter* Best = nullptr;
	float BestDistSq = FMath::Square(LoseTargetRange);

	for (TActorIterator<AEchoLineCharacter> It(GetWorld()); It; ++It)
	{
		AEchoLineCharacter* Other = *It;
		if (!Other || Other == Pawn || Other->IsDead())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), Other->GetActorLocation());
		if (DistSq > BestDistSq)
		{
			continue;
		}

		// Only track enemies we can actually see, but keep chasing a recent one
		// even without sight if it is close.
		const bool bVisible = HasLineOfSightTo(Other);
		if (!bVisible && DistSq > FMath::Square(SightRange * 0.5f))
		{
			continue;
		}

		Best = Other;
		BestDistSq = DistSq;
	}

	return Best;
}

bool AEchoBotController::HasLineOfSightTo(const AActor* InTarget) const
{
	if (!Pawn || !InTarget || !GetWorld())
	{
		return false;
	}

	const FVector Start = Pawn->GetEyeLocation();
	const FVector End = InTarget->GetActorLocation();

	FCollisionQueryParams Params(FName(TEXT("BotSight")), false, Pawn);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	return !bHit || Hit.GetActor() == InTarget;
}

AEchoPickup* AEchoBotController::FindNearestPickup(bool bHealth) const
{
	if (!Pawn || !GetWorld())
	{
		return nullptr;
	}

	const AEchoGameMode* GM = GetWorld()->GetAuthGameMode<AEchoGameMode>();
	if (!GM)
	{
		return nullptr;
	}

	AEchoPickup* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (const TObjectPtr<AEchoPickup>& Pickup : GM->GetPickups())
	{
		if (!Pickup || !Pickup->IsAvailable())
		{
			continue;
		}
		const bool bIsHealth = (Pickup->Type == EEchoPickupType::Health);
		if (bIsHealth != bHealth)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), Pickup->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Pickup;
		}
	}

	return Best;
}

void AEchoBotController::Think(float DeltaSeconds)
{
	if (!Pawn || !Pawn->Weapon)
	{
		return;
	}

	const float HealthFraction = Pawn->GetHealth() / 100.0f;
	const bool bLowHealth = HealthFraction < LowHealthFraction;
	const bool bLowAmmo = Pawn->Weapon->GetAmmo() < 8;

	const bool bTargetVisible = Target != nullptr && HasLineOfSightTo(Target);
	const bool bTargetInRange = bTargetVisible && Target &&
		FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation()) <= AttackRange;

	// --- Decision ---
	if (bLowHealth)
	{
		// Fight back if the enemy is right there, otherwise break off for a medkit.
		EnterState(bTargetInRange ? EBotState::Attack : EBotState::Heal);
	}
	else if (bTargetVisible || Target)
	{
		EnterState(bTargetInRange ? EBotState::Attack : EBotState::Chase);
	}
	else if (bLowAmmo)
	{
		EnterState(EBotState::GetAmmo);
	}
	else
	{
		EnterState(EBotState::Patrol);
	}

	// --- Act ---
	switch (State)
	{
	case EBotState::Attack:
	{
		StopMoving();
		if (Target)
		{
			FaceTowards(Target->GetActorLocation(), DeltaSeconds);

			const FVector ToTarget = (Target->GetActorLocation() - Pawn->GetEyeLocation()).GetSafeNormal();
			const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
				FVector::DotProduct(Pawn->GetActorForwardVector(), FVector(ToTarget.X, ToTarget.Y, 0.0f).GetSafeNormal()), -1.0f, 1.0f)));

			ReactionDelay -= DeltaSeconds;
			if (Angle <= AimToleranceDegrees && ReactionDelay <= 0.0f)
			{
				Pawn->Weapon->BotFire(ToTarget);
				ReactionDelay = 0.0f;
			}
		}
		break;
	}

	case EBotState::Chase:
	{
		if (Target)
		{
			LastKnownTargetLocation = Target->GetActorLocation();
		}
		CurrentGoal = LastKnownTargetLocation;
		bHasGoal = true;
		RepathIfNeeded();
		FollowPath(DeltaSeconds);
		break;
	}

	case EBotState::Heal:
	{
		AEchoPickup* Medkit = FindNearestPickup(true);
		if (Medkit)
		{
			GoalPickup = Medkit;
			CurrentGoal = Medkit->GetActorLocation();
			bHasGoal = true;
			RepathIfNeeded();
			FollowPath(DeltaSeconds);
		}
		else
		{
			// No medkit: fall back to patrolling.
			EnterState(EBotState::Patrol);
			ChooseGoal();
			FollowPath(DeltaSeconds);
		}
		break;
	}

	case EBotState::GetAmmo:
	{
		AEchoPickup* Ammo = FindNearestPickup(false);
		if (Ammo)
		{
			GoalPickup = Ammo;
			CurrentGoal = Ammo->GetActorLocation();
			bHasGoal = true;
			RepathIfNeeded();
			FollowPath(DeltaSeconds);
		}
		else
		{
			EnterState(EBotState::Patrol);
			ChooseGoal();
			FollowPath(DeltaSeconds);
		}
		break;
	}

	case EBotState::Patrol:
	default:
	{
		ChooseGoal();
		RepathIfNeeded();
		FollowPath(DeltaSeconds);
		break;
	}
	}

	// Ammo reaction: reload when the magazine runs dry.
	if (Pawn->Weapon->GetAmmo() <= 0 && !Pawn->Weapon->IsReloading())
	{
		Pawn->Weapon->StartReload();
	}
}

void AEchoBotController::EnterState(EBotState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	StateTime = 0.0f;

	// Repath immediately when the goal meaningfully changes.
	TimeSinceRepath = 999.0f;

	if (NewState != EBotState::Chase && NewState != EBotState::Heal && NewState != EBotState::GetAmmo)
	{
		GoalPickup = nullptr;
	}
}

void AEchoBotController::ChooseGoal()
{
	// Reached the goal? Pick a new one.
	const bool bReachedGoal = bHasGoal && FVector::Dist2D(Pawn->GetActorLocation(), CurrentGoal) < 250.0f;
	if (bHasGoal && !bReachedGoal)
	{
		return;
	}

	if (!GetWorld())
	{
		return;
	}

	const AEchoGameMode* GM = GetWorld()->GetAuthGameMode<AEchoGameMode>();
	const FEchoNavGrid* Grid = GM ? GM->GetNavGrid() : nullptr;

	if (Grid && Grid->IsValid())
	{
		CurrentGoal = Grid->RandomWalkablePoint(RandStream);
	}
	else
	{
		CurrentGoal = FVector(RandStream.FRandRange(-2000.0f, 2000.0f), RandStream.FRandRange(-2000.0f, 2000.0f), 0.0f);
	}

	GoalPickup = nullptr;
	bHasGoal = true;
	TimeSinceRepath = 999.0f;
}

void AEchoBotController::RepathIfNeeded()
{
	if (TimeSinceRepath < RepathInterval && HasPath())
	{
		return;
	}
	TimeSinceRepath = 0.0f;

	if (bHasGoal)
	{
		RepathTo(CurrentGoal);
	}
}

void AEchoBotController::RepathTo(const FVector& Destination)
{
	PathPoints.Reset();
	PathIndex = 0;

	if (!Pawn)
	{
		return;
	}

	const AEchoGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AEchoGameMode>() : nullptr;
	const FEchoNavGrid* Grid = GM ? GM->GetNavGrid() : nullptr;
	if (!Grid || !Grid->IsValid())
	{
		// No grid available: head straight for the destination.
		PathPoints.Add(Destination);
		return;
	}

	TArray<FVector> NewPath;
	if (Grid->FindPath(Pawn->GetActorLocation(), Destination, NewPath))
	{
		PathPoints = MoveTemp(NewPath);
	}
	else
	{
		PathPoints.Add(Destination);
	}
}

void AEchoBotController::FollowPath(float DeltaSeconds)
{
	if (!Pawn || PathPoints.Num() == 0)
	{
		return;
	}

	PathIndex = FMath::Clamp(PathIndex, 0, PathPoints.Num() - 1);
	const FVector Node = PathPoints[PathIndex];

	FVector ToNode = Node - Pawn->GetActorLocation();
	ToNode.Z = 0.0f;

	if (ToNode.SizeSquared() < FMath::Square(120.0f))
	{
		++PathIndex;
		if (PathIndex >= PathPoints.Num())
		{
			PathPoints.Reset();
			PathIndex = 0;
		}
		return;
	}

	const FVector Direction = ToNode.GetSafeNormal();
	Pawn->AddMovementInput(Direction, 1.0f);
	FaceTowards(Node, DeltaSeconds);
}

void AEchoBotController::StopMoving()
{
	// Nothing to do: movement input decays automatically each frame when not fed.
}

void AEchoBotController::FaceTowards(const FVector& WorldPoint, float DeltaSeconds)
{
	if (!Pawn)
	{
		return;
	}

	FVector ToPoint = WorldPoint - Pawn->GetActorLocation();
	ToPoint.Z = 0.0f;
	if (ToPoint.IsNearlyZero())
	{
		return;
	}

	const FRotator Desired(0.0f, ToPoint.Rotation().Yaw, 0.0f);
	const FRotator Current = Pawn->GetActorRotation();
	const FRotator Smoothed = FMath::RInterpTo(Current, Desired, DeltaSeconds, 8.0f);
	Pawn->SetActorRotation(FRotator(0.0f, Smoothed.Yaw, 0.0f));
}
