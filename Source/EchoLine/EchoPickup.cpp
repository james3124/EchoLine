#include "EchoPickup.h"

#include "Components/PrimitiveComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EchoAudio.h"
#include "EchoLineCharacter.h"
#include "EchoWeaponComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AEchoPickup::AEchoPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->InitSphereRadius(70.0f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sphere->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.4f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CubeFinder.Object);
	}

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Sphere);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetIntensity(1200.0f);
	Glow->SetAttenuationRadius(220.0f);
	Glow->SetCastShadows(false);
}

void AEchoPickup::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Sphere->OnComponentBeginOverlap.AddDynamic(this, &AEchoPickup::OnSphereBeginOverlap);
	}

	ApplyVisualState();
}

void AEchoPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoPickup, bAvailable);
}

bool AEchoPickup::IsUsefulFor(const AEchoLineCharacter* Character) const
{
	if (!Character || Character->IsDead())
	{
		return false;
	}

	if (Type == EEchoPickupType::Health)
	{
		return Character->GetHealth() < 100.0f;
	}
	return true;
}

void AEchoPickup::OnSphereBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!HasAuthority() || !bAvailable)
	{
		return;
	}

	AEchoLineCharacter* Character = Cast<AEchoLineCharacter>(OtherActor);
	if (!IsUsefulFor(Character))
	{
		return;
	}

	if (Type == EEchoPickupType::Health)
	{
		Character->Heal(Amount);
	}
	else if (Character->Weapon)
	{
		Character->Weapon->RefillAmmo();
	}

	bAvailable = false;
	ApplyVisualState();

	// Pickup cue.
	UEchoAudioSubsystem::Play(this, EEchoSfx::Pickup, GetActorLocation(), false);

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, this, &AEchoPickup::RespawnPickup, RespawnSeconds, false);
}

void AEchoPickup::RespawnPickup()
{
	bAvailable = true;
	ApplyVisualState();
}

void AEchoPickup::OnRep_Available()
{
	ApplyVisualState();
}

void AEchoPickup::ApplyVisualState()
{
	SetActorHiddenInGame(!bAvailable);
	SetActorEnableCollision(bAvailable);

	if (Glow)
	{
		const FLinearColor Color = (Type == EEchoPickupType::Health)
			? FLinearColor(0.15f, 0.9f, 0.35f)
			: FLinearColor(0.95f, 0.75f, 0.2f);
		Glow->SetLightColor(Color);
		Glow->SetIntensity(bAvailable ? 1200.0f : 0.0f);
	}

	if (Mesh)
	{
		if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
		{
			const FLinearColor Color = (Type == EEchoPickupType::Health)
				? FLinearColor(0.15f, 0.9f, 0.35f)
				: FLinearColor(0.95f, 0.75f, 0.2f);
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}
