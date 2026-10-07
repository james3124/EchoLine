// EchoLine - AI bot controller.
// A hierarchical state machine: heal when hurt, engage a visible enemy, chase a lost
// one, otherwise patrol to a goal (usually an useful pickup), with item/ammo reactions.
// Movement uses the level's A* navigation grid; there is no baked NavMesh dependency.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoBotController.generated.h"

class AEchoLineCharacter;
class AEchoPickup;
class ARelayYardBuilder;

UENUM(BlueprintType)
enum class EBotState : uint8
{
	Idle,
	Patrol,
	Chase,
	Attack,
	Heal,
	GetAmmo,
	Dead
};

/**
 * Lightweight bot brain. It is an Actor (not an AController) so it does not need
 * navigation or a PlayerState, and it drives the pawn directly on the server.
 */
UCLASS()
class ECHOLINE_API AEchoBotController : public AActor
{
	GENERATED_BODY()

public:
	AEchoBotController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Bind the controller to a pawn (server only). */
	void Possess(AEchoLineCharacter* InPawn);

	EBotState GetState() const { return State; }
	AEchoLineCharacter* GetPawn() const { return Pawn; }

protected:
	/** Runs one decision tick. */
	void Think(float DeltaSeconds);

	/** Refreshes the visible-enemy cache, including the shootable one. */
	void SenseEnemies();

	/** Picks a destination for patrol / item goals. */
	void ChooseGoal();

	/** Requests a fresh A* path to the current goal. */
	void RepathTo(const FVector& Destination);

	/** Re-paths if the cached path is stale or missing. */
	void RepathIfNeeded();

	/** Steers along the current path. */
	void FollowPath(float DeltaSeconds);

	void StopMoving();

	bool HasPath() const { return PathPoints.Num() > 0; }
	bool HasLineOfSightTo(const AActor* Target) const;

	AEchoLineCharacter* FindBestTarget() const;
	AEchoPickup* FindNearestPickup(bool bHealth) const;

	/** Turns the pawn toward a world point (yaw only). */
	void FaceTowards(const FVector& WorldPoint, float DeltaSeconds);

	void EnterState(EBotState NewState);

	UPROPERTY()
	TObjectPtr<AEchoLineCharacter> Pawn;

	UPROPERTY()
	TObjectPtr<AEchoLineCharacter> Target;

	UPROPERTY()
	TObjectPtr<AEchoPickup> GoalPickup;

	EBotState State = EBotState::Idle;

	// Senses.
	UPROPERTY(EditAnywhere, Category = "Bot")
	float SightRange = 4500.0f;

	UPROPERTY(EditAnywhere, Category = "Bot")
	float LoseTargetRange = 6000.0f;

	UPROPERTY(EditAnywhere, Category = "Bot")
	float AttackRange = 3200.0f;

	UPROPERTY(EditAnywhere, Category = "Bot")
	float AimToleranceDegrees = 6.0f;

	/** Below this fraction of max health the bot looks for a medkit. */
	UPROPERTY(EditAnywhere, Category = "Bot")
	float LowHealthFraction = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Bot")
	float FireInterval = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Bot")
	float RepathInterval = 0.5f;

	// Runtime.
	TArray<FVector> PathPoints;
	int32 PathIndex = 0;
	float TimeSinceRepath = 0.0f;
	float TimeSinceFire = 0.0f;
	float TimeSinceSense = 0.0f;
	float StateTime = 0.0f;
	float ReactionDelay = 0.0f;

	FVector LastKnownTargetLocation = FVector::ZeroVector;

	/** Current navigation goal this bot is moving toward. */
	FVector CurrentGoal = FVector::ZeroVector;
	bool bHasGoal = false;

	FRandomStream RandStream;
};
