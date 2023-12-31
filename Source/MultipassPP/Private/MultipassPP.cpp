// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipassPP.h"

#include "InterlacePPSceneExtension.h"
#include "AccumulationMotionBlurSceneExtension.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FMultipassPPModule"

void FMultipassPPModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("MultipassPP"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/MultipassPP"), PluginShaderDir);

	FCoreDelegates::OnPostEngineInit.AddLambda([this]()
	{	
		InterlaceSceneExtension = FSceneViewExtensions::NewExtension<FInterlacePPSceneExtension>();
		MotionBlurSceneExtension = FSceneViewExtensions::NewExtension<FAccumulationMotionBlurSceneExtension>();
		SharpenSceneExtension = FSceneViewExtensions::NewExtension<FAdaptiveSharpenSceneExtension>();
	});
}

void FMultipassPPModule::ShutdownModule()
{
	MotionBlurSceneExtension.Reset();
	InterlaceSceneExtension.Reset();
	SharpenSceneExtension.Reset();
}

TSharedPtr<FInterlacePPSceneExtension> FMultipassPPModule::GetInterlaceSceneExtension()
{
	return InterlaceSceneExtension;
}

TSharedPtr<FAccumulationMotionBlurSceneExtension> FMultipassPPModule::GetAccumulationMotionBlurSceneExtension()
{
	return MotionBlurSceneExtension;
}

TSharedPtr<class FAdaptiveSharpenSceneExtension> FMultipassPPModule::GetSharpenSceneExtension()
{
	return SharpenSceneExtension;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMultipassPPModule, MultipassPP)