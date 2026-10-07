#include "EchoAudio.h"

#include "Components/AudioComponent.h"
#include "EchoTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
	uint32 ReadU32(const uint8* P) { return P[0] | (P[1] << 8) | (P[2] << 16) | (P[3] << 24); }
	uint16 ReadU16(const uint8* P) { return static_cast<uint16>(P[0] | (P[1] << 8)); }

	const TCHAR* FileNameFor(EEchoSfx Id)
	{
		switch (Id)
		{
		case EEchoSfx::MeridianFire:  return TEXT("SFX_Meridian_Fire.wav");
		case EEchoSfx::KestrelFire:   return TEXT("SFX_Kestrel_Fire.wav");
		case EEchoSfx::LancerFire:    return TEXT("SFX_Lancer_Fire.wav");
		case EEchoSfx::Reload:        return TEXT("SFX_Reload.wav");
		case EEchoSfx::Hitmarker:     return TEXT("SFX_Hitmarker.wav");
		case EEchoSfx::Kill:          return TEXT("SFX_Kill.wav");
		case EEchoSfx::EchoPlace:     return TEXT("SFX_EchoPlace.wav");
		case EEchoSfx::EchoDestroy:   return TEXT("SFX_EchoDestroy.wav");
		case EEchoSfx::Pickup:        return TEXT("SFX_Pickup.wav");
		case EEchoSfx::UIClick:       return TEXT("SFX_UIClick.wav");
		case EEchoSfx::Footstep:      return TEXT("SFX_Footstep.wav");
		default:                      return TEXT("");
		}
	}
}

void UEchoAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// A shared 3D attenuation for world-space effects.
	Attenuation = NewObject<USoundAttenuation>(this);
	if (Attenuation)
	{
		Attenuation->Attenuation.bAttenuate = true;
		Attenuation->Attenuation.AttenuationShapeExtents = FVector(600.0f, 0.0f, 0.0f);
		Attenuation->Attenuation.FalloffDistance = 6000.0f;
	}

	// Warn once if the generated sources are missing, so silent games are easy to diagnose.
	const FString Dir = GetSoundDirectory();
	if (!FPaths::DirectoryExists(Dir))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("EchoLine audio: '%s' not found. Run 'python3 Tooling/generate_assets.py' to create the WAVs."),
			*Dir);
	}
}

UEchoAudioSubsystem* UEchoAudioSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	if (const UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject))
	{
		return GI->GetSubsystem<UEchoAudioSubsystem>();
	}
	return nullptr;
}

void UEchoAudioSubsystem::Play(const UObject* WorldContextObject, EEchoSfx Id,
	const FVector& Location, bool b2D, float PitchVariation)
{
	if (UEchoAudioSubsystem* Audio = Get(WorldContextObject))
	{
		Audio->PlaySfx(Id, Location, b2D, PitchVariation);
	}
}

EEchoSfx UEchoAudioSubsystem::FireSfxForWeapon(uint8 WeaponId)
{
	switch (static_cast<EWeaponId>(WeaponId))
	{
	case EWeaponId::Kestrel: return EEchoSfx::KestrelFire;
	case EWeaponId::Lancer:  return EEchoSfx::LancerFire;
	case EWeaponId::Meridian:
	default:                 return EEchoSfx::MeridianFire;
	}
}

FString UEchoAudioSubsystem::GetSoundDirectory() const
{
	return FPaths::ProjectDir() / TEXT("ArtSource/Audio");
}

