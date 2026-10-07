#include "EchoGameMode.h"

#include "EchoBotController.h"
#include "EchoGameState.h"
#include "EchoHUD.h"
#include "EchoLineCharacter.h"
#include "EchoLinePlayerController.h"
#include "EchoPickup.h"
#include "EchoPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "RelayYardBuilder.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

AEchoGameMode::AEchoGameMode()
{
	DefaultPawnClass = AEchoLineCharacter::StaticClass();
	PlayerControllerClass = AEchoPlayerController::StaticClass();
	HUDClass = AEchoHUD::StaticClass();
	GameStateClass = AEchoGameState::StaticClass();
	PlayerStateClass = AEchoPlayerState::StaticClass();

	// The match timer is advanced from a repeating timer (see BeginPlay).
	PrimaryActorTick.bCanEverTick = false;
}

void AEchoGameMode::BeginPlay()
{
	Super::BeginPlay();

	// The server owns the authoritative level. Clients build their own copy in GameState.
	EnsureBuilder();

	if (AEchoGameState* GS = GetGameState<AEchoGameState>())
	{
		GS->TimeLeft = MatchDuration;
		GS->bMatchOver = false;
	}

	// Drive the round clock from a timer so we do not depend on the game mode ticking.
	GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &AEchoGameMode::UpdateMatchTimer, MatchTickInterval, true);

	// Spawn bots shortly after the level exists.
	if (BotCount > 0)
	{
		GetWorldTimerManager().SetTimer(BotSpawnTimer, this, &AEchoGameMode::SpawnBot, 1.0f, true, 1.0f);
	}
}

void AEchoGameMode::SpawnBot()
{
	if (BotsSpawned >= BotCount)
	{
		GetWorldTimerManager().ClearTimer(BotSpawnTimer);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Choose a spawn point.
	FTransform SpawnTransform = FTransform::Identity;
	ARelayYardBuilder* LevelBuilder = EnsureBuilder();
	if (LevelBuilder && LevelBuilder->GetSpawnTransforms().Num() > 0)
	{
		const TArray<FTransform>& Spawns = LevelBuilder->GetSpawnTransforms();
		SpawnTransform = Spawns[BotsSpawned % Spawns.Num()];
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AEchoBotController* BotController = World->SpawnActor<AEchoBotController>(AEchoBotController::StaticClass(), SpawnTransform, Params);
	if (!BotController)
	{
		return;
	}

	AEchoLineCharacter* Pawn = World->SpawnActor<AEchoLineCharacter>(AEchoLineCharacter::StaticClass(), SpawnTransform, Params);
	if (!Pawn)
	{
		BotController->Destroy();
		return;
	}

	Pawn->bBot = true;
	Pawn->BotName = FString::Printf(TEXT("Bot %d"), BotsSpawned + 1);

	BotController->Possess(Pawn);

	++BotsSpawned;
}

void AEchoGameMode::AddBot()
{
	// Allow adding past the initial count, up to a sane cap.
	if (BotsSpawned >= 8)
	{
		return;
	}
	++BotCount;
	SpawnBot();
}

const FEchoNavGrid* AEchoGameMode::GetNavGrid() const
{
	return Builder ? &Builder->GetNavGrid() : nullptr;
}

const TArray<TObjectPtr<AEchoPickup>>& AEchoGameMode::GetPickups() const
{
	static const TArray<TObjectPtr<AEchoPickup>> Empty;
	return Builder ? Builder->GetPickups() : Empty;
}

ARelayYardBuilder* AEchoGameMode::EnsureBuilder()
{
	if (Builder)
	{
		return Builder;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ARelayYardBuilder> It(World); It; ++It)
	{
		Builder = *It;
		break;
	}

	if (!Builder)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Builder = World->SpawnActor<ARelayYardBuilder>(ARelayYardBuilder::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}

	if (Builder && !Builder->IsBuilt())
	{
		Builder->BuildLevel();
	}

	return Builder;
}

void AEchoGameMode::UpdateMatchTimer()
{
	AEchoGameState* GS = GetGameState<AEchoGameState>();
	if (!GS || GS->bMatchOver)
	{
		return;
	}

	GS->TimeLeft = FMath::Max(0.0f, GS->TimeLeft - MatchTickInterval);

	if (GS->TimeLeft <= 0.0f)
	{
		EndMatch();
	}
}

AActor* AEchoGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Make sure the procedural level exists before the first spawn.
	ARelayYardBuilder* LevelBuilder = EnsureBuilder();

	// Prefer the spawn farthest from any living enemy.
	if (!LevelBuilder || LevelBuilder->GetSpawnPoints().Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const TArray<TObjectPtr<APlayerStart>>& Spawns = LevelBuilder->GetSpawnPoints();
	int32 BestIndex = FMath::RandRange(0, Spawns.Num() - 1);
	float BestScore = -1.0f;

	for (int32 i = 0; i < Spawns.Num(); ++i)
	{
		if (!Spawns[i])
		{
			continue;
		}

		float NearestEnemy = TNumericLimits<float>::Max();

		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || PC == Player)
			{
				continue;
			}
			if (AEchoLineCharacter* Other = Cast<AEchoLineCharacter>(PC->GetPawn()))
			{
				if (!Other->IsDead())
				{
					NearestEnemy = FMath::Min(NearestEnemy, FVector::Dist(Other->GetActorLocation(), Spawns[i]->GetActorLocation()));
				}
			}
		}

		if (NearestEnemy > BestScore)
		{
			BestScore = NearestEnemy;
			BestIndex = i;
		}
	}

	return Spawns[BestIndex];
}

