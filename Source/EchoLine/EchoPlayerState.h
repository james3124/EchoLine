// EchoLine - per-player replicated state.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "EchoTypes.h"
#include "EchoPlayerState.generated.h"

UCLASS()
class ECHOLINE_API AEchoPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AEchoPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo")
	int32 Deaths = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo")
	EOperatorId Operator = EOperatorId::Rook;
};
