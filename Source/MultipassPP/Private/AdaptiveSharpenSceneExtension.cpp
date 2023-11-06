// Fill out your copyright notice in the Description page of Project Settings.


#include "AdaptiveSharpenSceneExtension.h"

#include "AdaptiveSharpenBlendable.h"
#include "CommonRenderResources.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"
#include "ScenePrivate.h"
#include "Engine/TextureRenderTarget2D.h"

static TAutoConsoleVariable<int32> CVarAdaptiveSharpeningEnabled(
	TEXT("r.AdaptiveSharpening.Enabled"),
	-1,
	TEXT(""),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAdaptiveSharpeningStrength(
	TEXT("r.AdaptiveSharpening.Strength"),
	-1.f,
	TEXT(""),
	ECVF_Default);

IMPLEMENT_GLOBAL_SHADER(FAdaptiveSharpenPixelShaderPass1, "/MultipassPP/Private/AdaptiveSharpening.usf", "Pass1PS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FAdaptiveSharpenPixelShaderPass2, "/MultipassPP/Private/AdaptiveSharpeningPass2.usf", "Pass2PS", SF_Pixel);

FAdaptiveSharpenSceneExtension::FAdaptiveSharpenSceneExtension(const FAutoRegister& AutoReg)
	: FMultipassPPSceneExtension(AutoReg)
{
	PostProcessingPassName = "Adaptive Sharpen";
	PostProcessingPasses = { EPostProcessingPass::FXAA };
}

void FAdaptiveSharpenSceneExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	FMultipassPPSceneExtension::SetupView(InViewFamily, InView);

	TSharedPtr<FAdaptiveSharpenViewData> ViewData = StaticCastSharedPtr<FAdaptiveSharpenViewData>(GetViewData(InView));
	if (ViewData != nullptr)
	{
		ViewData->Strength = 0.f;
		ViewData->BlendableWeight = 0.f;

		if (CVarAdaptiveSharpeningEnabled.GetValueOnAnyThread() >= 0)
		{
			ViewData->BlendableWeight = FMath::Clamp(CVarAdaptiveSharpeningEnabled.GetValueOnAnyThread(), 0, 1);
		}
		else
		{
			const FFinalPostProcessSettings& Dest = InView.FinalPostProcessSettings;
			FBlendableEntry* BlendableIt = nullptr;
			int32 NumEntries = 0;
			while (FAdaptiveSharpenNode* DataPtr = Dest.BlendableManager.IterateBlendables<FAdaptiveSharpenNode>(BlendableIt))
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

		if (CVarAdaptiveSharpeningStrength.GetValueOnAnyThread() >= 0.f)
		{
			ViewData->Strength = FMath::Max(CVarAdaptiveSharpeningStrength.GetValueOnAnyThread(), 0);
		}
		else
		{
			const FFinalPostProcessSettings& Dest = InView.FinalPostProcessSettings;
			FBlendableEntry* BlendableIt = nullptr;
			int32 NumEntries = 0;
			while (FAdaptiveSharpenNode* DataPtr = Dest.BlendableManager.IterateBlendables<FAdaptiveSharpenNode>(BlendableIt))
			{
				if (DataPtr)
				{
					ViewData->Strength += DataPtr->Strength;
					NumEntries++;
				}
			}

			if (NumEntries > 0)
			{
				ViewData->Strength /= NumEntries;
			}
		}
	}
}

bool FAdaptiveSharpenSceneExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	check(IsInGameThread());

	bool bIsActive = true;
	bool bStrengthActive = true;

	if (CVarAdaptiveSharpeningEnabled.GetValueOnGameThread() > -1)
	{
		bIsActive = CVarAdaptiveSharpeningEnabled.GetValueOnGameThread() > 0;
	}

	if (CVarAdaptiveSharpeningStrength.GetValueOnGameThread() >= 0.f)
	{
		bStrengthActive = CVarAdaptiveSharpeningStrength.GetValueOnGameThread() > 0.f;
	}

	return FMultipassPPSceneExtension::IsActiveThisFrame_Internal(Context) && bIsActive && bStrengthActive;
}

FScreenPassTexture FAdaptiveSharpenSceneExtension::PostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs, EPostProcessingPass Pass)
{
	const FScreenPassTexture& SceneColor = InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor);
	check(SceneColor.IsValid());
	checkSlow(View.bIsViewInfo);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	InOutInputs.Validate();

	TSharedPtr<FAdaptiveSharpenViewData> ViewData = StaticCastSharedPtr<FAdaptiveSharpenViewData>(GetViewData(View));
	if (ViewData != nullptr && ViewData->Strength > 0 && ViewData->BlendableWeight > 0)
	{
		// Pass 1: Scene color -> RT
		FRDGTextureRef RTTexture = GraphBuilder.RegisterExternalTexture(ViewData->GetRT());
		FScreenPassRenderTarget RT = FScreenPassRenderTarget(RTTexture, ViewInfo.ViewRect, ERenderTargetLoadAction::ELoad);

		DrawPass(1, GraphBuilder, View, ViewInfo, SceneColor, RT);

		if (InOutInputs.OverrideOutput.IsValid())
		{
			// Pass 2: RT -> Output
			DrawPass(2, GraphBuilder, View, ViewInfo, RT, InOutInputs.OverrideOutput);

			return InOutInputs.OverrideOutput;
		}
		else
		{
			// Pass 2: RT -> RT
			DrawPass(2, GraphBuilder, View, ViewInfo, RT, RT);

			return MoveTemp(RT);
		}
	}

	return ReturnUntouchedSceneColorForPostProcessing(GraphBuilder, View, ViewInfo, InOutInputs);
}

