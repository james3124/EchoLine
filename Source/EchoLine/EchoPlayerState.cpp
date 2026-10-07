#include "EchoPlayerState.h"

#include "Net/UnrealNetwork.h"

AEchoPlayerState::AEchoPlayerState()
{
	// Firearms are automatic when the trigger is held, with client-paced requests.
	// PlayerState replicates by default.
}

void AEchoPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoPlayerState, Deaths);
	DOREPLIFETIME(AEchoPlayerState, Operator);
}
