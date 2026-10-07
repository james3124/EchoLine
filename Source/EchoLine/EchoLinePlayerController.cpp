#include "EchoLinePlayerController.h"

#include "Components/InputComponent.h"
#include "CoreGlobals.h"
#include "EchoAudio.h"
#include "EchoGameMode.h"
#include "EchoGameState.h"
#include "EchoLineCharacter.h"
#include "EchoLinkComponent.h"
#include "EchoWeaponComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Char.h"
#include "Misc/ConfigCacheIni.h"
#include "Scalability.h"

namespace
{
	const FName BtnFire(TEXT("Fire"));
	const FName BtnAim(TEXT("Aim"));
	const FName BtnReload(TEXT("Reload"));
	const FName BtnEcho(TEXT("Echo"));
	const FName BtnCrouch(TEXT("Crouch"));
	const FName BtnJump(TEXT("Jump"));
	const FName BtnSwitch(TEXT("Switch"));
	const FName BtnMenu(TEXT("Menu"));

	const FName BtnDeploy(TEXT("Deploy"));
	const FName BtnHost(TEXT("Host"));
	const FName BtnJoin(TEXT("Join"));
	const FName BtnSettings(TEXT("Settings"));
	const FName BtnBack(TEXT("Back"));
	const FName BtnSensDown(TEXT("SensDown"));
	const FName BtnSensUp(TEXT("SensUp"));
	const FName BtnFovDown(TEXT("FovDown"));
	const FName BtnFovUp(TEXT("FovUp"));
	const FName BtnInvert(TEXT("Invert"));
	const FName BtnQuality(TEXT("Quality"));
	const FName BtnConnect(TEXT("Connect"));
	const FName BtnBackspace(TEXT("Backspace"));
	const FName BtnReturnMenu(TEXT("ReturnMenu"));
	const FName BtnAddBot(TEXT("AddBot"));

	FEchoButton MakeButton(const FName& Id, const FString& Label, float X, float Y, float W, float H, const FLinearColor& Tint)
	{
		FEchoButton B;
		B.Id = Id;
		B.Label = Label;
		B.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
		B.Tint = Tint;
		return B;
	}
}

AEchoLinePlayerController::AEchoLinePlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
}

void AEchoLinePlayerController::BeginPlay()
{
	Super::BeginPlay();
	LoadSettings();
	ApplyQuality();

	// Apply the saved field of view to the pawn's camera.
	if (AEchoLineCharacter* Character = GetEchoCharacter())
	{
		Character->SetBaseFieldOfView(FieldOfView);
	}

	MenuState = EMenuState::MainMenu;
}

void AEchoLinePlayerController::LoadSettings()
{
	GConfig->GetFloat(TEXT("EchoLine"), TEXT("Sensitivity"), Sensitivity, GGameUserSettingsIni);
	GConfig->GetFloat(TEXT("EchoLine"), TEXT("FieldOfView"), FieldOfView, GGameUserSettingsIni);
	GConfig->GetBool(TEXT("EchoLine"), TEXT("InvertY"), bInvertY, GGameUserSettingsIni);
	GConfig->GetInt(TEXT("EchoLine"), TEXT("QualityLevel"), QualityLevel, GGameUserSettingsIni);
	GConfig->GetString(TEXT("EchoLine"), TEXT("JoinIP"), JoinIP, GGameUserSettingsIni);
}

