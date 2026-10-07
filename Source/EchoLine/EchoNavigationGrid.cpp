#include "EchoNavigationGrid.h"

#include "Algo/Reverse.h"
#include "Math/UnrealMathUtility.h"

void FEchoNavGrid::Build(const FVector2D& InMin, const FVector2D& InMax, float InCellSize,
	const TArray<FBox>& Obstacles, float AgentRadius, float MinObstacleHeight)
{
	Min = InMin;
	CellSize = FMath::Max(10.0f, InCellSize);

	NumX = FMath::Max(0, FMath::CeilToInt((InMax.X - InMin.X) / CellSize));
	NumY = FMath::Max(0, FMath::CeilToInt((InMax.Y - InMin.Y) / CellSize));

	Blocked.Init(0, NumX * NumY);

	for (const FBox& Box : Obstacles)
	{
		// Low boxes (like the ground plane) are not obstacles.
		if (Box.Max.Z <= MinObstacleHeight)
		{
			continue;
		}

		// Expand by the agent radius so paths keep clear of walls.
		const float X0 = Box.Min.X - AgentRadius;
		const float X1 = Box.Max.X + AgentRadius;
		const float Y0 = Box.Min.Y - AgentRadius;
		const float Y1 = Box.Max.Y + AgentRadius;

		int32 CX0 = FMath::FloorToInt((X0 - Min.X) / CellSize);
		int32 CX1 = FMath::FloorToInt((X1 - Min.X) / CellSize);
		int32 CY0 = FMath::FloorToInt((Y0 - Min.Y) / CellSize);
		int32 CY1 = FMath::FloorToInt((Y1 - Min.Y) / CellSize);

		CX0 = FMath::Clamp(CX0, 0, NumX - 1);
		CX1 = FMath::Clamp(CX1, 0, NumX - 1);
		CY0 = FMath::Clamp(CY0, 0, NumY - 1);
		CY1 = FMath::Clamp(CY1, 0, NumY - 1);

		for (int32 Y = CY0; Y <= CY1; ++Y)
		{
			for (int32 X = CX0; X <= CX1; ++X)
			{
				Blocked[Y * NumX + X] = 1;
			}
		}
	}
}

bool FEchoNavGrid::NearestWalkableCell(const FVector& World, int32& OutX, int32& OutY) const
{
	if (!IsValid())
	{
		return false;
	}

	int32 CX = 0, CY = 0;
	WorldToCell(World, CX, CY);
	CX = FMath::Clamp(CX, 0, NumX - 1);
	CY = FMath::Clamp(CY, 0, NumY - 1);

	if (IsWalkable(CX, CY))
	{
		OutX = CX;
		OutY = CY;
		return true;
	}

	// Expanding ring search.
	for (int32 Radius = 1; Radius < FMath::Max(NumX, NumY); ++Radius)
	{
		for (int32 DY = -Radius; DY <= Radius; ++DY)
		{
			for (int32 DX = -Radius; DX <= Radius; ++DX)
			{
				// Only the ring perimeter.
				if (FMath::Abs(DX) != Radius && FMath::Abs(DY) != Radius)
				{
					continue;
				}
				const int32 NX = CX + DX;
				const int32 NY = CY + DY;
				if (IsWalkable(NX, NY))
				{
					OutX = NX;
					OutY = NY;
					return true;
				}
			}
		}
	}
	return false;
}

FVector FEchoNavGrid::RandomWalkablePoint(FRandomStream& Stream) const
{
	if (!IsValid())
	{
		return FVector(Min.X, Min.Y, 0.0f);
	}

	for (int32 Attempt = 0; Attempt < 64; ++Attempt)
	{
		const int32 X = Stream.RandRange(0, NumX - 1);
		const int32 Y = Stream.RandRange(0, NumY - 1);
		if (IsWalkable(X, Y))
		{
			return CellCenter(X, Y);
		}
	}

	return CellCenter(NumX / 2, NumY / 2);
}

bool FEchoNavGrid::GridLineClear(int32 X0, int32 Y0, int32 X1, int32 Y1) const
{
	// Bresenham line over cells.
	int32 DX = FMath::Abs(X1 - X0);
	int32 DY = -FMath::Abs(Y1 - Y0);
	int32 SX = X0 < X1 ? 1 : -1;
	int32 SY = Y0 < Y1 ? 1 : -1;
	int32 Err = DX + DY;

	int32 X = X0;
	int32 Y = Y0;

	for (int32 Guard = 0; Guard < NumX + NumY + 4; ++Guard)
	{
		if (!IsWalkable(X, Y))
		{
			return false;
		}
		if (X == X1 && Y == Y1)
		{
			return true;
		}
		const int32 E2 = 2 * Err;
		if (E2 >= DY)
		{
			Err += DY;
			X += SX;
		}
		if (E2 <= DX)
		{
			Err += DX;
			Y += SY;
		}
	}
	return false;
}

