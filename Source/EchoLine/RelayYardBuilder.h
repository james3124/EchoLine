// EchoLine - procedural greybox of the Relay Yard map.
// Building geometry in C++ means the prototype needs no imported assets or hand-authored map.
// A level artist can replace this later with a real .umap; nothing else depends on it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoNavigationGrid.h"
#include "RelayYardBuilder.generated.h"

class AStaticMeshActor;
class APlayerStart;
class UStaticMesh;
class AEchoPickup;

UCLASS()
class ECHOLINE_API ARelayYardBuilder : public AActor
{
	GENERATED_BODY()

public:
	ARelayYardBuilder();

	virtual void BeginPlay() override;

	/** Builds all geometry, lights and spawn points. Safe to call once. */
	void BuildLevel();

	/** Spawn transforms used by the game mode when respawning players. */
	const TArray<FTransform>& GetSpawnTransforms() const { return SpawnTransforms; }

	/** PlayerStart actors created for the level. */
	const TArray<TObjectPtr<APlayerStart>>& GetSpawnPoints() const { return SpawnPoints; }

	/** True if the level has already been built. */
	bool IsBuilt() const { return bBuilt; }

	/** Navigation grid rasterised from the level's collision boxes. */
	const FEchoNavGrid& GetNavGrid() const { return NavGrid; }

	/** Pickup actors placed in the level, for bots to seek. */
	const TArray<TObjectPtr<AEchoPickup>>& GetPickups() const { return Pickups; }

protected:
	/** Spawns one box. HalfExtent is in cm; Color tints the box if the material supports it. */
	AStaticMeshActor* SpawnBlock(const FVector& Center, const FVector& HalfExtent,
		const FLinearColor& Color, bool bCollision, bool bEchoPanel, FName Tag);

	void SpawnLighting();
	void SpawnPickups();
	void BuildNavigation();

	/** Collision boxes recorded while building, used to rasterise navigation. */
	TArray<FBox> ObstacleBoxes;

	FEchoNavGrid NavGrid;

	UPROPERTY()
	TArray<TObjectPtr<AEchoPickup>> Pickups;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedActors;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	TArray<FTransform> SpawnTransforms;

	UPROPERTY()
	TArray<TObjectPtr<APlayerStart>> SpawnPoints;

	bool bBuilt = false;
};
