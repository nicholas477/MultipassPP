# MultipassPP

This plugin implements a framework for writing multipass post processing effects using scene view extensions. It also includes three example post processing effects, a deinterlacing style effect, an accumulation motion blur effect, and a sharpening effect.

If you want to write your own effect using the framework, take a look at [InterlacePPSceneExtension](Source/MultipassPP/Private/InterlacePPSceneExtension.cpp) and [AccumulationMotionBlurSceneExtension](Source/MultipassPP/Private/AccumulationMotionBlurSceneExtension.cpp).

# Controlling the included effects

### Using the console commands

You can control the included effects by using the provided console commands or by using a blendable object.

The console commands are:
```
r.AccumulationMotionBlur.Scale
r.AccumulationMotionBlur.Weight

r.AdaptiveSharpening.Enabled
r.AdaptiveSharpening.Strength

r.InterlacingPP.Enabled
```
The console commands take precedence over the blendables. For example, if the `r.AdaptiveSharpening.Strength` is set to 1 then that overrides any blendables currently applied in the post processing settings.

### Using blendable objects

This way of controlling the post processing effects is a bit harder. You can control the post processing effects by constructing a blendable object and adding/updating it in a post process volume or in a camera's post processing settings. You can see how to do this [here](https://docs.unrealengine.com/4.27/en-US/RenderingAndGraphics/PostProcessEffects/Blendables/#howtocreateyourownblendable_inc++_) in the `How to create your own Blendable (in C++)` section.

The blendable objects you can construct and add are: `AdaptiveSharpenBlendable`, `InterlacePPBlendable`, and `AccumulationMotionBlurBlendable`.
