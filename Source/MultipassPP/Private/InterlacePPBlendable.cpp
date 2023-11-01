// Fill out your copyright notice in the Description page of Project Settings.


#include "InterlacePPBlendable.h"

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "SceneRendering.h"
#endif


void UInterlacePPBlendable::OverrideBlendableSettings(FSceneView& View, float Weight) const
{
	check(Weight > 0.0f && Weight <= 1.0f);

	if (!View.State)
	{
		return;
	}

	FFinalPostProcessSettings& Dest = View.FinalPostProcessSettings;

	FInterlacePPNode Node;
	FInterlacePPNode* PushedNode = Dest.BlendableManager.PushBlendableData(Weight, Node);
}