void AEchoLinePlayerController::SaveSettings()
{
	GConfig->SetFloat(TEXT("EchoLine"), TEXT("Sensitivity"), Sensitivity, GGameUserSettingsIni);
	GConfig->SetFloat(TEXT("EchoLine"), TEXT("FieldOfView"), FieldOfView, GGameUserSettingsIni);
	GConfig->SetBool(TEXT("EchoLine"), TEXT("InvertY"), bInvertY, GGameUserSettingsIni);
	GConfig->SetInt(TEXT("EchoLine"), TEXT("QualityLevel"), QualityLevel, GGameUserSettingsIni);
	GConfig->SetString(TEXT("EchoLine"), TEXT("JoinIP"), *JoinIP, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void AEchoLinePlayerController::ApplyQuality()
{
	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();

	// Apply one scalar level across every group (supported in all UE5 versions).
	Levels.ViewDistanceQuality = QualityLevel;
	Levels.AntiAliasingQuality = QualityLevel;
	Levels.ShadowQuality = QualityLevel;
	Levels.GlobalIlluminationQuality = QualityLevel;
	Levels.ReflectionQuality = QualityLevel;
	Levels.PostProcessQuality = QualityLevel;
	Levels.TextureQuality = QualityLevel;
	Levels.EffectsQuality = QualityLevel;
	Levels.FoliageQuality = QualityLevel;

	switch (QualityLevel)
	{
	case 0:
		Levels.ResolutionQuality = 70.0f;
		break;
	case 2:
		Levels.ResolutionQuality = 100.0f;
		break;
	default:
		Levels.ResolutionQuality = 85.0f;
		break;
	}

	Scalability::SetQualityLevels(Levels);
}

AEchoLineCharacter* AEchoLinePlayerController::GetEchoCharacter() const
{
	return Cast<AEchoLineCharacter>(GetPawn());
}

void AEchoLinePlayerController::Deploy()
{
	MenuState = EMenuState::Playing;
	bEnteringIP = false;
}

void AEchoLinePlayerController::HostMatch()
{
	const FString MapName = UGameplayStatics::GetCurrentLevelName(this, true);
	UGameplayStatics::OpenLevel(this, FName(*MapName), true, TEXT("listen"));
}

void AEchoLinePlayerController::JoinMatch(const FString& Address)
{
	if (Address.IsEmpty())
	{
		return;
	}
	SaveSettings();
	ClientTravel(Address, ETravelType::TRAVEL_Absolute);
}

void AEchoLinePlayerController::OpenSettings()
{
	PreviousMenuState = MenuState;
	MenuState = EMenuState::Settings;
	bEnteringIP = false;
}

void AEchoLinePlayerController::ReturnToMainMenu()
{
	MenuState = EMenuState::MainMenu;
	bEnteringIP = false;
}

void AEchoLinePlayerController::CloseSettingsToMenu()
{
	SaveSettings();
	MenuState = (PreviousMenuState == EMenuState::Settings) ? EMenuState::MainMenu : PreviousMenuState;
}

void AEchoLinePlayerController::EchoHost()
{
	HostMatch();
}

void AEchoLinePlayerController::EchoJoin(const FString& Address)
{
	JoinMatch(Address);
}

void AEchoLinePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Show the results screen when the round ends.
	if (const AEchoGameState* GS = GetWorld() ? GetWorld()->GetGameState<AEchoGameState>() : nullptr)
	{
		if (GS->bMatchOver && MenuState == EMenuState::Playing)
		{
			MenuState = EMenuState::Results;
		}
		else if (!GS->bMatchOver && MenuState == EMenuState::Results)
		{
			MenuState = EMenuState::Playing;
		}
	}

	// Apply the saved field of view once the pawn is available.
	if (!bAppliedInitialSettings)
	{
		if (AEchoLineCharacter* Character = GetEchoCharacter())
		{
			Character->SetBaseFieldOfView(FieldOfView);
			bAppliedInitialSettings = true;
		}
	}

	// Show the system cursor in menus (handy for desktop testing) and hide it in play.
	bShowMouseCursor = (MenuState != EMenuState::Playing);

	// Feed the movement stick into the pawn.
	if (MenuState == EMenuState::Playing && bMoveStickActive && !MoveInput.IsNearlyZero())
	{
		if (AEchoLineCharacter* Character = GetEchoCharacter())
		{
			Character->MoveForward(MoveInput.Y);
			Character->MoveRight(MoveInput.X);
		}
	}
}

void AEchoLinePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Desktop / editor testing.
	InputComponent->BindAxis(TEXT("Turn"), this, &AEchoLinePlayerController::InputTurn);
	InputComponent->BindAxis(TEXT("LookUp"), this, &AEchoLinePlayerController::InputLookUp);
	InputComponent->BindAction(TEXT("MenuClick"), IE_Pressed, this, &AEchoLinePlayerController::InputMenuClick);
}

void AEchoLinePlayerController::InputTurn(float Value)
{
	if (MenuState != EMenuState::Playing || FMath::IsNearlyZero(Value))
	{
		return;
	}
	AddYawInput(Value * Sensitivity);
}

void AEchoLinePlayerController::InputLookUp(float Value)
{
	if (MenuState != EMenuState::Playing || FMath::IsNearlyZero(Value))
	{
		return;
	}
	AddPitchInput(Value * Sensitivity * (bInvertY ? -1.0f : 1.0f));
}