bool UEchoAudioSubsystem::LoadWav(const FString& FilePath, FEchoWavData& OutData) const
{
	TArray<uint8> Raw;
	if (!FFileHelper::LoadFileToArray(Raw, *FilePath))
	{
		return false;
	}

	if (Raw.Num() < 44 || FMemory::Memcmp(Raw.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(Raw.GetData() + 8, "WAVE", 4) != 0)
	{
		return false;
	}

	int32 Offset = 12;
	int32 BitsPerSample = 16;
	bool bFoundFmt = false;

	while (Offset + 8 <= Raw.Num())
	{
		const uint8* Chunk = Raw.GetData() + Offset;
		const uint32 ChunkSize = ReadU32(Chunk + 4);
		const uint8* ChunkData = Chunk + 8;

		if (FMemory::Memcmp(Chunk, "fmt ", 4) == 0 && ChunkSize >= 16)
		{
			OutData.NumChannels = ReadU16(ChunkData + 2);
			OutData.SampleRate = ReadU32(ChunkData + 4);
			BitsPerSample = ReadU16(ChunkData + 14);
			bFoundFmt = true;
		}
		else if (FMemory::Memcmp(Chunk, "data", 4) == 0)
		{
			const int32 Available = FMath::Min(static_cast<int32>(ChunkSize), Raw.Num() - (Offset + 8));
			OutData.PCM.Append(ChunkData, FMath::Max(0, Available));
		}

		// Chunks are word-aligned.
		Offset += 8 + ChunkSize + (ChunkSize & 1);
	}

	if (!bFoundFmt || OutData.PCM.Num() == 0 || OutData.NumChannels <= 0 || OutData.SampleRate <= 0)
	{
		return false;
	}

	// The procedural wave expects 16-bit little-endian PCM.
	if (BitsPerSample == 8)
	{
		// Promote 8-bit unsigned to 16-bit signed.
		const int32 Count = OutData.PCM.Num();
		TArray<uint8> Promoted;
		Promoted.SetNumUninitialized(Count * 2);
		for (int32 i = 0; i < Count; ++i)
		{
			const int16 Sample = (static_cast<int16>(OutData.PCM[i]) - 128) * 256;
			Promoted[i * 2] = Sample & 0xFF;
			Promoted[i * 2 + 1] = (Sample >> 8) & 0xFF;
		}
		OutData.PCM = MoveTemp(Promoted);
	}

	const int32 BytesPerFrame = OutData.NumChannels * 2;
	OutData.Duration = static_cast<float>(OutData.PCM.Num()) / static_cast<float>(OutData.SampleRate * BytesPerFrame);
	return true;
}

const FEchoWavData* UEchoAudioSubsystem::GetSound(EEchoSfx Id)
{
	if (FEchoWavData* Cached = Cache.Find(Id))
	{
		return Cached;
	}

	FEchoWavData Loaded;
	const FString Path = GetSoundDirectory() / FileNameFor(Id);
	if (!LoadWav(Path, Loaded))
	{
		// Cache an empty entry so we do not retry every shot.
		Cache.Add(Id, FEchoWavData());
		return nullptr;
	}

	FEchoWavData& Stored = Cache.Add(Id, MoveTemp(Loaded));
	return &Stored;
}

void UEchoAudioSubsystem::PlaySfx(EEchoSfx Id, const FVector& Location, bool b2D, float PitchVariation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FEchoWavData* Data = GetSound(Id);
	if (!Data || !Data->IsValid())
	{
		return;
	}

	// A fresh procedural wave is needed per play because the queue drains as it plays.
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	if (!Wave)
	{
		return;
	}

	Wave->SetSampleRate(Data->SampleRate);
	Wave->NumChannels = Data->NumChannels;
	Wave->Duration = Data->Duration;
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->QueueAudio(Data->PCM.GetData(), Data->PCM.Num());

	const float Pitch = (PitchVariation > 0.0f)
		? FMath::FRandRange(1.0f - PitchVariation, 1.0f + PitchVariation)
		: 1.0f;

	UAudioComponent* Component = b2D
		? UGameplayStatics::SpawnSound2D(this, Wave, 0.6f, Pitch, 0.0f, nullptr, false, false)
		: UGameplayStatics::SpawnSoundAtLocation(this, Wave, Location, FRotator::ZeroRotator, 0.7f, Pitch, 0.0f, Attenuation);

	if (Component)
	{
		// Keep the wave referenced while it plays, then let it go.
		ActiveWaves.Add(Wave);
		TWeakObjectPtr<USoundWaveProcedural> WeakWave(Wave);
		Component->OnAudioFinishedNative.AddLambda([this, WeakWave]()
		{
			ActiveWaves.Remove(WeakWave.Get());
		});
	}
}
