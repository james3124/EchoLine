// EchoLine - replicated match state (timer, scores, kill feed).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "EchoGameState.generated.h"

class AActor;

/** One line in the kill feed. */
USTRUCT(BlueprintType)
struct FEchoKillFeedEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Echo")
	FString KillerName;

	UPROPERTY(BlueprintReadOnly, Category = "Echo")
	FString VictimName;

	UPROPERTY(BlueprintReadOnly, Category = "Echo")
	float TimeStamp = 0.0f;
};

UCLASS()
class ECHOLINE_API AEchoGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AEchoGameState();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Seconds left in the current round. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo|Match")
	float TimeLeft = 300.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo|Match")
	bool bMatchOver = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo|Match")
	FString WinnerName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo|Match")
	TArray<FEchoKillFeedEntry> KillFeed;

	/** Bot scores and deaths, keyed by bot display name (bots have no PlayerState). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo|Match")
	TMap<FString, int32> BotScores;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo|Match")
	TMap<FString, int32> BotDeaths;

	/** Server: appends a kill feed line, trimming the list. */
	void AddKillFeedEntry(const FString& KillerName, const FString& VictimName);

	/** Server: records a bot's kill. */
	void AddBotKill(const FString& BotName);

	/** Server: records a bot's death. */
	void AddBotDeath(const FString& BotName);

	/** Server: clears bot stats for a new round. */
	void ClearBotStats();

protected:
	/** The procedural level, built locally on every machine (server and clients). */
	UPROPERTY()
	TObjectPtr<AActor> LevelBuilder;
};
