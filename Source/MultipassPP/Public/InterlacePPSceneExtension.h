#pragma once

#include "MultipassPPSceneExtension.h"

class MULTIPASSPP_API FInterlacePPPixelShader : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FInterlacePPPixelShader, Global);
	SHADER_USE_PARAMETER_STRUCT(FInterlacePPPixelShader, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER(float, Time)
		SHADER_PARAMETER(float, Weight)
		SHADER_PARAMETER(uint32, FrameNumber)
		SHADER_PARAMETER(uint32, ResX)
		SHADER_PARAMETER(uint32, ResY)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

struct MULTIPASSPP_API FInterlacePPViewData : public FMultipassPPViewData
{
	FInterlacePPViewData()
	{
		RTDebugName = "InterlacePP_RT";
	}

	float BlendableWeight = 0.f;
	uint32 LastFrameNumber = 0;
	float LastFrameTime = 0.f;
};

class MULTIPASSPP_API FInterlacePPSceneExtension 
	: public FMultipassPPSceneExtensionWithShader<FInterlacePPSceneExtension, FInterlacePPPixelShader>
{
public:
	FInterlacePPSceneExtension(const FAutoRegister& AutoReg);

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;

	virtual int32 GetPriority() const override { return 100; }

	virtual size_t GetTypeHash() const override;

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
		FInterlacePPPixelShader::FParameters* Parameters
	);

	virtual TSharedPtr<IMultipassPPViewData> ConstructViewData(const FSceneView& InView) override;

	friend class FMultipassPPSceneExtensionWithShader<FInterlacePPSceneExtension, FInterlacePPPixelShader, FInterlacePPPixelShader::FParameters>;
};