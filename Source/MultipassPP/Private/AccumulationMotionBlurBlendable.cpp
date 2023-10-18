// Fill out your copyright notice in the Description page of Project Settings.


#include "AccumulationMotionBlurBlendable.h"

void UAccumulationMotionBlurBlendable::OverrideBlendableSettings(FSceneView& View, float Weight) const
{
	check(Weight > 0.0f && Weight <= 1.0f);

	if (!View.State)
	{
		return;
	}

	FFinalPostProcessSettings& Dest = View.FinalPostProcessSettings;

	FAccumulationMotionBlurNode Node;
	Node.MotionBlurScale = MotionBlurScale;
	FAccumulationMotionBlurNode* PushedNode = Dest.BlendableManager.PushBlendableData(Weight, Node);
}