void AEchoLinePlayerController::InputMenuClick()
{
	if (MenuState == EMenuState::Playing)
	{
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	TArray<FEchoButton> Buttons;
	GetActiveButtons(Buttons);

	const FVector2D Point(MouseX, MouseY);
	for (const FEchoButton& Button : Buttons)
	{
		if (Button.Rect.IsInside(Point))
		{
			ActivateMenuButton(Button.Id, 1.0f);
			return;
		}
	}
}
bool AEchoLinePlayerController::InputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, float Force, FDateTime DeviceTimestamp)
{
	// Consume all touches; EchoLine draws its own controls.
	Super::InputTouch(Handle, Type, TouchLocation, Force, DeviceTimestamp);

	if (MenuState == EMenuState::Playing)
	{
		ProcessPlayingTouch(Handle, Type, TouchLocation);
	}
	else
	{
		ProcessMenuTouch(Handle, Type, TouchLocation);
	}
	return true;
}

void AEchoLinePlayerController::ProcessPlayingTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& Pos)
{
	int32 SizeX = 0;
	int32 SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	const FVector2D Size(SizeX, SizeY);

	TArray<FEchoButton> Buttons;
	BuildPlayingButtons(Buttons, Size);

	if (Type == ETouchType::Began)
	{
		// Buttons take priority.
		for (const FEchoButton& Button : Buttons)
		{
			if (Button.Rect.IsInside(Pos))
			{
				ActiveButtonTouches.Add(Handle, Button.Id);

				if (AEchoLineCharacter* Character = GetEchoCharacter())
				{
					if (Button.Id == BtnFire) { Character->InputFireStart(); }
					else if (Button.Id == BtnAim) { Character->InputAimStart(); }
					else if (Button.Id == BtnReload) { Character->InputReload(); }
					else if (Button.Id == BtnEcho) { Character->InputEcho(); }
					else if (Button.Id == BtnCrouch) { Character->InputToggleCrouch(); }
					else if (Button.Id == BtnJump) { Character->InputJump(); }
					else if (Button.Id == BtnSwitch) { Character->InputSwitchWeapon(); }
				}

				if (Button.Id == BtnMenu)
				{
					PreviousMenuState = EMenuState::Playing;
					MenuState = EMenuState::Settings;
				}
				return;
			}
		}

		// Left half is the movement stick; the rest is look.
		if (Pos.X < Size.X * 0.5f)
		{
			MoveTouchHandle = Handle;
			MoveStickBase = Pos;
			MoveStickKnob = Pos;
			MoveInput = FVector2D::ZeroVector;
			bMoveStickActive = true;
		}
		else
		{
			LookTouchHandle = Handle;
			LastLookPos = Pos;
		}
	}
	else if (Type == ETouchType::Moved)
	{
		if (MoveTouchHandle.IsSet() && MoveTouchHandle.GetValue() == Handle)
		{
			FVector2D Delta = Pos - MoveStickBase;
			const float Length = Delta.Size();
			if (Length > MoveStickRadius)
			{
				Delta = Delta.GetSafeNormal() * MoveStickRadius;
			}
			MoveStickKnob = MoveStickBase + Delta;
			MoveInput = Delta / MoveStickRadius;

			// Invert Y so pushing up moves forward.
			MoveInput.Y = -MoveInput.Y;
		}
		else if (LookTouchHandle.IsSet() && LookTouchHandle.GetValue() == Handle)
		{
			const FVector2D Delta = Pos - LastLookPos;
			LastLookPos = Pos;

			FRotator Rotation = GetControlRotation();
			Rotation.Yaw += Delta.X * Sensitivity * 0.22f;
			Rotation.Pitch = FMath::Clamp(Rotation.Pitch - Delta.Y * Sensitivity * 0.22f * (bInvertY ? -1.0f : 1.0f), -89.0f, 89.0f);
			SetControlRotation(Rotation);
		}
	}
	else if (Type == ETouchType::Ended)
	{
		if (MoveTouchHandle.IsSet() && MoveTouchHandle.GetValue() == Handle)
		{
			MoveTouchHandle.Reset();
			bMoveStickActive = false;
			MoveInput = FVector2D::ZeroVector;
		}
		else if (LookTouchHandle.IsSet() && LookTouchHandle.GetValue() == Handle)
		{
			LookTouchHandle.Reset();
		}
		else if (FName* Released = ActiveButtonTouches.Find(Handle))
		{
			if (AEchoLineCharacter* Character = GetEchoCharacter())
			{
				if (*Released == BtnFire) { Character->InputFireStop(); }
				else if (*Released == BtnAim) { Character->InputAimStop(); }
			}
			ActiveButtonTouches.Remove(Handle);
		}
	}
}

