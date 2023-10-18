// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipassPPSceneExtension.h"

#include "SceneView.h"
#include "ScreenPass.h"
#include "CommonRenderResources.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"
#include "ScenePrivate.h"
#include "Engine/TextureRenderTarget2D.h"

FMultipassPPSceneExtension::FMultipassPPSceneExtension(const FAutoRegister& AutoReg)
	: FSceneViewExtensionBase(AutoReg)
{
	PostProcessingPasses = { EPostProcessingPass::Tonemap };
}

void FMultipassPPSceneExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	TSharedPtr<IMultipassPPViewData> ViewData = GetOrCreateViewData(InView);
	if (ViewData != nullptr)
	{
		ViewData->SetupRT(InView.UnconstrainedViewRect.Size());
	}
}

void FMultipassPPSceneExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (PostProcessingPasses.Contains(Pass))
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FMultipassPPSceneExtension::PostProcessPass_RenderThread, Pass));
	}
}

FScreenPassTexture FMultipassPPSceneExtension::PostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs, EPostProcessingPass Pass)
{
	const FScreenPassTexture& SceneColor = InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor);
	check(SceneColor.IsValid());
	checkSlow(View.bIsViewInfo);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	InOutInputs.Validate();

	TSharedPtr<IMultipassPPViewData> ViewData = GetViewData(View);
	if (ViewData != nullptr)
	{
		FRDGTextureRef OutputTexture = GraphBuilder.RegisterExternalTexture(ViewData->GetRT());
		FScreenPassRenderTarget Output = FScreenPassRenderTarget(OutputTexture, ViewInfo.ViewRect, ERenderTargetLoadAction::ELoad);

		AddPass_RenderThread(
			GraphBuilder,
			View,
			ViewInfo,
			SceneColor,
			Output
		);

		return MoveTemp(Output);
	}
	
	return ReturnUntouchedSceneColorForPostProcessing(GraphBuilder, View, ViewInfo, InOutInputs);
}

FScreenPassTexture FMultipassPPSceneExtension::ReturnUntouchedSceneColorForPostProcessing(FRDGBuilder& GraphBuilder, const FSceneView& View, const FViewInfo& ViewInfo, const FPostProcessMaterialInputs& InOutInputs) const
{
	// If OverrideOutput is valid, we need to write to it, even if we're bypassing pp rendering
	if (InOutInputs.OverrideOutput.IsValid())
	{
		FCopyRectPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCopyRectPS::FParameters>();
		Parameters->InputTexture = InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor).Texture;
		Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
		Parameters->RenderTargets[0] = InOutInputs.OverrideOutput.GetRenderTargetBinding();

		const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.FeatureLevel);

		TShaderMapRef<FCopyRectPS> CopyPixelShader(GlobalShaderMap);
		TShaderMapRef<FScreenPassVS> ScreenPassVS(GlobalShaderMap);

		const FScreenPassTextureViewport InputViewport(InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor));
		const FScreenPassTextureViewport OutputViewport(InOutInputs.OverrideOutput);

		FRHIBlendState* CopyBlendState = FScreenPassPipelineState::FDefaultBlendState::GetRHI();
		FRHIDepthStencilState* DepthStencilState = FScreenPassPipelineState::FDefaultDepthStencilState::GetRHI();
		AddDrawScreenPass(GraphBuilder, FRDGEventName(TEXT("ReturnUntouchedSceneColorForPostProcessing")), ViewInfo, OutputViewport, InputViewport, ScreenPassVS, CopyPixelShader, CopyBlendState, DepthStencilState, Parameters, EScreenPassDrawFlags::None);

		return InOutInputs.OverrideOutput;
	}
	else
	{
		/** We don't want to modify scene texture in any way. We just want it to be passed back onto the next stage. */
		FScreenPassTexture SceneTexture = const_cast<FScreenPassTexture&>(InOutInputs.Textures[(uint32)EPostProcessMaterialInput::SceneColor]);
		return SceneTexture;
	}
}

void FMultipassPPViewData::SetupRT(const FIntPoint& Resolution)
{
	if (Resolution.X <= 0 || Resolution.Y <= 0)
	{
		return;
	}

	bool bCreateRT = false;
	if (!RT.IsValid())
	{
		bCreateRT = true;
	}
	else
	{
		FIntVector TexSize = RT->GetDesc().GetSize();
		if (TexSize.X != Resolution.X || TexSize.Y != Resolution.Y)
		{
			bCreateRT = true;
		}
	}

	if (bCreateRT)
	{
		if (Resolution.X > 0 && Resolution.Y > 0)
		{
			if (IsInRenderingThread())
			{
				const FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
					Resolution,
					GetPixelFormatFromRenderTargetFormat(ETextureRenderTargetFormat::RTF_RGBA8_SRGB),
					FClearValueBinding::None,
					TexCreate_None,
					TexCreate_ShaderResource | TexCreate_RenderTargetable | ETextureCreateFlags::UAV,
					false);

				GRenderTargetPool.FindFreeElement(GetImmediateCommandList_ForRenderCommand(), Desc, RT, *RTDebugName);
			}
			else
			{
				ENQUEUE_RENDER_COMMAND(FlushRHIThreadToUpdateTextureRenderTargetReference)(
				[SharedThis = SharedThis(this), Resolution](FRHICommandListImmediate& RHICmdList)
				{
					const FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
						Resolution,
						GetPixelFormatFromRenderTargetFormat(ETextureRenderTargetFormat::RTF_RGBA8_SRGB),
						FClearValueBinding::None,
						TexCreate_None,
						TexCreate_ShaderResource | TexCreate_RenderTargetable | ETextureCreateFlags::UAV,
						false);

					GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SharedThis->RT, *SharedThis->RTDebugName);
				});
			}
		}
	}
}

TSharedPtr<IMultipassPPViewData> FMultipassPPSceneExtension::GetViewData(const FSceneView& InView)
{
	if (InView.State == nullptr)
	{
		return nullptr;
	}

	const uint32 Index = InView.State->GetViewKey();
	TSharedPtr<IMultipassPPViewData>* FoundData = ViewDataMap.Find(Index);
	return FoundData ? *FoundData : nullptr;
}

TSharedPtr<IMultipassPPViewData> FMultipassPPSceneExtension::GetOrCreateViewData(const FSceneView& InView)
{
	if (InView.State == nullptr)
	{
		return nullptr;
	}

	const uint32 Index = InView.State->GetViewKey();
	TSharedPtr<IMultipassPPViewData>& ViewData = ViewDataMap.FindOrAdd(Index);
	if (!ViewData.IsValid())
	{
		ViewData = ConstructViewData(InView);
	}
	return ViewData;
}

TSharedPtr<IMultipassPPViewData> FMultipassPPSceneExtension::ConstructViewData(const FSceneView& InView)
{
	return MakeShared<FMultipassPPViewData>();
}

size_t FMultipassPPSceneExtension::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

