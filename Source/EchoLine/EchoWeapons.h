// EchoLine - hard-coded weapon table. Keeps the prototype asset-free.
// Later this can be moved to a DataTable or UDataAsset without changing call sites.

#pragma once

#include "CoreMinimal.h"
#include "EchoTypes.h"

namespace EchoWeapons
{
	/** Returns the stat block for a weapon. */
	inline FWeaponStats GetStats(EWeaponId Id)
	{
		FWeaponStats S;
		S.Id = Id;

		switch (Id)
		{
		case EWeaponId::Kestrel:
			S.DisplayName = TEXT("Kestrel-9");
			S.FireMode = EFireMode::Auto;
			S.Damage = 23.0f;
			S.MinDamageFactor = 0.45f;
			S.EffectiveRange = 1800.0f;
			S.MaxRange = 4000.0f;
			S.RoundsPerMinute = 800.0f;
			S.MagazineSize = 32;
			S.ReloadSeconds = 1.9f;
			S.RecoilPitch = 0.8f;
			S.RecoilYaw = 0.30f;
			S.Stability = 0.60f;
			S.HipSpreadDegrees = 3.0f;
			S.AdsSpreadDegrees = 0.40f;
			S.MoveSpreadMultiplier = 2.6f;
			S.AdsFieldOfView = 75.0f;
			S.AdsMoveSpeedScale = 0.70f;
			break;

		case EWeaponId::Lancer:
			S.DisplayName = TEXT("MR-12 Lancer");
			S.FireMode = EFireMode::Semi;
			S.Damage = 55.0f;
			S.MinDamageFactor = 0.60f;
			S.EffectiveRange = 6000.0f;
			S.MaxRange = 12000.0f;
			S.RoundsPerMinute = 180.0f;
			S.MagazineSize = 12;
			S.ReloadSeconds = 2.6f;
			S.RecoilPitch = 2.6f;
			S.RecoilYaw = 0.40f;
			S.Stability = 0.90f;
			S.HipSpreadDegrees = 4.0f;
			S.AdsSpreadDegrees = 0.05f;
			S.MoveSpreadMultiplier = 3.0f;
			S.AdsFieldOfView = 55.0f;
			S.AdsMoveSpeedScale = 0.40f;
			break;

		case EWeaponId::Meridian:
		default:
			S.DisplayName = TEXT("AR-7 Meridian");
			S.FireMode = EFireMode::Auto;
			S.Damage = 30.0f;
			S.MinDamageFactor = 0.50f;
			S.EffectiveRange = 3500.0f;
			S.MaxRange = 7000.0f;
			S.RoundsPerMinute = 600.0f;
			S.MagazineSize = 30;
			S.ReloadSeconds = 2.2f;
			S.RecoilPitch = 1.1f;
			S.RecoilYaw = 0.35f;
			S.Stability = 0.75f;
			S.HipSpreadDegrees = 2.2f;
			S.AdsSpreadDegrees = 0.15f;
			S.MoveSpreadMultiplier = 2.0f;
			S.AdsFieldOfView = 70.0f;
			S.AdsMoveSpeedScale = 0.55f;
			break;
		}

		return S;
	}

	/** Cycles to the next weapon id. */
	inline EWeaponId Next(EWeaponId Id)
	{
		const int32 Count = static_cast<int32>(EWeaponId::MAX);
		const int32 NextIndex = (static_cast<int32>(Id) + 1) % Count;
		return static_cast<EWeaponId>(NextIndex);
	}
}
