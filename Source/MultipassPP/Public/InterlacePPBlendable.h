// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/BlendableInterface.h"
#include "InterlacePPBlendable.generated.h"

struct MULTIPASSPP_API FInterlacePPNode
{
	static FName GetFName()
	{
		static FName Name = "FInterlacepPPNode";
		return Name;
	}
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = ("Post Processing"))
class MULTIPASSPP_API UInterlacePPBlendable : public UObject, public IBlendableInterface
{
	GENERATED_BODY()

public:
	virtual void OverrideBlendableSettings(class FSceneView& View, float Weight) const override;
};
