#include "EchoHUD.h"

#include "EchoGameState.h"
#include "EchoLineCharacter.h"
#include "EchoLinkComponent.h"
#include "EchoLinePlayerController.h"
#include "EchoPlayerState.h"
#include "EchoWeaponComponent.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

AEchoHUD::AEchoHUD()
{
}

void AEchoHUD::DrawHUD()
{
	Super::DrawHUD();

	AEchoLinePlayerController* PC = Cast<AEchoLinePlayerController>(PlayerOwner);
	if (!PC || !Canvas)
	{
		return;
	}

	const EMenuState State = PC->GetMenuState();
	if (State == EMenuState::Playing)
	{
		DrawPlayingHUD(PC);
	}
	else
	{
		DrawMenus(PC);
	}

	// On-screen controls for menus are drawn for every non-playing screen.
	if (State != EMenuState::Playing)
	{
		TArray<FEchoButton> Buttons;
		PC->GetActiveButtons(Buttons);
		DrawButtons(Buttons);
	}
}

void AEchoHUD::DrawCenterText(const FString& Text, float Y, const FLinearColor& Color, float Scale)
{
	const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font || !Canvas)
	{
		return;
	}

	float XL = 0.0f;
	float YL = 0.0f;
	Canvas->TextSize(Font, Text, XL, YL, Scale, Scale);

	const float X = (Canvas->SizeX - XL) * 0.5f;
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Canvas->DrawItem(Item);
}

float AEchoHUD::MeasureTextWidth(const FString& Text, float Scale) const
{
	const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font || !Canvas)
	{
		return 0.0f;
	}
	float XL = 0.0f;
	float YL = 0.0f;
	Canvas->TextSize(Font, Text, XL, YL, Scale, Scale);
	return XL;
}

void AEchoHUD::DrawTextAt(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale)
{
	const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font || !Canvas)
	{
		return;
	}

	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Canvas->DrawItem(Item);
}

void AEchoHUD::DrawMenus(AEchoLinePlayerController* PC)
{
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;

	// Dim the world behind menus.
	DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.72f), 0.0f, 0.0f, W, H);

	DrawCenterText(TEXT("ECHO"), H * 0.12f, FLinearColor(0.35f, 0.75f, 1.0f), 3.0f);
	DrawCenterText(TEXT("L I N E"), H * 0.20f, FLinearColor(0.9f, 0.9f, 0.95f), 1.6f);
	DrawCenterText(TEXT("offline multiplayer - realistic sci-fi arena shooter"), H * 0.26f, FLinearColor(0.6f, 0.65f, 0.7f), 1.0f);

	if (PC->GetMenuState() == EMenuState::Settings)
	{
		DrawCenterText(TEXT("SETTINGS"), H * 0.22f, FLinearColor::White, 1.8f);
		DrawCenterText(FString::Printf(TEXT("Sensitivity   %.1f"), PC->Sensitivity), H * 0.30f + 18.0f, FLinearColor::White, 1.2f);
		DrawCenterText(FString::Printf(TEXT("Field of View  %.0f"), PC->FieldOfView), H * 0.30f + 128.0f, FLinearColor::White, 1.2f);
		DrawCenterText(TEXT("Invert Y"), H * 0.30f + 238.0f, FLinearColor::White, 1.2f);
		DrawCenterText(TEXT("Graphics"), H * 0.30f + 348.0f, FLinearColor::White, 1.2f);
		return;
	}

	if (PC->IsEnteringIP())
	{
		DrawCenterText(TEXT("JOIN MATCH BY IP"), H * 0.20f, FLinearColor::White, 1.8f);
		DrawCenterText(FString::Printf(TEXT("[ %s ]"), *PC->GetJoinIP()), H * 0.26f, FLinearColor(0.4f, 0.9f, 0.5f), 1.4f);
		DrawCenterText(TEXT("Host runs on hotspot IP, often 192.168.43.1"), H * 0.26f + 40.0f, FLinearColor(0.6f, 0.65f, 0.7f), 0.9f);
		return;
	}

	if (PC->GetMenuState() == EMenuState::Results)
	{
		const AEchoGameState* GS = GetWorld() ? GetWorld()->GetGameState<AEchoGameState>() : nullptr;
		if (GS)
		{
			DrawCenterText(TEXT("MATCH COMPLETE"), H * 0.14f, FLinearColor(1.0f, 0.8f, 0.3f), 2.0f);
			DrawCenterText(FString::Printf(TEXT("Winner: %s"), *GS->WinnerName), H * 0.21f, FLinearColor::White, 1.4f);
		}
	}
}

