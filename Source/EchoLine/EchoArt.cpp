#include "EchoArt.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace EchoArt
{
	UStaticMesh* LoadMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	UStaticMesh* Cube()
	{
		static UStaticMesh* Mesh = LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
		return Mesh;
	}

	UStaticMesh* Sphere()
	{
		static UStaticMesh* Mesh = LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		return Mesh;
	}

	UStaticMesh* Cylinder()
	{
		static UStaticMesh* Mesh = LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		return Mesh;
	}

	void TintPart(UStaticMeshComponent* Part, const FLinearColor& Color)
	{
		if (!Part)
		{
			return;
		}
		if (UMaterialInstanceDynamic* MID = Part->CreateDynamicMaterialInstance(0))
		{
			// BasicShapeMaterial exposes "Color"; harmless if the parameter is absent.
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}

	void ConfigurePart(UStaticMeshComponent* Part, UStaticMesh* Mesh, const FVector& Location,
		const FVector& Scale, const FLinearColor& Color)
	{
		if (!Part)
		{
			return;
		}

		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		if (Mesh)
		{
			Part->SetStaticMesh(Mesh);
		}
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		TintPart(Part, Color);
	}

	UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, const FName& Name,
		UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color)
	{
		if (!Owner || !Parent)
		{
			return nullptr;
		}

		UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		if (!Part)
		{
			return nullptr;
		}
		Part->SetupAttachment(Parent);
		ConfigurePart(Part, Mesh, Location, Scale, Color);
		return Part;
	}

	UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, const FName& Name,
		UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color)
	{
		(void)Owner; (void)Parent; (void)Name; (void)Mesh; (void)Location; (void)Scale; (void)Color;
		return nullptr;
	}

	FLinearColor OperatorPrimary(EOperatorId Op)
	{
		switch (Op)
		{
		case EOperatorId::Wisp:  return FLinearColor(0.20f, 0.29f, 0.22f);
		case EOperatorId::Patch: return FLinearColor(0.49f, 0.41f, 0.29f);
		case EOperatorId::Cairn: return FLinearColor(0.78f, 0.79f, 0.78f);
		case EOperatorId::Rook:
		default:                 return FLinearColor(0.27f, 0.32f, 0.38f);
		}
	}

	FLinearColor OperatorSecondary(EOperatorId Op)
	{
		switch (Op)
		{
		case EOperatorId::Wisp:  return FLinearColor(0.13f, 0.20f, 0.15f);
		case EOperatorId::Patch: return FLinearColor(0.91f, 0.51f, 0.16f);
		case EOperatorId::Cairn: return FLinearColor(0.47f, 0.51f, 0.52f);
		case EOperatorId::Rook:
		default:                 return FLinearColor(0.85f, 0.86f, 0.88f);
		}
	}

	FLinearColor Gunmetal()
	{
		return FLinearColor(0.17f, 0.18f, 0.20f);
	}
}
