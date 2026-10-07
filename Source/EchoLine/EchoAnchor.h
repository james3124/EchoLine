// EchoLine - a placed Echo Link anchor. Spawned by the server and replicated.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoAnchor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class ECHOLINE_API AEchoAnchor : public AActor
{
	GENERATED_BODY()

public:
	AEchoAnchor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server-side initialisation after spawning. */
	void InitAnchor(int32 InOwnerId, const FVector& InNormal, const FLinearColor& InColor);

	void SetPartner(AEchoAnchor* InPartner);
	AEchoAnchor* GetPartner() const { return Partner; }

	int32 GetOwnerId() const { return OwnerId; }
	FVector GetSurfaceNormal() const { return SurfaceNormal; }
	FLinearColor GetAnchorColor() const { return AnchorColor; }

	/** Two linked anchors redirect shots between them. */
	bool IsLinked() const { return Partner != nullptr && Partner != this; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Echo")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Echo")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(Replicated)
	int32 OwnerId = -1;

	UPROPERTY(Replicated)
	FVector SurfaceNormal = FVector::ForwardVector;

	UPROPERTY(Replicated)
	FLinearColor AnchorColor = FLinearColor(0.10f, 0.55f, 1.00f);

	UPROPERTY(ReplicatedUsing = OnRep_Partner)
	TObjectPtr<AEchoAnchor> Partner;

	UFUNCTION()
	void OnRep_Partner();

	/** Applies the current colour to the glow and to the mesh material if possible. */
	void UpdateVisuals();

	float PulseTime = 0.0f;
};