void AEchoHUD::DrawPlayingHUD(AEchoLinePlayerController* PC)
{
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;

	AEchoLineCharacter* Character = PC->GetEchoCharacter();
	const AEchoGameState* GS = GetWorld() ? GetWorld()->GetGameState<AEchoGameState>() : nullptr;

	// --- Crosshair ---
	if (Character && !Character->IsDead())
	{
		float Spread = 14.0f;
		if (const UEchoWeaponComponent* WeaponComp = Character->Weapon)
		{
			Spread = 8.0f + WeaponComp->GetCurrentSpreadDegrees() * 6.0f;
		}

		const float CX = W * 0.5f;
		const float CY = H * 0.5f;
		const FLinearColor Color(0.9f, 0.95f, 1.0f, 0.85f);
		const float Len = 12.0f;
		const float Th = 2.0f;

		DrawLine(CX - Spread - Len, CY, CX - Spread, CY, Color, Th);
		DrawLine(CX + Spread, CY, CX + Spread + Len, CY, Color, Th);
		DrawLine(CX, CY - Spread - Len, CX, CY - Spread, Color, Th);
		DrawLine(CX, CY + Spread, CX, CY + Spread + Len, Color, Th);
		DrawRect(Color, CX - 1.0f, CY - 1.0f, 2.0f, 2.0f);

		// --- Echo status ---
		if (const class UEchoLinkComponent* Echo = Character->Echo)
		{
			FString EchoText;
			FLinearColor EchoColor(0.5f, 0.7f, 1.0f);
			if (Echo->IsEchoArmed())
			{
				EchoText = TEXT("ECHO ARMED - fire a panel");
				EchoColor = FLinearColor(0.3f, 0.9f, 1.0f);
			}
			else if (Echo->HasLink())
			{
				EchoText = TEXT("ECHO LINK ACTIVE");
				EchoColor = FLinearColor(0.3f, 1.0f, 0.5f);
			}
			else
			{
				EchoText = FString::Printf(TEXT("ECHO ANCHORS %d/2 - press ECHO"), Echo->GetAnchorCount());
			}
			DrawCenterText(EchoText, H - 150.0f, EchoColor, 1.0f);
		}

		// --- Hitmarker ---
		if (const UEchoWeaponComponent* WeaponComp = Character->Weapon)
		{
			const float TimeSince = GetWorld()->GetTimeSeconds() - WeaponComp->GetLastHitTime();
			if (TimeSince < 0.25f)
			{
				const FLinearColor HitColor = WeaponComp->WasLastHitKill() ? FLinearColor(1.0f, 0.3f, 0.2f) : FLinearColor(1.0f, 1.0f, 1.0f);
				const float CX = W * 0.5f;
				const float CY = H * 0.5f;
				DrawLine(CX - 16, CY - 16, CX - 6, CY - 6, HitColor, 3.0f);
				DrawLine(CX + 6, CY + 6, CX + 16, CY + 16, HitColor, 3.0f);
				DrawLine(CX - 16, CY + 16, CX - 6, CY + 6, HitColor, 3.0f);
				DrawLine(CX + 6, CY - 6, CX + 16, CY - 16, HitColor, 3.0f);
			}
		}
	}

	// --- Health ---
	if (Character)
	{
		const float HealthPct = FMath::Clamp(Character->GetHealth() / 100.0f, 0.0f, 1.0f);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), 30.0f, H - 80.0f, 320.0f, 30.0f);
		DrawRect(FLinearColor(0.2f + (1.0f - HealthPct) * 0.7f, 0.2f + HealthPct * 0.6f, 0.25f, 0.9f), 33.0f, H - 77.0f, 314.0f * HealthPct, 24.0f);
		DrawTextAt(FString::Printf(TEXT("HEALTH %d"), FMath::RoundToInt(Character->GetHealth())), 38.0f, H - 76.0f, FLinearColor::White, 1.0f);
	}

	// --- Ammo and weapon ---
	if (Character && Character->Weapon)
	{
		const UEchoWeaponComponent* WeaponComp = Character->Weapon;
		const FWeaponStats& Stats = WeaponComp->GetStats();
		const FString AmmoText = WeaponComp->IsReloading()
			? TEXT("RELOADING...")
			: FString::Printf(TEXT("%d / %d"), WeaponComp->GetAmmo(), Stats.MagazineSize);
		DrawCenterText(AmmoText, H - 60.0f, FLinearColor(0.95f, 0.95f, 0.9f), 1.6f);
		DrawCenterText(Stats.DisplayName, H - 30.0f, FLinearColor(0.6f, 0.7f, 0.8f), 1.0f);
	}

	// --- Timer ---
	if (GS)
	{
		const int32 Seconds = FMath::CeilToInt(GS->TimeLeft);
		const FString TimeText = FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60);
		DrawCenterText(TimeText, 20.0f, FLinearColor::White, 1.6f);
	}

	// --- Scoreboard (top right) ---
	if (GS && GetWorld())
	{
		float Y = 100.0f;
		const float RightEdge = W - 30.0f;
		DrawTextAt(TEXT("SCORE"), RightEdge - MeasureTextWidth(TEXT("SCORE"), 0.9f), Y - 26.0f, FLinearColor(0.7f, 0.75f, 0.8f), 0.9f);

		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* OtherPC = It->Get())
			{
				if (const AEchoPlayerState* PS = OtherPC->GetPlayerState<AEchoPlayerState>())
				{
					const FLinearColor Color = (OtherPC == PC) ? FLinearColor(0.4f, 0.9f, 0.5f) : FLinearColor::White;
					const FString Row = FString::Printf(TEXT("%s   %d    (deaths %d)"), *PS->GetPlayerName(), FMath::RoundToInt(PS->GetScore()), PS->Deaths);
					DrawTextAt(Row, RightEdge - MeasureTextWidth(Row, 0.9f), Y, Color, 0.9f);
					Y += 22.0f;
				}
			}
		}

		// Bots have no PlayerState, so their scores come from the game state.
		for (const TPair<FString, int32>& Pair : GS->BotScores)
		{
			const int32* Deaths = GS->BotDeaths.Find(Pair.Key);
			const FString Row = FString::Printf(TEXT("%s   %d    (deaths %d)"), *Pair.Key, Pair.Value, Deaths ? *Deaths : 0);
			DrawTextAt(Row, RightEdge - MeasureTextWidth(Row, 0.9f), Y, FLinearColor(0.95f, 0.8f, 0.55f), 0.9f);
			Y += 22.0f;
		}
	}

	// --- Kill feed (top left) ---
	if (GS)
	{
		float Y = 90.0f;
		for (int32 i = GS->KillFeed.Num() - 1; i >= 0; --i)
		{
			const FEchoKillFeedEntry& Entry = GS->KillFeed[i];
			const FString Text = FString::Printf(TEXT("%s  >  %s"), *Entry.KillerName, *Entry.VictimName);
			DrawTextAt(Text, 30.0f, Y, FLinearColor(0.95f, 0.7f, 0.6f), 0.9f);
			Y += 20.0f;
		}
	}

	// --- Death overlay ---
	if (Character && Character->IsDead())
	{
		DrawRect(FLinearColor(0.4f, 0.0f, 0.0f, 0.25f), 0.0f, 0.0f, W, H);
		DrawCenterText(TEXT("ELIMINATED - respawning"), H * 0.45f, FLinearColor(1.0f, 0.5f, 0.4f), 1.6f);
	}

	// --- Touch controls ---
	TArray<FEchoButton> Buttons;
	PC->GetActiveButtons(Buttons);
	DrawButtons(Buttons);

	// --- Movement stick ---
	if (PC->IsMoveStickActive())
	{
		const FVector2D Base = PC->GetMoveStickBase();
		const FVector2D Knob = PC->GetMoveStickKnob();
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), Base.X - 140.0f, Base.Y - 140.0f, 280.0f, 280.0f);
		DrawRect(FLinearColor(0.4f, 0.7f, 1.0f, 0.35f), Knob.X - 40.0f, Knob.Y - 40.0f, 80.0f, 80.0f);
	}
}

