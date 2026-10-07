#include "RelayYardBuilder.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "EchoPickup.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Greybox palette.
	const FLinearColor GroundColor(0.16f, 0.16f, 0.17f);
	const FLinearColor WallColor(0.28f, 0.28f, 0.30f);
	const FLinearColor BuildingColor(0.34f, 0.33f, 0.30f);
	const FLinearColor ContainerColor(0.22f, 0.34f, 0.30f);
	const FLinearColor CoverColor(0.30f, 0.26f, 0.22f);
	const FLinearColor DishColor(0.40f, 0.40f, 0.42f);
	const FLinearColor PanelColor(0.10f, 0.45f, 0.90f);
}

ARelayYardBuilder::ARelayYardBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
}

void ARelayYardBuilder::BeginPlay()
{
	Super::BeginPlay();
	BuildLevel();
}

AStaticMeshActor* ARelayYardBuilder::SpawnBlock(const FVector& Center, const FVector& HalfExtent,
	const FLinearColor& Color, bool bCollision, bool bEchoPanel, FName Tag)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Center, FRotator::ZeroRotator, Params);
	if (!Actor)
	{
		return nullptr;
	}

	UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
	Comp->SetMobility(EComponentMobility::Movable);
	if (CubeMesh)
	{
		Comp->SetStaticMesh(CubeMesh);
	}

	// Engine cube is 100 cm per side, so scale = extent * 2 / 100.
	Comp->SetWorldScale3D(HalfExtent / 50.0f);

	if (bCollision)
	{
		// QueryOnly is enough: player movement sweeps and weapon traces both use queries.
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Comp->SetCollisionResponseToAllChannels(ECR_Block);
	}
	else
	{
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(0))
	{
		// BasicShapeMaterial exposes a "Color" vector parameter.
		MID->SetVectorParameterValue(TEXT("Color"), bEchoPanel ? PanelColor : Color);
	}

	if (bEchoPanel)
	{
		Actor->Tags.Add(TEXT("EchoPanel"));
	}
	if (!Tag.IsNone())
	{
		Actor->Tags.Add(Tag);
	}

	// Record solid boxes for navigation rasterisation.
	if (bCollision)
	{
		ObstacleBoxes.Add(FBox(Center - HalfExtent, Center + HalfExtent));
	}

	SpawnedActors.Add(Actor);
	return Actor;
}

void ARelayYardBuilder::SpawnLighting()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Late-afternoon key light.
	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(),
		FVector(0.0f, 0.0f, 2000.0f), FRotator(-42.0f, 35.0f, 0.0f)))
	{
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			Light->SetIntensity(6.0f);
			Light->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f));
			Light->SetMobility(EComponentMobility::Movable);
		}
		SpawnedActors.Add(Sun);
	}

	// Ambient fill so interiors are readable.
	if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(0.0f, 0.0f, 1000.0f), FRotator::ZeroRotator))
	{
		if (USkyLightComponent* Light = Sky->GetLightComponent())
		{
			Light->SetIntensity(1.5f);
			Light->SetMobility(EComponentMobility::Movable);
			Light->RecaptureSky();
		}
		SpawnedActors.Add(Sky);
	}
}

