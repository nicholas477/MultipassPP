// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AccumulationMotionBlurBlendable.generated.h"

struct MULTIPASSPP_API FAccumulationMotionBlurNode
{
	static FName GetFName()
	{
		static FName Name = "FAccumulationMotionBlurNode";
		return Name;
	}

	float MotionBlurScale = 0.f;
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = ("Post Processing"))
class MULTIPASSPP_API UAccumulationMotionBlurBlendable : public UObject, public IBlendableInterface
{
	GENERATED_BODY()
public:
	virtual void OverrideBlendableSettings(class FSceneView& View, float Weight) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accumulation Motion Blur", meta=(ExposeOnSpawn=true))
		float MotionBlurScale = 0.01f;
};
