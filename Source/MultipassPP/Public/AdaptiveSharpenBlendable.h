// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/BlendableInterface.h"
#include "AdaptiveSharpenBlendable.generated.h"

struct MULTIPASSPP_API FAdaptiveSharpenNode
{
	static FName GetFName()
	{
		static FName Name = "FAdaptiveSharpenNode";
		return Name;
	}

	float Strength = 0.f;
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = ("Post Processing"))
class MULTIPASSPP_API UAdaptiveSharpenBlendable : public UObject, public IBlendableInterface
{
	GENERATED_BODY()

public:
	virtual void OverrideBlendableSettings(class FSceneView& View, float Weight) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Sharpening", meta = (ExposeOnSpawn = true))
		float Strength = 1.f;
	
};
