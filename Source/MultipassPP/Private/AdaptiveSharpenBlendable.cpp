// Fill out your copyright notice in the Description page of Project Settings.


#include "AdaptiveSharpenBlendable.h"

void UAdaptiveSharpenBlendable::OverrideBlendableSettings(FSceneView& View, float Weight) const
{
	check(Weight > 0.0f && Weight <= 1.0f);

	if (!View.State)
	{
		return;
	}

	FFinalPostProcessSettings& Dest = View.FinalPostProcessSettings;

	FAdaptiveSharpenNode Node;
	Node.Strength = Strength;
	FAdaptiveSharpenNode* PushedNode = Dest.BlendableManager.PushBlendableData(Weight, Node);
}