void AEchoLinePlayerController::ProcessMenuTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& Pos)
{
	if (Type != ETouchType::Began)
	{
		return;
	}

	TArray<FEchoButton> Buttons;
	GetActiveButtons(Buttons);

	for (const FEchoButton& Button : Buttons)
	{
		if (Button.Rect.IsInside(Pos))
		{
			ActivateMenuButton(Button.Id, 1.0f);
			return;
		}
	}
}

void AEchoLinePlayerController::ActivateMenuButton(const FName& Id, float Direction)
{
	UEchoAudioSubsystem::Play(this, EEchoSfx::UIClick, FVector::ZeroVector, true);

	if (Id == BtnDeploy)
	{
		Deploy();
	}
	else if (Id == BtnHost)
	{
		HostMatch();
	}
	else if (Id == BtnJoin)
	{
		bEnteringIP = true;
	}
	else if (Id == BtnSettings)
	{
		OpenSettings();
	}
	else if (Id == BtnBack)
	{
		if (bEnteringIP)
		{
			bEnteringIP = false;
		}
		else
		{
			CloseSettingsToMenu();
		}
	}
	else if (Id == BtnReturnMenu)
	{
		ReturnToMainMenu();
	}
	else if (Id == BtnAddBot)
	{
		if (AEchoGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AEchoGameMode>() : nullptr)
		{
			GM->AddBot();
		}
	}
	else if (Id == BtnSensDown)
	{
		Sensitivity = FMath::Clamp(Sensitivity - 0.1f, 0.2f, 3.0f);
		SaveSettings();
	}
	else if (Id == BtnSensUp)
	{
		Sensitivity = FMath::Clamp(Sensitivity + 0.1f, 0.2f, 3.0f);
		SaveSettings();
	}
	else if (Id == BtnFovDown)
	{
		FieldOfView = FMath::Clamp(FieldOfView - 5.0f, 60.0f, 110.0f);
		if (AEchoLineCharacter* Character = GetEchoCharacter())
		{
			Character->SetBaseFieldOfView(FieldOfView);
		}
		SaveSettings();
	}
	else if (Id == BtnFovUp)
	{
		FieldOfView = FMath::Clamp(FieldOfView + 5.0f, 60.0f, 110.0f);
		if (AEchoLineCharacter* Character = GetEchoCharacter())
		{
			Character->SetBaseFieldOfView(FieldOfView);
		}
		SaveSettings();
	}
	else if (Id == BtnInvert)
	{
		bInvertY = !bInvertY;
		SaveSettings();
	}
	else if (Id == BtnQuality)
	{
		QualityLevel = (QualityLevel + 1) % 3;
		ApplyQuality();
		SaveSettings();
	}
	else if (Id == BtnBackspace)
	{
		if (JoinIP.Len() > 0)
		{
			JoinIP.LeftChopInline(1);
		}
	}
	else if (Id == BtnConnect)
	{
		JoinMatch(JoinIP);
	}
	else
	{
		const FString IdString = Id.ToString();
		if (IdString == TEXT("Dot"))
		{
			JoinIP.AppendChar(TEXT('.'));
		}
		else if (IdString == TEXT("Colon"))
		{
			JoinIP.AppendChar(TEXT(':'));
		}
		else if (IdString.Len() == 1 && FChar::IsDigit(IdString[0]))
		{
			JoinIP += IdString;
		}
	}
}

void AEchoLinePlayerController::GetActiveButtons(TArray<FEchoButton>& Out) const
{
	Out.Reset();

	int32 SizeX = 0;
	int32 SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	const FVector2D Size(SizeX, SizeY);

	switch (MenuState)
	{
	case EMenuState::Playing:
		BuildPlayingButtons(Out, Size);
		break;
	case EMenuState::Settings:
		BuildSettingsButtons(Out, Size);
		break;
	case EMenuState::Results:
		BuildResultsButtons(Out, Size);
		break;
	case EMenuState::MainMenu:
	default:
		if (bEnteringIP)
		{
			BuildKeypadButtons(Out, Size);
		}
		else
		{
			BuildMenuButtons(Out, Size);
		}
		break;
	}
}

