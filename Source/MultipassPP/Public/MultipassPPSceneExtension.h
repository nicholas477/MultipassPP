#pragma once

#include "SceneViewExtension.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Containers/ContainersFwd.h"
#include "Templates/SharedPointer.h"
#include "ShaderParameters.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "SceneRendering.h"
#endif

struct IPooledRenderTarget;

struct MULTIPASSPP_API IMultipassPPViewData : public TSharedFromThis<IMultipassPPViewData, ESPMode::ThreadSafe>
{
	virtual TRefCountPtr<IPooledRenderTarget> GetRT() { return nullptr; };

	// Called on SetupView by the scene extension
	virtual void SetupRT(const FIntPoint& Resolution) {};

	virtual ~IMultipassPPViewData() {};
};

// Default view data implementation. Just holds the RT
struct MULTIPASSPP_API FMultipassPPViewData : public IMultipassPPViewData
{
	virtual TRefCountPtr<IPooledRenderTarget> GetRT() override { return RT; };
	virtual void SetupRT(const FIntPoint& Resolution) override;

	TRefCountPtr<IPooledRenderTarget> RT;
	FString RTDebugName = "Multipass PP View Data RT";
	EPixelFormat RTPixelFormat = EPixelFormat::PF_B8G8R8A8; // Equivalent to ETextureRenderTargetFormat::RTF_RGBA8_SRGB
};

class MULTIPASSPP_API FMultipassPPSceneExtension : public FSceneViewExtensionBase
{
public:
	FMultipassPPSceneExtension(const FAutoRegister& AutoReg);

	// Begin ISceneViewExtension interface
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void SetupViewFamily(FSceneViewFamily&) override {}; // = 0
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) {}; // = 0
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
	// End ISceneViewExtension interface

	// Just returns the inputted scene color as the output screen pass texture. Use this if you don't want to do any post processing
	FScreenPassTexture ReturnUntouchedSceneColorForPostProcessing(FRDGBuilder& GraphBuilder, const FSceneView& View, const FViewInfo& ViewInfo, const FPostProcessMaterialInputs& InOutInputs) const;

	// Looks up the ViewData in ViewDataMap. May return nullptr
	virtual TSharedPtr<IMultipassPPViewData> GetViewData(const FSceneView& InView);

	// Same as GetViewData, but calls ConstructViewData if the ViewData does not exist
	virtual TSharedPtr<IMultipassPPViewData> GetOrCreateViewData(const FSceneView& InView);

	virtual size_t GetTypeHash() const;

protected:
	// Which PP passes to bind to. Defaults to the tonemapping pass
	TSet<EPostProcessingPass> PostProcessingPasses;

	// The pass name that shows up in ProfileGPU
	FString PostProcessingPassName = "MultipassPP";
	
	// Called by SubscribeToPostProcessingPass. Calls AddPass_RenderThread
	virtual FScreenPassTexture PostProcessPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& InOutInputs,
		EPostProcessingPass Pass
	);

	// Derived classes should call AddDrawScreenPass in this function
	virtual void AddPass_RenderThread(
		class FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FViewInfo& ViewInfo,
		const struct FScreenPassTexture& Input,
		const struct FScreenPassRenderTarget& Output
	) {};

	// Map of ViewState index to ViewData
	// Each view should have a ViewData associated to it
	TMap<uint32, TSharedPtr<IMultipassPPViewData>> ViewDataMap;

	// Just constructs the view data. Called in GetOrCreateViewData if the viewdata is null. Override this function and return your custom viewdata type here
	virtual TSharedPtr<IMultipassPPViewData> ConstructViewData(const FSceneView& InView);
};

// Same as FMultipassPPSceneExtension, but has a default implementation for AddPass_RenderThread that handles shader setup
template<typename TDerivedType, typename TShaderType, typename TParametersType = TShaderType::FParameters>
class MULTIPASSPP_API FMultipassPPSceneExtensionWithShader : public FMultipassPPSceneExtension
{
public:
	FMultipassPPSceneExtensionWithShader(const FAutoRegister& AutoReg)
		: FMultipassPPSceneExtension(AutoReg)
	{

	}

	using BaseT = FMultipassPPSceneExtensionWithShader<TDerivedType, TShaderType, TParametersType>;

protected:
	FRHIBlendState* BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
	FRHIDepthStencilState* DepthStencilState = FScreenPassPipelineState::FDefaultDepthStencilState::GetRHI();

	virtual void AddPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FViewInfo& ViewInfo, const FScreenPassTexture& Input, const FScreenPassRenderTarget& Output) override
	{
		check(IsInRenderingThread());

		const FScreenPassTextureViewport InputViewport(Input);
		const FScreenPassTextureViewport OutputViewport(Output);

		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.FeatureLevel);
		check(GlobalShaderMap);

		TShaderMapRef<TShaderType> PixelShader(ViewInfo.ShaderMap);
		check(PixelShader.IsValid());

		TParametersType* Parameters = GraphBuilder.AllocParameters<TParametersType>();
		static_cast<TDerivedType*>(this)->SetupParameters(GraphBuilder, View, ViewInfo, Input, Output, Parameters);

		FRDGEventName PassName = FRDGEventName(TEXT("%s"), *PostProcessingPassName);

		TShaderMapRef<FScreenPassVS> VertexShader(ViewInfo.ShaderMap);

		AddDrawScreenPass(GraphBuilder, Forward<FRDGEventName&&>(PassName), ViewInfo, OutputViewport, InputViewport, VertexShader, PixelShader, BlendState, DepthStencilState, Parameters, EScreenPassDrawFlags::None);
	}

	virtual void SetupParameters(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FViewInfo& ViewInfo,
		const FScreenPassTexture& Input,
		const FScreenPassRenderTarget& Output,
		TParametersType* Parameters)
	{
		//Parameters->InputTexture = Input.Texture;
		//Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
		//Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
	}
};

// This is just an example of a pixel shader that can be used with this
// 
//class FMultipassPPPixelShader : public FGlobalShader
//{
//public:
//	DECLARE_SHADER_TYPE(FMultipassPPPixelShader, Global);
//	SHADER_USE_PARAMETER_STRUCT(FMultipassPPPixelShader, FGlobalShader);
//
//	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
//	{
//		return true;
//	}
//
//	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
//		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
//		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
//		RENDER_TARGET_BINDING_SLOTS()
//	END_SHADER_PARAMETER_STRUCT()
//};
