// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FInterlacePPSceneExtension;
class FAccumulationMotionBlurSceneExtension;

class FMultipassPPModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr<class FInterlacePPSceneExtension> GetInterlaceSceneExtension();
	TSharedPtr<class FAccumulationMotionBlurSceneExtension> GetAccumulationMotionBlurSceneExtension();
	TSharedPtr<class FAdaptiveSharpenSceneExtension> GetSharpenSceneExtension();

protected:
	TSharedPtr<class FInterlacePPSceneExtension> InterlaceSceneExtension;
	TSharedPtr<class FAccumulationMotionBlurSceneExtension> MotionBlurSceneExtension;
	TSharedPtr<class FAdaptiveSharpenSceneExtension> SharpenSceneExtension;
};
