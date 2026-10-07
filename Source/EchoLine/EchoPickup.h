// EchoLine - pickups (healing and ammo). Server authoritative, respawn on a timer.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "EchoPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EEchoPickupType : uint8
{
	Health		UMETA(DisplayName = "Medkit"),
	Ammo		UMETA(DisplayName = "Ammo Box")
};

UCLASS()
class ECHOLINE_API AEchoPickup : public AActor
{
	GENERATED_BODY()

public:
	AEchoPickup();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Category = "Pickup")
	EEchoPickupType Type = EEchoPickupType::Health;

	/** Health restored or ammo granted on pickup. */
	UPROPERTY(EditAnywhere, Category = "Pickup")
	float Amount = 40.0f;

	/** Seconds before the pickup reappears. */
	UPROPERTY(EditAnywhere, Category = "Pickup")
	float RespawnSeconds = 15.0f;

	bool IsAvailable() const { return bAvailable; }

	/** True when this actor can be used right now by the given character. */
	bool IsUsefulFor(const class AEchoLineCharacter* Character) const;

protected:
	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_Available();

	void ApplyVisualState();
	void RespawnPickup();

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(ReplicatedUsing = OnRep_Available)
	bool bAvailable = true;
};
