// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MultipassPPSceneExtension.h"

class MULTIPASSPP_API FAdaptiveSharpenPixelShaderPass1 : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FAdaptiveSharpenPixelShaderPass1, Global);
	SHADER_USE_PARAMETER_STRUCT(FAdaptiveSharpenPixelShaderPass1, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER(FVector2f, PixelUVSize)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

class MULTIPASSPP_API FAdaptiveSharpenPixelShaderPass2 : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FAdaptiveSharpenPixelShaderPass2, Global);
	SHADER_USE_PARAMETER_STRUCT(FAdaptiveSharpenPixelShaderPass2, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER(FVector2f, PixelUVSize)
		SHADER_PARAMETER(float, CurveHeight)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

struct MULTIPASSPP_API FAdaptiveSharpenViewData : public FMultipassPPViewData
{
	FAdaptiveSharpenViewData()
	{
		RTDebugName = "AdaptiveSharpen_RT";
		RTPixelFormat = ETextureRenderTargetFormat::RTF_RGBA32f;
		RTClearValueBinding = FClearValueBinding::Transparent;
	}

	float BlendableWeight = 0.f;
	float Strength = 1.f;
};


/**
 * 
 */
class MULTIPASSPP_API FAdaptiveSharpenSceneExtension
	: public FMultipassPPSceneExtension
{
public:
	FAdaptiveSharpenSceneExtension(const FAutoRegister& AutoReg);

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

	virtual FScreenPassTexture PostProcessPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& InOutInputs,
		EPostProcessingPass Pass
	) override;

	void SetupPass1Parameters(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FViewInfo& ViewInfo,
		const FScreenPassTexture& Input,
		const FScreenPassRenderTarget& Output,
		FAdaptiveSharpenPixelShaderPass1::FParameters* Parameters);

	void SetupPass2Parameters(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FViewInfo& ViewInfo,
		const FScreenPassTexture& Input,
		const FScreenPassRenderTarget& Output,
		FAdaptiveSharpenPixelShaderPass2::FParameters* Parameters);

	// PassNum is either 1 or 2
	void DrawPass(int32 PassNum, FRDGBuilder& GraphBuilder, const FSceneView& View, const FViewInfo& ViewInfo, const FScreenPassTexture& Input, const FScreenPassRenderTarget& Output);

	virtual size_t GetTypeHash() const override;

protected:
	FRHIBlendState* BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
	FRHIDepthStencilState* DepthStencilState = FScreenPassPipelineState::FDefaultDepthStencilState::GetRHI();

	virtual TSharedPtr<IMultipassPPViewData> ConstructViewData(const FSceneView& InView) { return MakeShared<FAdaptiveSharpenViewData>(); };
};
