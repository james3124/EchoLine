#include "EchoAnchor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AEchoAnchor::AEchoAnchor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Enemies can shoot anchors, and the server traces against them on Visibility.
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
	// Basic sphere is 100 cm across; a 0.25 scale makes a 25 cm anchor.
	Mesh->SetRelativeScale3D(FVector(0.25f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetIntensity(1500.0f);
	Glow->SetAttenuationRadius(260.0f);
	Glow->SetCastShadows(false);
	Glow->SetLightColor(FLinearColor(0.10f, 0.55f, 1.00f));
}

void AEchoAnchor::InitAnchor(int32 InOwnerId, const FVector& InNormal, const FLinearColor& InColor)
{
	OwnerId = InOwnerId;
	SurfaceNormal = InNormal.GetSafeNormal();
	AnchorColor = InColor;
	UpdateVisuals();
}

void AEchoAnchor::SetPartner(AEchoAnchor* InPartner)
{
	Partner = InPartner;
	UpdateVisuals();
}

void AEchoAnchor::OnRep_Partner()
{
	UpdateVisuals();
}

void AEchoAnchor::UpdateVisuals()
{
	// Linked anchors switch to a warmer colour so the pair reads clearly.
	const FLinearColor Color = IsLinked() ? FLinearColor(0.15f, 1.0f, 0.45f) : AnchorColor;

	if (Glow)
	{
		Glow->SetLightColor(Color);
	}

	if (Mesh)
	{
		UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0);
		if (MID)
		{
			// BasicShapeMaterial exposes "Color"; harmless if the parameter is absent.
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

void AEchoAnchor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PulseTime += DeltaSeconds;

	if (Mesh)
	{
		const float Pulse = 1.0f + 0.10f * FMath::Sin(PulseTime * 6.0f);
		Mesh->SetRelativeScale3D(FVector(0.25f * Pulse));
	}
}

void AEchoAnchor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoAnchor, OwnerId);
	DOREPLIFETIME(AEchoAnchor, SurfaceNormal);
	DOREPLIFETIME(AEchoAnchor, AnchorColor);
	DOREPLIFETIME(AEchoAnchor, Partner);
}