void ARelayYardBuilder::BuildLevel()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// --- Ground --- (60 m x 60 m)
	SpawnBlock(FVector(0.0f, 0.0f, -25.0f), FVector(3000.0f, 3000.0f, 25.0f), GroundColor, true, false, TEXT("Ground"));

	// --- Perimeter walls --- (4 m tall, keeps players inside)
	const float Half = 3000.0f;
	const float WallH = 200.0f;
	SpawnBlock(FVector(0.0f, Half, WallH), FVector(Half, 25.0f, WallH), WallColor, true, false, NAME_None);
	SpawnBlock(FVector(0.0f, -Half, WallH), FVector(Half, 25.0f, WallH), WallColor, true, false, NAME_None);
	SpawnBlock(FVector(Half, 0.0f, WallH), FVector(25.0f, Half, WallH), WallColor, true, false, NAME_None);
	SpawnBlock(FVector(-Half, 0.0f, WallH), FVector(25.0f, Half, WallH), WallColor, true, false, NAME_None);

	// --- Central communications dish ---
	SpawnBlock(FVector(0.0f, 0.0f, 100.0f), FVector(300.0f, 300.0f, 100.0f), DishColor, true, false, TEXT("DishBase"));
	// Low ring cover around the dish base.
	SpawnBlock(FVector(0.0f, 650.0f, 60.0f), FVector(350.0f, 40.0f, 60.0f), CoverColor, true, false, NAME_None);
	SpawnBlock(FVector(0.0f, -650.0f, 60.0f), FVector(350.0f, 40.0f, 60.0f), CoverColor, true, false, NAME_None);

	// --- North service building (z negative) ---
	// Walls leave a 4 m doorway facing the yard.
	SpawnBlock(FVector(-700.0f, -1700.0f, 175.0f), FVector(500.0f, 25.0f, 175.0f), BuildingColor, true, false, NAME_None);
	SpawnBlock(FVector(700.0f, -1700.0f, 175.0f), FVector(500.0f, 25.0f, 175.0f), BuildingColor, true, false, NAME_None);
	SpawnBlock(FVector(-1150.0f, -2300.0f, 175.0f), FVector(25.0f, 600.0f, 175.0f), BuildingColor, true, false, NAME_None);
	SpawnBlock(FVector(1150.0f, -2300.0f, 175.0f), FVector(25.0f, 600.0f, 175.0f), BuildingColor, true, false, NAME_None);
	SpawnBlock(FVector(0.0f, -2850.0f, 175.0f), FVector(1150.0f, 25.0f, 175.0f), BuildingColor, true, false, NAME_None);
	// Interior cover.
	SpawnBlock(FVector(-600.0f, -2400.0f, 55.0f), FVector(90.0f, 90.0f, 55.0f), CoverColor, true, false, NAME_None);
	SpawnBlock(FVector(600.0f, -2400.0f, 55.0f), FVector(90.0f, 90.0f, 55.0f), CoverColor, true, false, NAME_None);

	// --- South service building (z positive), mirrored openings ---
	SpawnBlock(FVector(-700.0f, 1700.0f, 175.0f), FVector(500.0f, 25.0f, 175.0f), BuildingColor, true, false, NAME_None);
	SpawnBlock(FVector(700.0f, 1700.0f, 175.0f), FVector(500.0f, 25.0f, 175.0f), BuildingColor, true, false, NAME_None);
	SpawnBlock(FVector(-1150.0f, 2300.0f, 175.0f), FVector(25.0f, 600.0f, 175.0f), BuildingColor, true, false, NAME_None);
	SpawnBlock(FVector(1150.0f, 2300.0f, 175.0f), FVector(25.0f, 600.0f, 175.0f), BuildingColor, true, false, NAME_None);
	SpawnBlock(FVector(0.0f, 2850.0f, 175.0f), FVector(1150.0f, 25.0f, 175.0f), BuildingColor, true, false, NAME_None);
	SpawnBlock(FVector(-600.0f, 2400.0f, 55.0f), FVector(90.0f, 90.0f, 55.0f), CoverColor, true, false, NAME_None);
	SpawnBlock(FVector(600.0f, 2400.0f, 55.0f), FVector(90.0f, 90.0f, 55.0f), CoverColor, true, false, NAME_None);

	// --- West and East flanks: container stacks ---
	SpawnBlock(FVector(-2000.0f, -900.0f, 120.0f), FVector(300.0f, 125.0f, 120.0f), ContainerColor, true, false, NAME_None);
	SpawnBlock(FVector(-2000.0f, 300.0f, 120.0f), FVector(300.0f, 125.0f, 120.0f), ContainerColor, true, false, NAME_None);
	SpawnBlock(FVector(-2000.0f, 1500.0f, 120.0f), FVector(300.0f, 125.0f, 120.0f), ContainerColor, true, false, NAME_None);

	SpawnBlock(FVector(2000.0f, -900.0f, 120.0f), FVector(300.0f, 125.0f, 120.0f), ContainerColor, true, false, NAME_None);
	SpawnBlock(FVector(2000.0f, 300.0f, 120.0f), FVector(300.0f, 125.0f, 120.0f), ContainerColor, true, false, NAME_None);
	SpawnBlock(FVector(2000.0f, 1500.0f, 120.0f), FVector(300.0f, 125.0f, 120.0f), ContainerColor, true, false, NAME_None);

	// Loose cover in the central lane.
	SpawnBlock(FVector(-900.0f, 0.0f, 55.0f), FVector(90.0f, 90.0f, 55.0f), CoverColor, true, false, NAME_None);
	SpawnBlock(FVector(900.0f, 0.0f, 55.0f), FVector(90.0f, 90.0f, 55.0f), CoverColor, true, false, NAME_None);

	// --- Echo Link panels ---
	// Thin, clearly tinted plates. Tagged EchoPanel so anchors may be placed on them.
	// Two near the dish (central play) and one on each flank.
	SpawnBlock(FVector(-1250.0f, -300.0f, 150.0f), FVector(20.0f, 250.0f, 150.0f), PanelColor, true, true, NAME_None);
	SpawnBlock(FVector(1250.0f, 300.0f, 150.0f), FVector(20.0f, 250.0f, 150.0f), PanelColor, true, true, NAME_None);
	SpawnBlock(FVector(-1700.0f, 2100.0f, 150.0f), FVector(250.0f, 20.0f, 150.0f), PanelColor, true, true, NAME_None);
	SpawnBlock(FVector(1700.0f, -2100.0f, 150.0f), FVector(250.0f, 20.0f, 150.0f), PanelColor, true, true, NAME_None);

	// --- Spawn points (four corners) ---
	SpawnTransforms.Reset();
	const float SpawnCoord = 2450.0f;
	const FVector SpawnLocations[4] = {
		FVector(-SpawnCoord, -SpawnCoord, 100.0f),
		FVector(SpawnCoord, -SpawnCoord, 100.0f),
		FVector(-SpawnCoord, SpawnCoord, 100.0f),
		FVector(SpawnCoord, SpawnCoord, 100.0f)
	};

	for (int32 i = 0; i < 4; ++i)
	{
		SpawnTransforms.Add(FTransform(FRotator(0.0f, (i % 2 == 0) ? 45.0f : -45.0f, 0.0f), SpawnLocations[i]));

		// APlayerStart actors keep compatibility with UE's default spawn logic.
		if (APlayerStart* Start = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), SpawnLocations[i], FRotator::ZeroRotator))
		{
			SpawnedActors.Add(Start);
			SpawnPoints.Add(Start);
		}
	}

	SpawnPickups();
	SpawnLighting();
	BuildNavigation();
}

