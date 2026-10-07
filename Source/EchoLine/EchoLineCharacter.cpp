#include "EchoLineCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EchoArt.h"
#include "EchoAudio.h"
#include "EchoGameMode.h"
#include "EchoLinkComponent.h"
#include "EchoLinePlayerController.h"
#include "EchoWeaponComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AEchoLineCharacter::AEchoLineCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	GetCapsuleComponent()->InitCapsuleSize(40.0f, 88.0f);

	// First-person camera at eye height.
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(BaseFieldOfView);

	// --- Operator body: assembled from primitives for a distinct silhouette ---
	// Parts are attached to the capsule and hidden from the owner. Colours are applied
	// in BeginPlay (dynamic materials need registered components).
	UStaticMesh* CubeMesh = EchoArt::Cube();
	UStaticMesh* SphereMesh = EchoArt::Sphere();

	auto MakeBodyPart = [this](const FName& Name, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(GetCapsuleComponent());
		Part->SetOwnerNoSee(true);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		if (Mesh)
		{
			Part->SetStaticMesh(Mesh);
		}
		Part->SetRelativeLocation(Loc);
		Part->SetRelativeScale3D(Scale);
		return Part;
	};

	// Torso and pelvis.
	PrimaryParts.Add(MakeBodyPart(TEXT("Torso"), CubeMesh, FVector(0.0f, 0.0f, 15.0f), FVector(0.42f, 0.26f, 0.45f)));
	SecondaryParts.Add(MakeBodyPart(TEXT("Pelvis"), CubeMesh, FVector(0.0f, 0.0f, -30.0f), FVector(0.34f, 0.24f, 0.22f)));
	// Legs.
	PrimaryParts.Add(MakeBodyPart(TEXT("LegL"), CubeMesh, FVector(0.0f, 12.0f, -60.0f), FVector(0.14f, 0.13f, 0.34f)));
	PrimaryParts.Add(MakeBodyPart(TEXT("LegR"), CubeMesh, FVector(0.0f, -12.0f, -60.0f), FVector(0.14f, 0.13f, 0.34f)));
	// Arms.
	PrimaryParts.Add(MakeBodyPart(TEXT("ArmL"), CubeMesh, FVector(0.0f, 26.0f, 15.0f), FVector(0.11f, 0.11f, 0.32f)));
	PrimaryParts.Add(MakeBodyPart(TEXT("ArmR"), CubeMesh, FVector(0.0f, -26.0f, 15.0f), FVector(0.11f, 0.11f, 0.32f)));
	// Head (skin tone, not tinted) and head gear.
	MakeBodyPart(TEXT("Head"), SphereMesh, FVector(0.0f, 0.0f, 66.0f), FVector(0.20f));
	SecondaryParts.Add(MakeBodyPart(TEXT("HeadGear"), SphereMesh, FVector(-2.0f, 0.0f, 70.0f), FVector(0.24f)));
	// Backpack.
	SecondaryParts.Add(MakeBodyPart(TEXT("Backpack"), CubeMesh, FVector(-20.0f, 0.0f, 18.0f), FVector(0.18f, 0.24f, 0.34f)));

	// Role-specific extras (visibility toggled by operator in BeginPlay).
	Harness = MakeBodyPart(TEXT("Harness"), CubeMesh, FVector(0.0f, 0.0f, 12.0f), FVector(0.45f, 0.29f, 0.20f));
	Overcoat = MakeBodyPart(TEXT("Overcoat"), CubeMesh, FVector(0.0f, 0.0f, -18.0f), FVector(0.40f, 0.30f, 0.40f));
	Antenna = MakeBodyPart(TEXT("Antenna"), CubeMesh, FVector(-22.0f, 8.0f, 52.0f), FVector(0.015f, 0.015f, 0.35f));

	// --- First-person weapon viewmodel: five parts, reconfigured per weapon ---
	auto MakeGunPart = [this](const FName& Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Camera);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		return Part;
	};

	GunBody = MakeGunPart(TEXT("GunBody"));
	GunBarrel = MakeGunPart(TEXT("GunBarrel"));
	GunMag = MakeGunPart(TEXT("GunMag"));
	GunStock = MakeGunPart(TEXT("GunStock"));
	GunOptic = MakeGunPart(TEXT("GunOptic"));

	Weapon = CreateDefaultSubobject<UEchoWeaponComponent>(TEXT("Weapon"));
	Echo = CreateDefaultSubobject<UEchoLinkComponent>(TEXT("Echo"));

	// We bind all input ourselves (touch controls plus keyboard/mouse), so disable
	// ACharacter's default movement bindings to avoid double-bound axes.
	bAddDefaultMovementBindings = false;

	// Movement defaults.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
		Move->MaxWalkSpeedCrouched = CrouchSpeed;
		Move->JumpZVelocity = 450.0f;
		Move->AirControl = 0.25f;
		Move->BrakingDecelerationWalking = 2000.0f;
		Move->GravityScale = 1.6f;
		Move->NavAgentProps.bCanCrouch = true;
		Move->bOrientRotationToMovement = false;
	}

	bUseControllerRotationYaw = true;
}

void AEchoLineCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentFOV = BaseFieldOfView;
	HealthPoints = MaxHealthPoints;

	ApplyOperatorVisuals();
	if (Weapon)
	{
		ApplyWeaponVisuals(Weapon->GetWeaponId());
	}
}

void AEchoLineCharacter::ApplyOperatorVisuals()
{
	LastAppliedOperator = Operator;

	const FLinearColor Primary = EchoArt::OperatorPrimary(Operator);
	const FLinearColor Secondary = EchoArt::OperatorSecondary(Operator);

	for (const TObjectPtr<UStaticMeshComponent>& Part : PrimaryParts)
	{
		EchoArt::TintPart(Part, Primary);
	}
	for (const TObjectPtr<UStaticMeshComponent>& Part : SecondaryParts)
	{
		EchoArt::TintPart(Part, Secondary);
	}

	// Role-specific gear.
	if (Harness)
	{
		Harness->SetVisibility(Operator == EOperatorId::Patch);
		EchoArt::TintPart(Harness, EchoArt::OperatorSecondary(EOperatorId::Patch));
	}
	if (Overcoat)
	{
		Overcoat->SetVisibility(Operator == EOperatorId::Cairn);
		EchoArt::TintPart(Overcoat, EchoArt::OperatorSecondary(EOperatorId::Cairn));
	}
	if (Antenna)
	{
		Antenna->SetVisibility(Operator == EOperatorId::Wisp);
		EchoArt::TintPart(Antenna, FLinearColor(0.5f, 0.55f, 0.58f));
	}
}

void AEchoLineCharacter::ApplyWeaponVisuals(EWeaponId Id)
{
	LastAppliedWeapon = Id;

	const FLinearColor Metal = EchoArt::Gunmetal();
	UStaticMesh* CubeMesh = EchoArt::Cube();
	UStaticMesh* CylinderMesh = EchoArt::Cylinder();

	// Shared offset: weapon sits lower-right of the camera.
	const FVector Base(55.0f, -16.0f, -16.0f);

	switch (Id)
	{
	case EWeaponId::Kestrel:
		// Short SMG.
		EchoArt::ConfigurePart(GunBody, CubeMesh, Base, FVector(0.30f, 0.07f, 0.11f), Metal);
		EchoArt::ConfigurePart(GunBarrel, CylinderMesh, Base + FVector(24.0f, 0.0f, 2.0f), FVector(0.03f, 0.03f, 0.20f), Metal);
		EchoArt::ConfigurePart(GunMag, CubeMesh, Base + FVector(0.0f, 0.0f, -12.0f), FVector(0.05f, 0.05f, 0.14f), Metal);
		EchoArt::ConfigurePart(GunStock, CubeMesh, Base + FVector(-18.0f, 0.0f, -2.0f), FVector(0.06f, 0.05f, 0.06f), Metal);
		GunOptic->SetVisibility(false);
		break;

	case EWeaponId::Lancer:
		// Long marksman rifle with an optic.
		EchoArt::ConfigurePart(GunBody, CubeMesh, Base + FVector(4.0f, 0.0f, 0.0f), FVector(0.42f, 0.06f, 0.09f), Metal);
		EchoArt::ConfigurePart(GunBarrel, CylinderMesh, Base + FVector(42.0f, 0.0f, 3.0f), FVector(0.025f, 0.025f, 0.28f), Metal);
		EchoArt::ConfigurePart(GunMag, CubeMesh, Base + FVector(-2.0f, 0.0f, -11.0f), FVector(0.045f, 0.045f, 0.12f), Metal);
		EchoArt::ConfigurePart(GunStock, CubeMesh, Base + FVector(-26.0f, 0.0f, -2.0f), FVector(0.09f, 0.05f, 0.06f), Metal);
		EchoArt::ConfigurePart(GunOptic, CylinderMesh, Base + FVector(6.0f, 0.0f, 10.0f), FVector(0.03f, 0.03f, 0.10f), FLinearColor(0.05f, 0.06f, 0.07f));
		GunOptic->SetVisibility(true);
		break;

	case EWeaponId::Meridian:
	default:
		// Standard assault rifle.
		EchoArt::ConfigurePart(GunBody, CubeMesh, Base, FVector(0.50f, 0.07f, 0.11f), Metal);
		EchoArt::ConfigurePart(GunBarrel, CylinderMesh, Base + FVector(36.0f, 0.0f, 3.0f), FVector(0.03f, 0.03f, 0.24f), Metal);
		EchoArt::ConfigurePart(GunMag, CubeMesh, Base + FVector(-2.0f, 0.0f, -13.0f), FVector(0.05f, 0.05f, 0.16f), Metal);
		EchoArt::ConfigurePart(GunStock, CubeMesh, Base + FVector(-32.0f, 0.0f, -2.0f), FVector(0.12f, 0.05f, 0.06f), Metal);
		EchoArt::ConfigurePart(GunOptic, CubeMesh, Base + FVector(4.0f, 0.0f, 9.0f), FVector(0.05f, 0.03f, 0.03f), FLinearColor(0.05f, 0.06f, 0.07f));
		GunOptic->SetVisibility(true);
		break;
	}
}

void AEchoLineCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoLineCharacter, HealthPoints);
	DOREPLIFETIME(AEchoLineCharacter, bDead);
	DOREPLIFETIME(AEchoLineCharacter, Operator);
	DOREPLIFETIME(AEchoLineCharacter, AimPitch);
	DOREPLIFETIME(AEchoLineCharacter, bIsAiming);
	DOREPLIFETIME(AEchoLineCharacter, bBot);
	DOREPLIFETIME(AEchoLineCharacter, BotName);
}

FVector AEchoLineCharacter::GetEyeLocation() const
{
	return Camera ? Camera->GetComponentLocation() : GetActorLocation();
}

FRotator AEchoLineCharacter::GetAimRotation() const
{
	if (const AController* Ctrl = GetController())
	{
		return Ctrl->GetControlRotation();
	}
	return GetActorRotation();
}

void AEchoLineCharacter::SetAiming(bool bAiming)
{
	if (bDead)
	{
		return;
	}

	// The owning client applies ADS instantly; the server is notified so it can
	// use the aimed spread when resolving shots.
	if (IsLocallyControlled())
	{
		ApplyAiming(bAiming);
		if (!HasAuthority())
		{
			ServerSetAiming(bAiming);
		}
	}
}

void AEchoLineCharacter::ApplyAiming(bool bAiming)
{
	bIsAiming = bAiming;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		const float Scale = bAiming && Weapon ? Weapon->GetStats().AdsMoveSpeedScale : 1.0f;
		Move->MaxWalkSpeed = WalkSpeed * Scale;
	}
}

void AEchoLineCharacter::ServerSetAiming_Implementation(bool bAiming)
{
	ApplyAiming(bAiming);
}

void AEchoLineCharacter::AddRecoil(float PitchDeg, float YawDeg)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FRotator Rotation = PC->GetControlRotation();
		Rotation.Pitch += PitchDeg;
		Rotation.Yaw += YawDeg;
		PC->SetControlRotation(Rotation);
	}
}

void AEchoLineCharacter::UpdateFOV(float DeltaSeconds)
{
	const float Target = (bIsAiming && Weapon) ? Weapon->GetStats().AdsFieldOfView : BaseFieldOfView;
	CurrentFOV = FMath::FInterpTo(CurrentFOV, Target, DeltaSeconds, 12.0f);
	if (Camera)
	{
		Camera->SetFieldOfView(CurrentFOV);
	}
}

void AEchoLineCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateFOV(DeltaSeconds);

	// Rebuild visuals when the operator or weapon changes (including replication).
	if (Operator != LastAppliedOperator)
	{
		ApplyOperatorVisuals();
	}
	if (Weapon && Weapon->GetWeaponId() != LastAppliedWeapon)
	{
		ApplyWeaponVisuals(Weapon->GetWeaponId());
	}

	// Local footstep audio while moving on the ground.
	if (IsLocallyControlled() && !bDead && GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround())
	{
		if (GetVelocity().Size2D() > 150.0f)
		{
			FootstepTimer += DeltaSeconds;
			if (FootstepTimer >= FootstepInterval)
			{
				FootstepTimer = 0.0f;
				UEchoAudioSubsystem::Play(this, EEchoSfx::Footstep, FVector::ZeroVector, true, 0.15f);
			}
		}
		else
		{
			FootstepTimer = 0.0f;
		}
	}

	// The owning client periodically shares aim with the server for remote players.
	if (IsLocallyControlled())
	{
		AimUpdateAccumulator += DeltaSeconds;
		if (AimUpdateAccumulator >= 0.05f)
		{
			AimUpdateAccumulator = 0.0f;
			const FRotator Aim = GetAimRotation();
			AimPitch = Aim.Pitch;
			ServerUpdateAim(Aim);
		}
	}
}

void AEchoLineCharacter::ServerUpdateAim_Implementation(FRotator ControlRotation)
{
	AimPitch = ControlRotation.Pitch;
}

float AEchoLineCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead || !HasAuthority())
	{
		return 0.0f;
	}

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	HealthPoints = FMath::Max(0.0f, HealthPoints - DamageAmount);

	if (HealthPoints <= 0.0f)
	{
		bDead = true;

		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->DisableMovement();
		}
		SetActorEnableCollision(false);

		// Report the kill to the game mode, which handles scoring and respawns.
		if (AEchoGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AEchoGameMode>() : nullptr)
		{
			GM->NotifyKill(EventInstigator, this, Cast<AEchoLineCharacter>(DamageCauser));
		}
	}

	return Applied;
}

void AEchoLineCharacter::OnRep_Health()
{
	// Reserved for damage visuals.
}

void AEchoLineCharacter::RespawnAt(const FTransform& SpawnTransform)
{
	HealthPoints = MaxHealthPoints;
	bDead = false;
	SetActorEnableCollision(true);
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}

	SetAiming(false);
	ApplyAiming(false);
	if (Weapon)
	{
		Weapon->SwitchToWeapon(Weapon->GetWeaponId());
	}
}

void AEchoLineCharacter::Heal(float Amount)
{
	if (!HasAuthority() || bDead || Amount <= 0.0f)
	{
		return;
	}
	HealthPoints = FMath::Min(MaxHealthPoints, HealthPoints + Amount);
}

FString AEchoLineCharacter::GetDisplayName() const
{
	if (bBot)
	{
		return BotName.IsEmpty() ? TEXT("Bot") : BotName;
	}
	if (const APlayerState* PS = GetPlayerState())
	{
		return PS->GetPlayerName();
	}
	return TEXT("Operator");
}

void AEchoLineCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AEchoLineCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AEchoLineCharacter::MoveRight);

	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AEchoLineCharacter::InputFireStart);
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Released, this, &AEchoLineCharacter::InputFireStop);
	PlayerInputComponent->BindAction(TEXT("Reload"), IE_Pressed, this, &AEchoLineCharacter::InputReload);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &AEchoLineCharacter::InputJump);
	PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Pressed, this, &AEchoLineCharacter::InputToggleCrouch);
	PlayerInputComponent->BindAction(TEXT("Echo"), IE_Pressed, this, &AEchoLineCharacter::InputEcho);
	PlayerInputComponent->BindAction(TEXT("Aim"), IE_Pressed, this, &AEchoLineCharacter::InputAimStart);
	PlayerInputComponent->BindAction(TEXT("Aim"), IE_Released, this, &AEchoLineCharacter::InputAimStop);
	PlayerInputComponent->BindAction(TEXT("SwitchWeapon"), IE_Pressed, this, &AEchoLineCharacter::InputSwitchWeapon);
}

void AEchoLineCharacter::MoveForward(float Value)
{
	if (!IsInputAllowed() || FMath::IsNearlyZero(Value))
	{
		return;
	}
	AddMovementInput(FRotationMatrix(FRotator(0.0f, GetControlRotation().Yaw, 0.0f)).GetUnitAxis(EAxis::X), Value);
}

void AEchoLineCharacter::MoveRight(float Value)
{
	if (!IsInputAllowed() || FMath::IsNearlyZero(Value))
	{
		return;
	}
	AddMovementInput(FRotationMatrix(FRotator(0.0f, GetControlRotation().Yaw, 0.0f)).GetUnitAxis(EAxis::Y), Value);
}

bool AEchoLineCharacter::IsInputAllowed() const
{
	if (bDead)
	{
		return false;
	}
	if (const AEchoLinePlayerController* PC = Cast<AEchoLinePlayerController>(GetController()))
	{
		return PC->GetMenuState() == EMenuState::Playing;
	}
	return true;
}

void AEchoLineCharacter::InputFireStart()
{
	if (IsInputAllowed() && Weapon)
	{
		Weapon->StartFire();
	}
}

void AEchoLineCharacter::InputFireStop()
{
	if (Weapon)
	{
		Weapon->StopFire();
	}
}

void AEchoLineCharacter::InputReload()
{
	if (IsInputAllowed() && Weapon)
	{
		Weapon->StartReload();
	}
}

void AEchoLineCharacter::InputEcho()
{
	if (IsInputAllowed() && Echo)
	{
		Echo->ToggleEcho();
	}
}

void AEchoLineCharacter::InputAimStart()
{
	if (IsInputAllowed())
	{
		SetAiming(true);
	}
}

void AEchoLineCharacter::InputAimStop()
{
	SetAiming(false);
}

void AEchoLineCharacter::InputJump()
{
	if (IsInputAllowed())
	{
		Jump();
	}
}

void AEchoLineCharacter::InputToggleCrouch()
{
	if (!IsInputAllowed())
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AEchoLineCharacter::InputSwitchWeapon()
{
	if (IsInputAllowed() && Weapon)
	{
		Weapon->CycleWeapon();
	}
}
