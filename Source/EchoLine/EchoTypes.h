// EchoLine - shared gameplay types. All C++, no editor assets required.

#pragma once

#include "CoreMinimal.h"
#include "EchoTypes.generated.h"

/** The firearms available in the prototype. */
UENUM(BlueprintType)
enum class EWeaponId : uint8
{
	Meridian	UMETA(DisplayName = "AR-7 Meridian"),
	Kestrel		UMETA(DisplayName = "Kestrel-9"),
	Lancer		UMETA(DisplayName = "MR-12 Lancer"),
	MAX			UMETA(Hidden)
};

/** How the trigger behaves. */
UENUM(BlueprintType)
enum class EFireMode : uint8
{
	Auto		UMETA(DisplayName = "Automatic"),
	Semi		UMETA(DisplayName = "Semi-Automatic")
};

/** Compact operator identities. Appearance only; all share identical stats. */
UENUM(BlueprintType)
enum class EOperatorId : uint8
{
	Rook		UMETA(DisplayName = "Mara \"Rook\" Venn"),
	Wisp		UMETA(DisplayName = "Ilan \"Wisp\" Sato"),
	Patch		UMETA(DisplayName = "Dara \"Patch\" Okafor"),
	Cairn		UMETA(DisplayName = "Soren \"Cairn\" Vale"),
	MAX			UMETA(Hidden)
};

/**
 * Full stat block for a fictional firearm. Ranges are in Unreal units (cm).
 * Values here are initial tuning targets and are expected to change after playtests.
 */
USTRUCT(BlueprintType)
struct FWeaponStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	EWeaponId Id = EWeaponId::Meridian;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FString DisplayName = TEXT("Unnamed");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	EFireMode FireMode = EFireMode::Auto;

	/** Damage applied at or inside EffectiveRange. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float Damage = 30.0f;

	/** Damage multiplier applied at or beyond MaxRange. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float MinDamageFactor = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float EffectiveRange = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float MaxRange = 7000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire")
	float RoundsPerMinute = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire")
	int32 MagazineSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire")
	float ReloadSeconds = 2.2f;

	/** Degrees of upward camera kick per shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilPitch = 1.1f;

	/** Degrees of random horizontal camera kick per shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilYaw = 0.35f;

	/** How quickly the camera returns to centre (higher is steadier). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float Stability = 0.75f;

	/** Hip-fire cone half-angle in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy")
	float HipSpreadDegrees = 2.2f;

	/** Aimed cone half-angle in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy")
	float AdsSpreadDegrees = 0.15f;

	/** Extra spread multiplier while moving. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy")
	float MoveSpreadMultiplier = 2.0f;

	/** Field of view while aiming down sights. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	float AdsFieldOfView = 70.0f;

	/** Movement speed scale while aiming down sights. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	float AdsMoveSpeedScale = 0.55f;

	/** Seconds between shots, derived from RPM. */
	float GetShotInterval() const
	{
		return RoundsPerMinute > 0.0f ? 60.0f / RoundsPerMinute : 1.0f;
	}

	/** Damage after distance falloff. */
	float GetDamageAtDistance(float Distance) const
	{
		if (Distance <= EffectiveRange)
		{
			return Damage;
		}
		if (Distance >= MaxRange || MaxRange <= EffectiveRange)
		{
			return Damage * MinDamageFactor;
		}
		const float Alpha = (Distance - EffectiveRange) / (MaxRange - EffectiveRange);
		return Damage * FMath::Lerp(1.0f, MinDamageFactor, Alpha);
	}
};
