// EchoLine - canvas HUD. Drawn entirely in C++ so no UMG assets are required.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EchoHUD.generated.h"

struct FEchoButton;
class AEchoLinePlayerController;

UCLASS()
class ECHOLINE_API AEchoHUD : public AHUD
{
	GENERATED_BODY()

public:
	AEchoHUD();

	virtual void DrawHUD() override;

protected:
	void DrawPlayingHUD(AEchoLinePlayerController* PC);
	void DrawMenus(AEchoLinePlayerController* PC);
	void DrawButtons(const TArray<FEchoButton>& Buttons);
	void DrawCenterText(const FString& Text, float Y, const FLinearColor& Color, float Scale);
	void DrawTextAt(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale);
	float MeasureTextWidth(const FString& Text, float Scale) const;
};
