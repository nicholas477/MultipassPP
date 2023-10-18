#pragma once

#include "MultipassPPSceneExtension.h"


class MULTIPASSPP_API FAccumulationMotionBlurPixelShader : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FAccumulationMotionBlurPixelShader, Global);
	SHADER_USE_PARAMETER_STRUCT(FAccumulationMotionBlurPixelShader, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MotionBlurTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, MotionBlurSampler)
		SHADER_PARAMETER(uint32, LastFrameNumber)
		SHADER_PARAMETER(FIntPoint, InputTextureSize)
		SHADER_PARAMETER(FIntPoint, OutputTextureSize)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, FadeTime)
		SHADER_PARAMETER(float, FadeWeight)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

struct MULTIPASSPP_API FAccumulationMotionBlurViewData : public FMultipassPPViewData
{
	FAccumulationMotionBlurViewData()
	{
		RTDebugName = "AccumulationMotionBlur_RT";
	}
	uint32 LastFrameNumber = 0;
	float Scale = 0.f;
	float Weight = 0.f;
};

class MULTIPASSPP_API FAccumulationMotionBlurSceneExtension
	: public FMultipassPPSceneExtensionWithShader<FAccumulationMotionBlurSceneExtension, FAccumulationMotionBlurPixelShader>
{
public:
	FAccumulationMotionBlurSceneExtension(const FAutoRegister& AutoReg);

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

protected:
	virtual FScreenPassTexture PostProcessPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& InOutInputs,
		EPostProcessingPass Pass
	) override;

	virtual void SetupParameters(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FViewInfo& ViewInfo,
		const FScreenPassTexture& Input,
		const FScreenPassRenderTarget& Output,
		FAccumulationMotionBlurPixelShader::FParameters* Parameters
	);

	virtual TSharedPtr<IMultipassPPViewData> ConstructViewData(const FSceneView& InView) override
	{
		return MakeShared<FAccumulationMotionBlurViewData>();
	}

	friend class FMultipassPPSceneExtensionWithShader<FAccumulationMotionBlurSceneExtension, FAccumulationMotionBlurPixelShader>;
};