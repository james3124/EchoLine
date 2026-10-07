// EchoLine - the signature Echo Link mechanic.
// A player arms Echo, fires at a tagged panel to drop anchor A, then drops anchor B.
// Shots fired into A exit from B (portal style). Enemies can destroy anchors.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchoLinkComponent.generated.h"

class AEchoAnchor;
class AEchoLineCharacter;

UCLASS(ClassGroup = (EchoLine), meta = (BlueprintSpawnableComponent))
class ECHOLINE_API UEchoLinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEchoLinkComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** How far a player may place an anchor from their eye. */
	UPROPERTY(EditDefaultsOnly, Category = "Echo")
	float MaxPlaceDistance = 4000.0f;

	/** Minimum time between anchor placements. */
	UPROPERTY(EditDefaultsOnly, Category = "Echo")
	float PlaceCooldown = 1.5f;

	/** Maximum simultaneous anchors per player. The oldest is recycled. */
	UPROPERTY(EditDefaultsOnly, Category = "Echo")
	int32 MaxAnchors = 2;

	/** Anchor lifetime in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Echo")
	float AnchorLifetime = 12.0f;

	/** Radius used when a shot is tested against an anchor. */
	UPROPERTY(EditDefaultsOnly, Category = "Echo")
	float AnchorHitRadius = 26.0f;

	void SetEchoArmed(bool bArmed);
	bool IsEchoArmed() const { return bEchoArmed; }
	void ToggleEcho() { SetEchoArmed(!bEchoArmed); }

	/** Replicated so the HUD can show anchor status on every client. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo")
	int32 AnchorCount = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Echo")
	bool bLinked = false;

	int32 GetAnchorCount() const { return AnchorCount; }
	bool HasLink() const { return bLinked; }
	float GetCooldownRemaining() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<TObjectPtr<AEchoAnchor>>& GetAnchors() const { return Anchors; }

	/** Server entry point: trace for a panel and place an anchor. Returns true on success. */
	bool ServerTryPlaceAnchor(const FVector& Origin, const FVector& AimDir);

	/** Server-side: if the shot enters one of my linked anchors, output the exit point. */
	bool TryGetRedirect(const FVector& Origin, const FVector& Dir, FVector& OutOrigin, FVector& OutDir) const;

	/** Removes an anchor from the tracked list (called when it is destroyed or expires). */
	void NotifyAnchorDestroyed(AEchoAnchor* Anchor);

	void DestroyAllAnchors();

protected:
	/** Anchor currently owned by this component. Server authoritative. */
	UPROPERTY()
	TArray<TObjectPtr<AEchoAnchor>> Anchors;

	UFUNCTION()
	void HandleAnchorDestroyed(AActor* DestroyedActor);

	/** Refreshes the replicated counters from the authoritative anchor list. */
	void RefreshReplicatedState();

	bool bEchoArmed = false;
	double LastPlaceTime = -1000.0;

	AEchoLineCharacter* GetEchoCharacter() const;
};