void AEchoHUD::DrawButtons(const TArray<FEchoButton>& Buttons)
{
	if (!Canvas)
	{
		return;
	}

	for (const FEchoButton& Button : Buttons)
	{
		const FVector2D Min = Button.Rect.Min;
		const FVector2D Max = Button.Rect.Max;
		const float BW = Max.X - Min.X;
		const float BH = Max.Y - Min.Y;

		FLinearColor Border(0.6f, 0.7f, 0.8f, 0.5f);
		if (Button.Id == TEXT("Echo"))
		{
			Border = FLinearColor(0.3f, 0.9f, 1.0f, 0.8f);
		}

		DrawRect(Button.Tint, Min.X, Min.Y, BW, BH);
		DrawRect(Border, Min.X, Min.Y, BW, 2.0f);
		DrawRect(Border, Min.X, Min.Y + BH - 2.0f, BW, 2.0f);
		DrawRect(Border, Min.X, Min.Y, 2.0f, BH);
		DrawRect(Border, Min.X + BW - 2.0f, Min.Y, 2.0f, BH);

		const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
		if (Font && Canvas)
		{
			float XL = 0.0f;
			float YL = 0.0f;
			Canvas->TextSize(Font, Button.Label, XL, YL, 1.0f, 1.0f);
			FCanvasTextItem Item(FVector2D(Min.X + (BW - XL) * 0.5f, Min.Y + (BH - YL) * 0.5f), FText::FromString(Button.Label), Font, FLinearColor::White);
			Canvas->DrawItem(Item);
		}
	}
}
