// Copyright Epic Games, Inc. All Rights Reserved.

#include "AccumulationMotionBlurSceneExtension.h"

#include "SceneView.h"
#include "ScreenPass.h"
#include "CommonRenderResources.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"
#include "ScenePrivate.h"
#include "Engine/TextureRenderTarget2D.h"
#include "AccumulationMotionBlurBlendable.h"

IMPLEMENT_GLOBAL_SHADER(FAccumulationMotionBlurPixelShader, "/MultipassPP/Private/AccumulationMotionBlurPP.usf", "AccumulationMotionBlurPS", SF_Pixel);

static TAutoConsoleVariable<float> CVarAccumulationMotionBlurScale(
	TEXT("r.AccumulationMotionBlur.Scale"),
	-1.f,
	TEXT(""),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAccumulationMotionBlurWeight(
	TEXT("r.AccumulationMotionBlur.Weight"),
	-1.f,
	TEXT(""),
	ECVF_Default);


FAccumulationMotionBlurSceneExtension::FAccumulationMotionBlurSceneExtension(const FAutoRegister& AutoReg)
	: BaseT(AutoReg)
{
	PostProcessingPassName = "Accumulation Motion Blur";
	BlendState = TStaticBlendState<>::GetRHI();
	PostProcessingPasses = { EPostProcessingPass::Tonemap };
}

void FAccumulationMotionBlurSceneExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	BaseT::SetupView(InViewFamily, InView);

	TSharedPtr<FAccumulationMotionBlurViewData> ViewData = StaticCastSharedPtr<FAccumulationMotionBlurViewData>(GetViewData(InView));
	if (ViewData != nullptr)
	{
		ViewData->Weight = 0.f;
		ViewData->Scale = 0.f;

		if (CVarAccumulationMotionBlurScale.GetValueOnAnyThread() >= 0.f)
		{
			ViewData->Scale = FMath::Clamp(CVarAccumulationMotionBlurScale.GetValueOnAnyThread(), 0, 1);
		}
		else
		{
			const FFinalPostProcessSettings& Dest = InView.FinalPostProcessSettings;
			FBlendableEntry* BlendableIt = nullptr;
			int32 NumEntries = 0;
			while (FAccumulationMotionBlurNode* DataPtr = Dest.BlendableManager.IterateBlendables<FAccumulationMotionBlurNode>(BlendableIt))
			{
				if (DataPtr)
				{
					ViewData->Scale += DataPtr->MotionBlurScale;
					NumEntries++;
				}
			}

			if (NumEntries > 0)
			{
				ViewData->Scale /= NumEntries;
			}
		}

		if (CVarAccumulationMotionBlurWeight.GetValueOnAnyThread() >= 0.f)
		{
			ViewData->Weight = FMath::Clamp(CVarAccumulationMotionBlurWeight.GetValueOnAnyThread(), 0, 1);
		}
		else
		{
			const FFinalPostProcessSettings& Dest = InView.FinalPostProcessSettings;
			FBlendableEntry* BlendableIt = nullptr;
			int32 NumEntries = 0;
			while (FAccumulationMotionBlurNode* DataPtr = Dest.BlendableManager.IterateBlendables<FAccumulationMotionBlurNode>(BlendableIt))
			{
				if (BlendableIt)
				{
					ViewData->Weight += BlendableIt->Weight;
					NumEntries++;
				}
			}

			if (NumEntries > 0)
			{
				ViewData->Weight /= NumEntries;
			}
		}
	}
}

bool FAccumulationMotionBlurSceneExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	check(IsInGameThread());

	bool bBlurScaleActive = true;
	bool bBlurWeightActive = true;

	if (CVarAccumulationMotionBlurScale.GetValueOnGameThread() >= 0.f)
	{
		bBlurScaleActive = CVarAccumulationMotionBlurScale.GetValueOnGameThread() > 0.f;
	}

	if (CVarAccumulationMotionBlurWeight.GetValueOnGameThread() >= 0.f)
	{
		bBlurWeightActive = CVarAccumulationMotionBlurWeight.GetValueOnGameThread() > 0.f;
	}

	return BaseT::IsActiveThisFrame_Internal(Context) && bBlurScaleActive && bBlurWeightActive;
}

FScreenPassTexture FAccumulationMotionBlurSceneExtension::PostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs, EPostProcessingPass Pass)
{
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);

	TSharedPtr<FAccumulationMotionBlurViewData> ViewData = StaticCastSharedPtr<FAccumulationMotionBlurViewData>(GetViewData(View));
	if (!ViewData)
	{
		return ReturnUntouchedSceneColorForPostProcessing(GraphBuilder, View, ViewInfo, InOutInputs);
	}

	if (ViewData->Weight <= 0.f)
	{
		return ReturnUntouchedSceneColorForPostProcessing(GraphBuilder, View, ViewInfo, InOutInputs);
	}

	if (ViewData->Scale <= 0.f)
	{
		return ReturnUntouchedSceneColorForPostProcessing(GraphBuilder, View, ViewInfo, InOutInputs);
	}

	return BaseT::PostProcessPass_RenderThread(GraphBuilder, View, InOutInputs, Pass);
}

void FAccumulationMotionBlurSceneExtension::SetupParameters(FRDGBuilder& GraphBuilder, const FSceneView& View, const FViewInfo& ViewInfo, const FScreenPassTexture& Input, const FScreenPassRenderTarget& Output, FAccumulationMotionBlurPixelShader::FParameters* Parameters)
{
	TSharedPtr<FAccumulationMotionBlurViewData> ViewData = StaticCastSharedPtr<FAccumulationMotionBlurViewData>(GetViewData(View));
	Parameters->InputTexture = Input.Texture;
	Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
	Parameters->MotionBlurTexture = Output.Texture;
	Parameters->MotionBlurSampler = TStaticSamplerState<>::GetRHI();

	Parameters->LastFrameNumber = ViewData->LastFrameNumber++;

	Parameters->InputTextureSize = Input.Texture->Desc.Extent;
	Parameters->OutputTextureSize = Output.Texture->Desc.Extent;

	Parameters->DeltaTime = ViewInfo.ViewState->LastRenderTimeDelta;

	Parameters->FadeTime = ViewData->Scale;
	Parameters->FadeWeight = ViewData->Weight;

	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
}
