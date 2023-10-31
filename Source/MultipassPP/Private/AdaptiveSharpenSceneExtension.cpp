// Fill out your copyright notice in the Description page of Project Settings.


#include "AdaptiveSharpenSceneExtension.h"

#include "CommonRenderResources.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"
#include "ScenePrivate.h"
#include "Engine/TextureRenderTarget2D.h"

IMPLEMENT_GLOBAL_SHADER(FAdaptiveSharpenPixelShaderPass1, "/MultipassPP/Private/AdaptiveSharpening.usf", "Pass1PS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FAdaptiveSharpenPixelShaderPass2, "/MultipassPP/Private/AdaptiveSharpeningPass2.usf", "Pass2PS", SF_Pixel);

FAdaptiveSharpenSceneExtension::FAdaptiveSharpenSceneExtension(const FAutoRegister& AutoReg)
	: FMultipassPPSceneExtension(AutoReg)
{
	PostProcessingPassName = "Adaptive Sharpen";
}

FScreenPassTexture FAdaptiveSharpenSceneExtension::PostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs, EPostProcessingPass Pass)
{
	const FScreenPassTexture& SceneColor = InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor);
	check(SceneColor.IsValid());
	checkSlow(View.bIsViewInfo);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	InOutInputs.Validate();

	TSharedPtr<IMultipassPPViewData> ViewData = GetViewData(View);
	if (ViewData != nullptr)
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
	Parameters->InputTexture = Input.Texture;
	Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
	Parameters->PixelUVSize.X = 1.f / Input.Texture->Desc.Extent.X;
	Parameters->PixelUVSize.Y = 1.f / Input.Texture->Desc.Extent.Y;
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