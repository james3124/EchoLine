// EchoLine - asset-free navigation.
// A grid is rasterised from the greybox's collision boxes and searched with A*.
// This avoids depending on a baked NavMesh (which would require editor-authored volumes).

#pragma once

#include "CoreMinimal.h"
#include "Math/Box.h"

struct FEchoNavGrid
{
	/** Rasterise the walkable area from collision boxes. Call once after the level is built. */
	void Build(const FVector2D& InMin, const FVector2D& InMax, float InCellSize,
		const TArray<FBox>& Obstacles, float AgentRadius, float MinObstacleHeight);

	bool IsValid() const { return NumX > 0 && NumY > 0; }

	bool IsWalkable(int32 X, int32 Y) const
	{
		return X >= 0 && Y >= 0 && X < NumX && Y < NumY && Blocked[Y * NumX + X] == 0;
	}

	bool WorldToCell(const FVector& World, int32& OutX, int32& OutY) const
	{
		const float FX = (World.X - Min.X) / CellSize;
		const float FY = (World.Y - Min.Y) / CellSize;
		OutX = FMath::FloorToInt(FX);
		OutY = FMath::FloorToInt(FY);
		return OutX >= 0 && OutY >= 0 && OutX < NumX && OutY < NumY;
	}

	FVector CellCenter(int32 X, int32 Y) const
	{
		return FVector(Min.X + (X + 0.5f) * CellSize, Min.Y + (Y + 0.5f) * CellSize, 0.0f);
	}

	bool IsWalkableWorld(const FVector& World) const
	{
		int32 X = 0, Y = 0;
		return WorldToCell(World, X, Y) && IsWalkable(X, Y);
	}

	/** Nearest walkable cell to a world point, searching outwards. */
	bool NearestWalkableCell(const FVector& World, int32& OutX, int32& OutY) const;

	/** Random walkable world point. Falls back to the grid centre. */
	FVector RandomWalkablePoint(FRandomStream& Stream) const;

	/** A* between two world points. Returns false if no route exists. */
	bool FindPath(const FVector& Start, const FVector& End, TArray<FVector>& OutPath) const;

	int32 NumX = 0;
	int32 NumY = 0;
	float CellSize = 50.0f;
	FVector2D Min = FVector2D::ZeroVector;

	/** 0 = walkable, 1 = blocked. */
	TArray<uint8> Blocked;

private:
	/** True if the straight grid line between two cells is free of blockers. */
	bool GridLineClear(int32 X0, int32 Y0, int32 X1, int32 Y1) const;
};