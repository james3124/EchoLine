#include "EchoTracer.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AEchoTracer::AEchoTracer()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}

	InitialLifeSpan = 0.05f;
}

void AEchoTracer::SetSegment(const FVector& Start, const FVector& End)
{
	const FVector Delta = End - Start;
	const float Length = Delta.Size();

	if (Length < KINDA_SMALL_NUMBER)
	{
		Destroy();
		return;
	}

	SetActorLocation((Start + End) * 0.5f);
	SetActorRotation(Delta.Rotation());

	// Engine cylinder is 100 cm tall on Z; scale X/Y thin and Z to the segment length.
	Mesh->SetWorldScale3D(FVector(0.03f, 0.03f, Length / 100.0f));
}
