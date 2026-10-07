#include "EchoGameState.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "RelayYardBuilder.h"

AEchoGameState::AEchoGameState()
{
}

void AEchoGameState::BeginPlay()
{
	Super::BeginPlay();

	// The server's game mode spawns the level. Clients build their own copy here.
	if (HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ARelayYardBuilder> It(World); It; ++It)
	{
		LevelBuilder = *It;
		break;
	}

	if (!LevelBuilder)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		LevelBuilder = World->SpawnActor<ARelayYardBuilder>(ARelayYardBuilder::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
}

void AEchoGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoGameState, TimeLeft);
	DOREPLIFETIME(AEchoGameState, bMatchOver);
	DOREPLIFETIME(AEchoGameState, WinnerName);
	DOREPLIFETIME(AEchoGameState, KillFeed);
	DOREPLIFETIME(AEchoGameState, BotScores);
	DOREPLIFETIME(AEchoGameState, BotDeaths);
}

void AEchoGameState::AddKillFeedEntry(const FString& KillerName, const FString& VictimName)
{
	FEchoKillFeedEntry Entry;
	Entry.KillerName = KillerName;
	Entry.VictimName = VictimName;
	Entry.TimeStamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	KillFeed.Add(Entry);

	while (KillFeed.Num() > 5)
	{
		KillFeed.RemoveAt(0);
	}
}

void AEchoGameState::AddBotKill(const FString& BotName)
{
	int32& Score = BotScores.FindOrAdd(BotName);
	Score++;
}

void AEchoGameState::AddBotDeath(const FString& BotName)
{
	int32& Deaths = BotDeaths.FindOrAdd(BotName);
	Deaths++;
}

void AEchoGameState::ClearBotStats()
{
	BotScores.Reset();
	BotDeaths.Reset();
}
