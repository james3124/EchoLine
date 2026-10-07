// EchoLine - procedural art helpers.
// Builds operator bodies and weapon viewmodels out of engine basic shapes so the
// prototype has readable, distinct silhouettes without importing any models.

#pragma once

#include "CoreMinimal.h"
#include "EchoTypes.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;

namespace EchoArt
{
	/** Engine primitive meshes (available in every project). */
	UStaticMesh* Cube();
	UStaticMesh* Sphere();
	UStaticMesh* Cylinder();

	/** Creates and configures a StaticMeshComponent as a visual part. */
	// (Parts are created inside the owning actor's constructor; see EchoLineCharacter.)

	/** Reconfigures an existing part (used when swapping weapon models). */
	void ConfigurePart(UStaticMeshComponent* Part, UStaticMesh* Mesh, const FVector& Location,
		const FVector& Scale, const FLinearColor& Color);

	/** Applies a colour through the BasicShapeMaterial "Color" parameter when present. */
	void TintPart(UStaticMeshComponent* Part, const FLinearColor& Color);

	/** Per-operator colours from the design document. */
	FLinearColor OperatorPrimary(EOperatorId Op);
	FLinearColor OperatorSecondary(EOperatorId Op);

	/** Shared weapon metal. */
	FLinearColor Gunmetal();
}