void AEchoLinePlayerController::BuildPlayingButtons(TArray<FEchoButton>& Out, const FVector2D& Size) const
{
	const float W = Size.X;
	const float H = Size.Y;

	Out.Add(MakeButton(BtnFire, TEXT("FIRE"), W - 190.0f, H - 190.0f, 150.0f, 150.0f, FLinearColor(0.65f, 0.12f, 0.10f, 0.55f)));
	Out.Add(MakeButton(BtnAim, TEXT("ADS"), W - 370.0f, H - 175.0f, 120.0f, 120.0f, FLinearColor(0.10f, 0.30f, 0.55f, 0.50f)));
	Out.Add(MakeButton(BtnReload, TEXT("RELOAD"), W - 360.0f, H - 320.0f, 110.0f, 100.0f, FLinearColor(0.10f, 0.35f, 0.20f, 0.50f)));
	Out.Add(MakeButton(BtnEcho, TEXT("ECHO"), W - 360.0f, H - 445.0f, 110.0f, 100.0f, FLinearColor(0.10f, 0.45f, 0.90f, 0.55f)));
	Out.Add(MakeButton(BtnCrouch, TEXT("CROUCH"), W - 185.0f, H - 360.0f, 100.0f, 90.0f, FLinearColor(0.15f, 0.15f, 0.18f, 0.50f)));
	Out.Add(MakeButton(BtnJump, TEXT("JUMP"), W - 185.0f, H - 480.0f, 100.0f, 90.0f, FLinearColor(0.15f, 0.15f, 0.18f, 0.50f)));
	Out.Add(MakeButton(BtnSwitch, TEXT("SWAP"), W - 520.0f, H - 175.0f, 100.0f, 90.0f, FLinearColor(0.15f, 0.15f, 0.18f, 0.50f)));
	Out.Add(MakeButton(BtnMenu, TEXT("|||"), W - 90.0f, 24.0f, 66.0f, 56.0f, FLinearColor(0.10f, 0.10f, 0.12f, 0.55f)));
}

void AEchoLinePlayerController::BuildMenuButtons(TArray<FEchoButton>& Out, const FVector2D& Size) const
{
	const float W = Size.X;
	const float H = Size.Y;
	const float BtnW = FMath::Min(420.0f, W - 80.0f);
	const float X = (W - BtnW) * 0.5f;
	const float BtnH = 74.0f;
	float Y = H * 0.42f;
	const float Gap = 20.0f;

	Out.Add(MakeButton(BtnDeploy, TEXT("DEPLOY"), X, Y, BtnW, BtnH, FLinearColor(0.10f, 0.45f, 0.90f, 0.60f)));
	Y += BtnH + Gap;
	Out.Add(MakeButton(BtnHost, TEXT("HOST (HOTSPOT / LAN)"), X, Y, BtnW, BtnH, FLinearColor(0.12f, 0.30f, 0.18f, 0.55f)));
	Y += BtnH + Gap;
	Out.Add(MakeButton(BtnJoin, TEXT("JOIN BY IP"), X, Y, BtnW, BtnH, FLinearColor(0.20f, 0.20f, 0.24f, 0.55f)));
	Y += BtnH + Gap;
	Out.Add(MakeButton(BtnSettings, TEXT("SETTINGS"), X, Y, BtnW, BtnH, FLinearColor(0.20f, 0.20f, 0.24f, 0.55f)));
	Y += BtnH + Gap;
	Out.Add(MakeButton(BtnAddBot, TEXT("ADD BOT (HOST)"), X, Y, BtnW, BtnH, FLinearColor(0.22f, 0.16f, 0.10f, 0.55f)));
}

void AEchoLinePlayerController::BuildResultsButtons(TArray<FEchoButton>& Out, const FVector2D& Size) const
{
	const float W = Size.X;
	const float H = Size.Y;
	const float BtnW = FMath::Min(420.0f, W - 80.0f);
	const float X = (W - BtnW) * 0.5f;
	const float BtnH = 74.0f;

	Out.Add(MakeButton(BtnReturnMenu, TEXT("MAIN MENU"), X, H * 0.60f, BtnW, BtnH, FLinearColor(0.20f, 0.20f, 0.24f, 0.55f)));
}

