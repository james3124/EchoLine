// EchoLine - runtime audio.
// Loads the generated WAV files from ArtSource/Audio at runtime and plays them through
// procedural sound waves, so the game has audio without importing anything into the editor.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EchoAudio.generated.h"

class USoundAttenuation;
class USoundWaveProcedural;

UENUM(BlueprintType)
enum class EEchoSfx : uint8
{
	MeridianFire,
	KestrelFire,
	LancerFire,
	Reload,
	Hitmarker,
	Kill,
	EchoPlace,
	EchoDestroy,
	Pickup,
	UIClick,
	Footstep,
	MAX			UMETA(Hidden)
};

/** Decoded 16-bit PCM mono/stereo audio held in memory. */
struct FEchoWavData
{
	TArray<uint8> PCM;
	int32 SampleRate = 44100;
	int32 NumChannels = 1;
	float Duration = 0.0f;

	bool IsValid() const { return PCM.Num() > 0 && SampleRate > 0 && NumChannels > 0; }
};

UCLASS()
class ECHOLINE_API UEchoAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Plays one of the generated sound effects. */
	void PlaySfx(EEchoSfx Id, const FVector& Location = FVector::ZeroVector, bool b2D = true, float PitchVariation = 0.0f);

	/** Convenience accessor from any world-context object. */
	static UEchoAudioSubsystem* Get(const UObject* WorldContextObject);

	/** Convenience one-liner used across gameplay code. */
	static void Play(const UObject* WorldContextObject, EEchoSfx Id,
		const FVector& Location = FVector::ZeroVector, bool b2D = true, float PitchVariation = 0.0f);

	/** The sound effect that belongs to a weapon. */
	static EEchoSfx FireSfxForWeapon(uint8 WeaponId);

protected:
	const FEchoWavData* GetSound(EEchoSfx Id);
	bool LoadWav(const FString& FilePath, FEchoWavData& OutData) const;
	FString GetSoundDirectory() const;

	UPROPERTY()
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Procedural waves kept alive while their audio component plays. */
	UPROPERTY()
	TArray<TObjectPtr<USoundWaveProcedural>> ActiveWaves;

	/** Decoded clips, loaded on demand and cached. */
	TMap<EEchoSfx, FEchoWavData> Cache;
};