void ARelayYardBuilder::SpawnPickups()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	struct FPickupSpec
	{
		FVector Location;
		EEchoPickupType Type;
	};

	const FPickupSpec Specs[] = {
		{ FVector(-1500.0f, 0.0f, 30.0f), EEchoPickupType::Health },
		{ FVector(1500.0f, 0.0f, 30.0f), EEchoPickupType::Health },
		{ FVector(0.0f, -1700.0f, 30.0f), EEchoPickupType::Ammo },
		{ FVector(0.0f, 1700.0f, 30.0f), EEchoPickupType::Ammo }
	};

	for (const FPickupSpec& Spec : Specs)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AEchoPickup* Pickup = World->SpawnActor<AEchoPickup>(AEchoPickup::StaticClass(), Spec.Location, FRotator::ZeroRotator, Params);
		if (Pickup)
		{
			Pickup->Type = Spec.Type;
			Pickups.Add(Pickup);
		}
	}
}

void ARelayYardBuilder::BuildNavigation()
{
	// Rasterise a navigation grid from the recorded obstacle boxes.
	// 100 cm cells give a good balance of accuracy and search speed for a 60 m map.
	const FVector2D GridMin(-3000.0f, -3000.0f);
	const FVector2D GridMax(3000.0f, 3000.0f);
	NavGrid.Build(GridMin, GridMax, 100.0f, ObstacleBoxes, 55.0f, 120.0f);
}