void AEchoGameMode::NotifyKill(AController* Killer, AEchoLineCharacter* Victim, AEchoLineCharacter* KillerPawn)
{
	if (!Victim)
	{
		return;
	}

	AEchoPlayerState* VictimState = Victim->GetPlayerState<AEchoPlayerState>();
	FString KillerName = TEXT("Environment");
	FString VictimName = Victim->GetDisplayName();

	if (VictimState)
	{
		VictimState->Deaths++;
	}

	// A bot killer has no PlayerState, so it is attributed through its pawn.
	if (KillerPawn && KillerPawn != Victim && KillerPawn->bBot)
	{
		KillerName = KillerPawn->GetDisplayName();
		if (AEchoGameState* GS = GetGameState<AEchoGameState>())
		{
			GS->AddBotKill(KillerName);
		}
	}
	else if (Killer && Killer != Victim->GetController())
	{
		if (AEchoPlayerState* KillerState = Killer->GetPlayerState<AEchoPlayerState>())
		{
			KillerState->SetScore(KillerState->GetScore() + 1.0f);
			KillerName = KillerState->GetPlayerName();
		}
	}

	if (AEchoGameState* GS = GetGameState<AEchoGameState>())
	{
		if (Victim->bBot)
		{
			GS->AddBotDeath(VictimName);
		}
		GS->AddKillFeedEntry(KillerName, VictimName);
	}

	// Respawn the victim (works for both players and bots) after a delay.
	if (RespawnDelay > 0.0f)
	{
		if (Victim->bBot)
		{
			FTimerHandle Handle;
			FTimerDelegate Delegate;
			Delegate.BindUObject(this, &AEchoGameMode::RespawnBot, Victim);
			GetWorldTimerManager().SetTimer(Handle, Delegate, RespawnDelay, false);
		}
		else
		{
			AController* VictimController = Victim->GetController();
			if (VictimController)
			{
				FTimerHandle Handle;
				FTimerDelegate Delegate;
				Delegate.BindUObject(this, &AEchoGameMode::RespawnPlayer, VictimController);
				GetWorldTimerManager().SetTimer(Handle, Delegate, RespawnDelay, false);
			}
		}
	}
}

void AEchoGameMode::BotsSpawnedThisRoundScore(AEchoLineCharacter* BotPawn)
{
	// Track bot scores in a simple map; they are shown via the pawn, not a PlayerState.
	int32& Score = BotScores.FindOrAdd(BotPawn);
	Score++;
}

int32 AEchoGameMode::GetBotScore(const AEchoLineCharacter* BotPawn) const
{
	const int32* Found = BotScores.Find(BotPawn);
	return Found ? *Found : 0;
}

void AEchoGameMode::RespawnBot(AEchoLineCharacter* Bot)
{
	if (!Bot || !Builder)
	{
		return;
	}

	const TArray<FTransform>& Spawns = Builder->GetSpawnTransforms();
	if (Spawns.Num() == 0)
	{
		return;
	}

	Bot->RespawnAt(Spawns[FMath::RandRange(0, Spawns.Num() - 1)]);
}

void AEchoGameMode::RespawnPlayer(AController* Controller)
{
	AEchoLineCharacter* Character = Controller ? Cast<AEchoLineCharacter>(Controller->GetPawn()) : nullptr;
	if (!Character || !Builder)
	{
		return;
	}

	const TArray<FTransform>& Spawns = Builder->GetSpawnTransforms();
	if (Spawns.Num() == 0)
	{
		return;
	}

	const FTransform& Spawn = Spawns[FMath::RandRange(0, Spawns.Num() - 1)];
	Character->RespawnAt(Spawn);
}

void AEchoGameMode::EndMatch()
{
	AEchoGameState* GS = GetGameState<AEchoGameState>();
	if (!GS)
	{
		return;
	}

	GS->bMatchOver = true;

	// Determine the winner.
	FString Winner = TEXT("No Contest");
	float BestScore = -1.0f;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->PlayerState && PC->PlayerState->GetScore() > BestScore)
		{
			BestScore = PC->PlayerState->GetScore();
			Winner = PC->PlayerState->GetPlayerName();
		}
	}
	GS->WinnerName = Winner;

	GetWorldTimerManager().SetTimer(RoundResetTimer, this, &AEchoGameMode::ResetRound, RoundResetDelay, false);
}

void AEchoGameMode::ResetRound()
{
	AEchoGameState* GS = GetGameState<AEchoGameState>();
	if (!GS)
	{
		return;
	}

	GS->bMatchOver = false;
	GS->TimeLeft = MatchDuration;
	GS->KillFeed.Reset();
	GS->ClearBotStats();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		if (AEchoPlayerState* PS = PC->GetPlayerState<AEchoPlayerState>())
		{
			PS->SetScore(0.0f);
			PS->Deaths = 0;
		}
		RespawnPlayer(PC);
	}
}
