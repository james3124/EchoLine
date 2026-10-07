// EchoLine - free-for-all match flow. Server authoritative.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EchoNavigationGrid.h"
#include "EchoGameMode.generated.h"

class AEchoLineCharacter;
class ARelayYardBuilder;
class AEchoPickup;

UCLASS()
class ECHOLINE_API AEchoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEchoGameMode();

	virtual void BeginPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Called by a character when it dies. Handles scoring and schedules a respawn. */
	void NotifyKill(AController* Killer, AEchoLineCharacter* Victim, AEchoLineCharacter* KillerPawn);

	/** Round length in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Echo|Match")
	float MatchDuration = 300.0f;

	/** Delay before a dead player respawns. */
	UPROPERTY(EditDefaultsOnly, Category = "Echo|Match")
	float RespawnDelay = 3.0f;

	/** Pause between rounds after the results screen. */
	UPROPERTY(EditDefaultsOnly, Category = "Echo|Match")
	float RoundResetDelay = 8.0f;

	/** Number of AI bots to spawn for offline / solo play. */
	UPROPERTY(EditDefaultsOnly, Category = "Echo|Bots")
	int32 BotCount = 3;

	/** Adds a bot at runtime (also exposed to Blueprint). */
	UFUNCTION(BlueprintCallable, Category = "Echo|Bots")
	void AddBot();

	/** Navigation grid built by the level, used by bot controllers. */
	const FEchoNavGrid* GetNavGrid() const;

	/** Pickups placed in the level, used by bot controllers. */
	const TArray<TObjectPtr<AEchoPickup>>& GetPickups() const;

protected:
	void EndMatch();
	void ResetRound();
	void RespawnPlayer(AController* Controller);
	void RespawnBot(AEchoLineCharacter* Bot);

	/** Advances the round clock; driven by a repeating timer. */
	void UpdateMatchTimer();

	/** Returns the level builder, spawning it if necessary. */
	ARelayYardBuilder* EnsureBuilder();

	/** Spawns one AI bot and possess it. */
	void SpawnBot();

	/** Respawn handles per controller. */
	FTimerHandle RoundResetTimer;
	FTimerHandle MatchTimerHandle;
	FTimerHandle BotSpawnTimer;

	TObjectPtr<ARelayYardBuilder> Builder;

	/** Number of bots we have already spawned this match. */
	int32 BotsSpawned = 0;

	/** Interval, in seconds, at which the round clock ticks down. */
	static constexpr float MatchTickInterval = 0.5f;
};