void AEchoLinePlayerController::BuildSettingsButtons(TArray<FEchoButton>& Out, const FVector2D& Size) const
{
	const float W = Size.X;
	const float H = Size.Y;
	const float RowH = 70.0f;
	const float LabelGap = 40.0f;

	float Y = H * 0.30f;
	const float CenterX = W * 0.5f;
	const float Side = 70.0f;

	// Sensitivity
	Out.Add(MakeButton(BtnSensDown, TEXT("-"), CenterX - 200.0f, Y, Side, RowH, FLinearColor(0.3f, 0.3f, 0.34f, 0.6f)));
	Out.Add(MakeButton(BtnSensUp, TEXT("+"), CenterX + 130.0f, Y, Side, RowH, FLinearColor(0.3f, 0.3f, 0.34f, 0.6f)));
	Y += RowH + LabelGap;

	// Field of view
	Out.Add(MakeButton(BtnFovDown, TEXT("-"), CenterX - 200.0f, Y, Side, RowH, FLinearColor(0.3f, 0.3f, 0.34f, 0.6f)));
	Out.Add(MakeButton(BtnFovUp, TEXT("+"), CenterX + 130.0f, Y, Side, RowH, FLinearColor(0.3f, 0.3f, 0.34f, 0.6f)));
	Y += RowH + LabelGap;

	// Invert Y
	Out.Add(MakeButton(BtnInvert, bInvertY ? TEXT("ON") : TEXT("OFF"), CenterX - 100.0f, Y, 200.0f, RowH, FLinearColor(0.3f, 0.3f, 0.34f, 0.6f)));
	Y += RowH + LabelGap;

	// Quality
	const TCHAR* QualityText = QualityLevel == 0 ? TEXT("LOW") : (QualityLevel == 2 ? TEXT("HIGH") : TEXT("MEDIUM"));
	Out.Add(MakeButton(BtnQuality, QualityText, CenterX - 100.0f, Y, 200.0f, RowH, FLinearColor(0.3f, 0.3f, 0.34f, 0.6f)));
	Y += RowH + LabelGap + 20.0f;

	Out.Add(MakeButton(BtnBack, TEXT("BACK"), CenterX - 120.0f, Y, 240.0f, RowH, FLinearColor(0.35f, 0.15f, 0.15f, 0.6f)));
}

void AEchoLinePlayerController::BuildKeypadButtons(TArray<FEchoButton>& Out, const FVector2D& Size) const
{
	const float W = Size.X;
	const float H = Size.Y;
	const float KeyW = 80.0f;
	const float KeyH = 70.0f;
	const float Gap = 12.0f;

	// 3x4 grid centered on screen.
	const float GridW = KeyW * 3.0f + Gap * 2.0f;
	const float GridX = (W - GridW) * 0.5f;
	const float GridY = H * 0.30f;

	const TCHAR* Keys[12] = {
		TEXT("1"), TEXT("2"), TEXT("3"),
		TEXT("4"), TEXT("5"), TEXT("6"),
		TEXT("7"), TEXT("8"), TEXT("9"),
		TEXT("Dot"), TEXT("0"), TEXT("Colon")
	};

	for (int32 i = 0; i < 12; ++i)
	{
		const int32 Row = i / 3;
		const int32 Col = i % 3;
		const float X = GridX + Col * (KeyW + Gap);
		const float Y = GridY + Row * (KeyH + Gap);

		FString Label = Keys[i];
		if (Label == TEXT("Dot")) { Label = TEXT("."); }
		else if (Label == TEXT("Colon")) { Label = TEXT(":"); }

		Out.Add(MakeButton(FName(Keys[i]), Label, X, Y, KeyW, KeyH, FLinearColor(0.2f, 0.2f, 0.24f, 0.6f)));
	}

	const float ActionY = GridY + 4.0f * (KeyH + Gap);
	Out.Add(MakeButton(BtnBackspace, TEXT("DEL"), GridX, ActionY, KeyW, KeyH, FLinearColor(0.35f, 0.2f, 0.2f, 0.6f)));
	Out.Add(MakeButton(BtnBack, TEXT("BACK"), GridX + KeyW + Gap, ActionY, KeyW, KeyH, FLinearColor(0.35f, 0.2f, 0.2f, 0.6f)));
	Out.Add(MakeButton(BtnConnect, TEXT("GO"), GridX + 2.0f * (KeyW + Gap), ActionY, KeyW, KeyH, FLinearColor(0.1f, 0.45f, 0.2f, 0.7f)));
}
