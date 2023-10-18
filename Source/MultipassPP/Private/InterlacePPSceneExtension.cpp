// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterlacePPSceneExtension.h"

#include "SceneView.h"
#include "ScreenPass.h"
#include "CommonRenderResources.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"
#include "ScenePrivate.h"
#include "Engine/TextureRenderTarget2D.h"
#include "InterlacePPBlendable.h"

static TAutoConsoleVariable<int32> CVarInterlacingEnabled(
	TEXT("r.InterlacingPP.Enabled"),
	-1,
	TEXT(""),
	ECVF_Default);

IMPLEMENT_GLOBAL_SHADER(FInterlacePPPixelShader, "/MultipassPP/Private/InterlacePP.usf", "InterlacePS", SF_Pixel);

FInterlacePPSceneExtension::FInterlacePPSceneExtension(const FAutoRegister& AutoReg)
	: BaseT(AutoReg)
{
	PostProcessingPassName = "InterlacePP";
}

void FInterlacePPSceneExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	BaseT::SetupView(InViewFamily, InView);

	TSharedPtr<FInterlacePPViewData> ViewData = StaticCastSharedPtr<FInterlacePPViewData>(GetViewData(InView));
	if (ViewData != nullptr)
	{
		ViewData->BlendableWeight = 0.f;
		if (CVarInterlacingEnabled.GetValueOnAnyThread() >= 0)
		{
			ViewData->BlendableWeight = FMath::Clamp(CVarInterlacingEnabled.GetValueOnAnyThread(), 0, 1);
		}
		else
		{
			const FFinalPostProcessSettings& Dest = InView.FinalPostProcessSettings;
			FBlendableEntry* BlendableIt = nullptr;
			int32 NumEntries = 0;
			while (FInterlacePPNode* DataPtr = Dest.BlendableManager.IterateBlendables<FInterlacePPNode>(BlendableIt))
			{
				if (DataPtr)
				{
					ViewData->BlendableWeight += BlendableIt->Weight;
					NumEntries++;
				}
			}

			if (NumEntries > 0)
			{
				ViewData->BlendableWeight /= NumEntries;
			}
		}
	}
}

size_t FInterlacePPSceneExtension::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

bool FInterlacePPSceneExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	check(IsInGameThread());

	bool bIsActive = true;

	if (CVarInterlacingEnabled.GetValueOnGameThread() > -1)
	{
		bIsActive = CVarInterlacingEnabled.GetValueOnGameThread() > 0;
	}

	return BaseT::IsActiveThisFrame_Internal(Context) && bIsActive;
}

FScreenPassTexture FInterlacePPSceneExtension::PostProcessPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& InOutInputs,
	EPostProcessingPass Pass
)
{
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);

	TSharedPtr<FInterlacePPViewData> ViewData = StaticCastSharedPtr<FInterlacePPViewData>(GetViewData(View));
	if (!ViewData)
	{
		return ReturnUntouchedSceneColorForPostProcessing(GraphBuilder, View, ViewInfo, InOutInputs);
	}

	if (ViewData->BlendableWeight <= 0.f)
	{
		return ReturnUntouchedSceneColorForPostProcessing(GraphBuilder, View, ViewInfo, InOutInputs);
	}

	return BaseT::PostProcessPass_RenderThread(GraphBuilder, View, InOutInputs, Pass);
}

void FInterlacePPSceneExtension::SetupParameters(
	FRDGBuilder& GraphBuilder, 
	const FSceneView& View,
	const FViewInfo& ViewInfo,
	const FScreenPassTexture& Input,
	const FScreenPassRenderTarget& Output,
	FInterlacePPPixelShader::FParameters* Parameters
)
{
	TSharedPtr<FInterlacePPViewData> ViewData = StaticCastSharedPtr<FInterlacePPViewData>(GetViewData(View)); 
	Parameters->InputTexture = Input.Texture;
	Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
	ViewData->LastFrameTime = ViewInfo.ViewState->LastRenderTime;
	Parameters->Time = ViewData->LastFrameTime;
	Parameters->Weight = ViewData->BlendableWeight;
	Parameters->FrameNumber = ViewData->LastFrameNumber++;
	Parameters->ResX = ViewInfo.ViewRect.Width();
	Parameters->ResY = ViewInfo.ViewRect.Height();
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
}

TSharedPtr<IMultipassPPViewData> FInterlacePPSceneExtension::ConstructViewData(const FSceneView& InView)
{
	return MakeShared<FInterlacePPViewData>();
};