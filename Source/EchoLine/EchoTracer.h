// EchoLine - short-lived bullet tracer. Purely cosmetic, spawned locally on each client.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoTracer.generated.h"

class UStaticMeshComponent;

UCLASS()
class ECHOLINE_API AEchoTracer : public AActor
{
	GENERATED_BODY()

public:
	AEchoTracer();

	/** Stretch the tracer between two points in the world. */
	void SetSegment(const FVector& Start, const FVector& End);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Tracer")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