void FAdaptiveSharpenSceneExtension::SetupPass1Parameters(FRDGBuilder& GraphBuilder, const FSceneView& View, const FViewInfo& ViewInfo, const FScreenPassTexture& Input, const FScreenPassRenderTarget& Output, FAdaptiveSharpenPixelShaderPass1::FParameters* Parameters)
{
	Parameters->InputTexture = Input.Texture;
	Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
	Parameters->PixelUVSize.X = 1.f / Input.Texture->Desc.Extent.X;
	Parameters->PixelUVSize.Y = 1.f / Input.Texture->Desc.Extent.Y;
}

void FAdaptiveSharpenSceneExtension::SetupPass2Parameters(FRDGBuilder& GraphBuilder, const FSceneView& View, const FViewInfo& ViewInfo, const FScreenPassTexture& Input, const FScreenPassRenderTarget& Output, FAdaptiveSharpenPixelShaderPass2::FParameters* Parameters)
{
	TSharedPtr<FAdaptiveSharpenViewData> ViewData = StaticCastSharedPtr<FAdaptiveSharpenViewData>(GetViewData(View));

	Parameters->InputTexture = Input.Texture;
	Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
	Parameters->PixelUVSize.X = 1.f / Input.Texture->Desc.Extent.X;
	Parameters->PixelUVSize.Y = 1.f / Input.Texture->Desc.Extent.Y;
	Parameters->CurveHeight = FMath::Clamp(ViewData->BlendableWeight, 0.f, 1.f) * ViewData->Strength;
}

void FAdaptiveSharpenSceneExtension::DrawPass(int32 PassNum, FRDGBuilder& GraphBuilder, const FSceneView& View, const FViewInfo& ViewInfo, const FScreenPassTexture& Input, const FScreenPassRenderTarget& Output)
{
	check(PassNum == 1 || PassNum == 2);

	check(IsInRenderingThread());

	const FScreenPassTextureViewport InputViewport(Input);
	const FScreenPassTextureViewport OutputViewport(Output);

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.FeatureLevel);
	check(GlobalShaderMap);

	TShaderMapRef<FScreenPassVS> VertexShader(ViewInfo.ShaderMap);

	FRDGEventName PassName = FRDGEventName(TEXT("%s Pass %d"), *PostProcessingPassName, PassNum);

	if (PassNum == 1)
	{
		TShaderMapRef<FAdaptiveSharpenPixelShaderPass1> PixelShader(ViewInfo.ShaderMap);
		check(PixelShader.IsValid());

		FAdaptiveSharpenPixelShaderPass1::FParameters* Parameters = GraphBuilder.AllocParameters<FAdaptiveSharpenPixelShaderPass1::FParameters>();
		SetupPass1Parameters(GraphBuilder, View, ViewInfo, Input, Output, Parameters);

		AddDrawScreenPass(GraphBuilder, Forward<FRDGEventName&&>(PassName), ViewInfo, OutputViewport, InputViewport, VertexShader, PixelShader, BlendState, DepthStencilState, Parameters, EScreenPassDrawFlags::None);
	}
	else if (PassNum == 2)
	{
		TShaderMapRef<FAdaptiveSharpenPixelShaderPass2> PixelShader(ViewInfo.ShaderMap);
		check(PixelShader.IsValid());

		FAdaptiveSharpenPixelShaderPass2::FParameters* Parameters = GraphBuilder.AllocParameters<FAdaptiveSharpenPixelShaderPass2::FParameters>();
		SetupPass2Parameters(GraphBuilder, View, ViewInfo, Input, Output, Parameters);

		AddDrawScreenPass(GraphBuilder, Forward<FRDGEventName&&>(PassName), ViewInfo, OutputViewport, InputViewport, VertexShader, PixelShader, BlendState, DepthStencilState, Parameters, EScreenPassDrawFlags::None);
	}
}


size_t FAdaptiveSharpenSceneExtension::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}