bool FEchoNavGrid::FindPath(const FVector& Start, const FVector& End, TArray<FVector>& OutPath) const
{
	OutPath.Reset();

	if (!IsValid())
	{
		return false;
	}

	int32 SX = 0, SY = 0, EX = 0, EY = 0;
	if (!WorldToCell(Start, SX, SY) || !IsWalkable(SX, SY))
	{
		if (!NearestWalkableCell(Start, SX, SY))
		{
			return false;
		}
	}
	if (!WorldToCell(End, EX, EY) || !IsWalkable(EX, EY))
	{
		if (!NearestWalkableCell(End, EX, EY))
		{
			return false;
		}
	}

	if (SX == EX && SY == EY)
	{
		OutPath.Add(CellCenter(EX, EY));
		return true;
	}

	const int32 Count = NumX * NumY;
	TArray<float> GScore;
	TArray<int32> CameFrom;
	TArray<uint8> Closed;
	GScore.Init(TNumericLimits<float>::Max(), Count);
	CameFrom.Init(-1, Count);
	Closed.Init(0, Count);

	auto Index = [this](int32 X, int32 Y) { return Y * NumX + X; };
	auto Heuristic = [EX, EY](int32 X, int32 Y)
	{
		const float DX = FMath::Abs(static_cast<float>(X - EX));
		const float DY = FMath::Abs(static_cast<float>(Y - EY));
		return (DX + DY) + (1.41421356f - 2.0f) * FMath::Min(DX, DY);
	};

	const int32 StartIndex = Index(SX, SY);
	GScore[StartIndex] = 0.0f;

	TArray<int32> Open;
	auto Better = [&GScore](int32 A, int32 B) { return GScore[A] < GScore[B]; };
	Open.HeapPush(StartIndex, Better);

	static const int32 DirX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	static const int32 DirY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
	static const float DirCost[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f };

	bool bFound = false;
	while (Open.Num() > 0)
	{
		int32 Current = 0;
		Open.HeapPop(Current, Better);

		if (Closed[Current])
		{
			continue;
		}
		Closed[Current] = 1;

		const int32 CX = Current % NumX;
		const int32 CY = Current / NumX;

		if (CX == EX && CY == EY)
		{
			bFound = true;
			break;
		}

		for (int32 D = 0; D < 8; ++D)
		{
			const int32 NX = CX + DirX[D];
			const int32 NY = CY + DirY[D];
			if (!IsWalkable(NX, NY))
			{
				continue;
			}
			// Diagonal moves require both orthogonal neighbours to be free.
			if (D >= 4 && (!IsWalkable(CX + DirX[D], CY) || !IsWalkable(CX, CY + DirY[D])))
			{
				continue;
			}

			const int32 NIndex = Index(NX, NY);
			if (Closed[NIndex])
			{
				continue;
			}

			const float Tentative = GScore[Current] + DirCost[D];
			if (Tentative < GScore[NIndex])
			{
				GScore[NIndex] = Tentative;
				CameFrom[NIndex] = Current;
				Open.HeapPush(NIndex, Better);
			}
		}
	}

	if (!bFound)
	{
		return false;
	}

	// Reconstruct cell indices.
	TArray<int32> Cells;
	int32 Current = Index(EX, EY);
	while (Current != -1)
	{
		Cells.Add(Current);
		if (Current == StartIndex)
		{
			break;
		}
		Current = CameFrom[Current];
	}
	Algo::Reverse(Cells);

	// Convert to world points.
	TArray<FVector> Raw;
	Raw.Reserve(Cells.Num());
	for (int32 Cell : Cells)
	{
		Raw.Add(CellCenter(Cell % NumX, Cell / NumX));
	}

	// String-pull: drop intermediate points when a straight line is clear.
	if (Raw.Num() <= 2)
	{
		OutPath = Raw;
		return OutPath.Num() > 0;
	}

	OutPath.Add(Raw[0]);
	int32 Anchor = 0;
	for (int32 i = 2; i < Raw.Num(); ++i)
	{
		int32 AX = 0, AY = 0, IX = 0, IY = 0;
		WorldToCell(Raw[Anchor], AX, AY);
		WorldToCell(Raw[i], IX, IY);
		if (!GridLineClear(AX, AY, IX, IY))
		{
			OutPath.Add(Raw[i - 1]);
			Anchor = i - 1;
		}
	}
	OutPath.Add(Raw.Last());
	return true;
}
