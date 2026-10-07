// EchoLine - player controller: touch controls, menus and settings.
// Touch controls are implemented in C++ (no UMG assets) so the prototype is asset-free.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Math/Box2D.h"
#include "Templates/Optional.h"
#include "EchoLinePlayerController.generated.h"

UENUM(BlueprintType)
enum class EMenuState : uint8
{
	MainMenu,
	Settings,
	Playing,
	Results
};

/** A touch button, rebuilt every frame from the current viewport size. */
struct FEchoButton
{
	FName Id;
	FString Label;
	FBox2D Rect;
	FLinearColor Tint = FLinearColor(0.05f, 0.05f, 0.06f, 0.55f);
	bool bPressed = false;
};

UCLASS()
class ECHOLINE_API AEchoLinePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AEchoLinePlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	virtual bool InputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, float Force, FDateTime DeviceTimestamp) override;

	// --- Menu actions ---
	void Deploy();
	void HostMatch();
	void JoinMatch(const FString& Address);
	void OpenSettings();
	void ReturnToMainMenu();
	void CloseSettingsToMenu();

	// --- Console helpers for testing on device without UMG ---
	UFUNCTION(exec)
	void EchoHost();

	UFUNCTION(exec)
	void EchoJoin(const FString& Address);

	// --- Settings ---
	UPROPERTY(EditAnywhere, Category = "Echo|Settings")
	float Sensitivity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Echo|Settings")
	float FieldOfView = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Echo|Settings")
	bool bInvertY = false;

	/** 0 = Low, 1 = Medium, 2 = High */
	UPROPERTY(EditAnywhere, Category = "Echo|Settings")
	int32 QualityLevel = 1;

	void LoadSettings();
	void SaveSettings();
	void ApplyQuality();

	// --- State for the HUD ---
	EMenuState GetMenuState() const { return MenuState; }
	bool IsEnteringIP() const { return bEnteringIP; }
	const FString& GetJoinIP() const { return JoinIP; }
	bool IsLookActive() const { return LookTouchHandle.IsSet(); }
	FVector2D GetMoveStickBase() const { return MoveStickBase; }
	FVector2D GetMoveStickKnob() const { return MoveStickKnob; }
	bool IsMoveStickActive() const { return bMoveStickActive; }

	/** Current on-screen buttons for the active screen. */
	void GetActiveButtons(TArray<FEchoButton>& Out) const;

	/** Fires when the weapon HUD should point at the given pawn. */
	class AEchoLineCharacter* GetEchoCharacter() const;
protected:
	// --- Touch handling ---
	void ProcessPlayingTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& Pos);
	void ProcessMenuTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& Pos);

	void BuildPlayingButtons(TArray<FEchoButton>& Out, const FVector2D& Size) const;
	void BuildMenuButtons(TArray<FEchoButton>& Out, const FVector2D& Size) const;
	void BuildResultsButtons(TArray<FEchoButton>& Out, const FVector2D& Size) const;
	void BuildSettingsButtons(TArray<FEchoButton>& Out, const FVector2D& Size) const;
	void BuildKeypadButtons(TArray<FEchoButton>& Out, const FVector2D& Size) const;

	void ActivateMenuButton(const FName& Id, float Direction);

	// --- Desktop input ---
	void InputTurn(float Value);
	void InputLookUp(float Value);
	void InputMenuClick();

	EMenuState MenuState = EMenuState::MainMenu;
	EMenuState PreviousMenuState = EMenuState::MainMenu;

	// Touch tracking.
	TOptional<uint32> MoveTouchHandle;
	TOptional<uint32> LookTouchHandle;
	TMap<uint32, FName> ActiveButtonTouches;

	FVector2D MoveStickBase = FVector2D::ZeroVector;
	FVector2D MoveStickKnob = FVector2D::ZeroVector;
	FVector2D MoveInput = FVector2D::ZeroVector;
	bool bMoveStickActive = false;

	FVector2D LastLookPos = FVector2D::ZeroVector;
	float MoveStickRadius = 140.0f;

	bool bEnteringIP = false;
	FString JoinIP = TEXT("192.168.43.1");
	bool bAppliedInitialSettings = false;
};
