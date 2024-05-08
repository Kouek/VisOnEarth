// Author : Kouek Kou

#include "DVRRenderer.h"

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "EngineModule.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

#include "Runtime/Renderer/Private/SceneRendering.h"

#include "Util.h"

class VIS4EARTH_API FDVRShader : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FDVRShader);
    SHADER_USE_PARAMETER_STRUCT(FDVRShader, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, VIS4EARTH_API)
    SHADER_PARAMETER(int, MaxStepCnt)
    SHADER_PARAMETER(float, Step)
    SHADER_PARAMETER(float, RelativeLightness)
    SHADER_PARAMETER(FVector2f, RenderSize)
    SHADER_PARAMETER(FVector2f, LonRng)
    SHADER_PARAMETER(FVector2f, LatRng)
    SHADER_PARAMETER(FVector2f, HeightRng)
    SHADER_PARAMETER(FMatrix44f, EyeToEarth)
    SHADER_PARAMETER(FMatrix44f, InvProj)
    SHADER_PARAMETER_SAMPLER(SamplerState, VolSamplerState)
    SHADER_PARAMETER_SAMPLER(SamplerState, TFSamplerState)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, VolInput)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, TFInput)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, DepthInput)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, ColorOutput)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters &Parameters) {
        return true;
    }
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters &Parameters,
                                             FShaderCompilerEnvironment &OutEnvironment) {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("THREAD_PER_GROUP_X"), VIS4EARTH_THREAD_PER_GROUP_X);
        OutEnvironment.SetDefine(TEXT("THREAD_PER_GROUP_Y"), VIS4EARTH_THREAD_PER_GROUP_Y);
        OutEnvironment.SetDefine(TEXT("THREAD_PER_GROUP_Z"), 1);
    }
};

IMPLEMENT_GLOBAL_SHADER(FDVRShader, "/VIS4Earth/DVR.usf", "DVR", SF_Compute);

class VIS4EARTH_API FPreIntDVRShader : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FPreIntDVRShader);
    SHADER_USE_PARAMETER_STRUCT(FPreIntDVRShader, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, VIS4EARTH_API)
    SHADER_PARAMETER(int, MaxStepCnt)
    SHADER_PARAMETER(float, Step)
    SHADER_PARAMETER(float, RelativeLightness)
    SHADER_PARAMETER(FVector2f, RenderSize)
    SHADER_PARAMETER(FVector2f, LonRng)
    SHADER_PARAMETER(FVector2f, LatRng)
    SHADER_PARAMETER(FVector2f, HeightRng)
    SHADER_PARAMETER(FMatrix44f, EyeToEarth)
    SHADER_PARAMETER(FMatrix44f, InvProj)
    SHADER_PARAMETER_SAMPLER(SamplerState, VolSamplerState)
    SHADER_PARAMETER_SAMPLER(SamplerState, TFSamplerState)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, VolInput)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, TFInput)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, DepthInput)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, ColorOutput)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters &Parameters) {
        return true;
    }
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters &Parameters,
                                             FShaderCompilerEnvironment &OutEnvironment) {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("THREAD_PER_GROUP_X"), VIS4EARTH_THREAD_PER_GROUP_X);
        OutEnvironment.SetDefine(TEXT("THREAD_PER_GROUP_Y"), VIS4EARTH_THREAD_PER_GROUP_Y);
        OutEnvironment.SetDefine(TEXT("THREAD_PER_GROUP_Z"), 1);
    }
};

IMPLEMENT_GLOBAL_SHADER(FPreIntDVRShader, "/VIS4Earth/DVR.usf", "PreIntDVR", SF_Compute);

void FDVRRenderer::Register() {
    Unregister();

    ENQUEUE_RENDER_COMMAND(RegisterRenderScreenRenderer)
    ([renderer = SharedThis(this)](FRHICommandListImmediate &RHICmdList) {
        renderer->onPostOpaqueRender = GetRendererModule().RegisterPostOpaqueRenderDelegate(
            FPostOpaqueRenderDelegate::CreateRaw(&renderer.Get(), &FDVRRenderer::render));
    });
}

void FDVRRenderer::Unregister() {
    if (!onPostOpaqueRender.IsValid())
        return;

    ENQUEUE_RENDER_COMMAND(UnregisterRenderScreenRenderer)
    ([renderer = SharedThis(this)](FRHICommandListImmediate &RHICmdList) {
        GetRendererModule().RemovePostOpaqueRenderDelegate(renderer->onPostOpaqueRender);
        renderer->onPostOpaqueRender.Reset();
    });
}

void FDVRRenderer::render(FPostOpaqueRenderParameters &PostQpqRndrParams) {
    if (!rndrParams.VolumeTexture.IsValid() || !rndrParams.TransferFunctionTexture.IsValid() ||
        !geoParams.GeoRef.IsValid())
        return;
    if (!rndrParams.VolumeTexture->GetResource() ||
        !rndrParams.TransferFunctionTexture->GetResource())
        return;

    if (rndrParams.UsePreIntegratedTF)
        render<FPreIntDVRShader>(PostQpqRndrParams);
    else
        render<FDVRShader>(PostQpqRndrParams);
}

template <typename ShaderTy>
void FDVRRenderer::render(FPostOpaqueRenderParameters &PostQpqRndrParams) {
    using ShaderParamsType = ShaderTy::FParameters;

    auto &grphBldr = *PostQpqRndrParams.GraphBuilder;

    auto rndrSz = FIntVector2(PostQpqRndrParams.ViewportRect.Width(),
                              PostQpqRndrParams.ViewportRect.Height());

    TShaderMapRef<ShaderTy> shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
    auto shaderParams = grphBldr.AllocParameters<ShaderParamsType>();
    {
        shaderParams->MaxStepCnt = rndrParams.MaxStepCount;
        shaderParams->Step = rndrParams.Step;
        shaderParams->RelativeLightness = rndrParams.RelativeLightness;

        shaderParams->RenderSize.X = rndrSz.X;
        shaderParams->RenderSize.Y = rndrSz.Y;

        shaderParams->LonRng = FVector2f(FMath::DegreesToRadians(geoParams.LongtitudeRange));
        shaderParams->LatRng = FVector2f(FMath::DegreesToRadians(geoParams.LatitudeRange));
        shaderParams->HeightRng = FVector2f(geoParams.HeightRange);

        auto eyeToWorld = PostQpqRndrParams.View->ViewMatrices.GetInvViewMatrix();
        shaderParams->EyeToEarth.SetIdentity();
        shaderParams->EyeToEarth.SetOrigin(
            FVector3f(geoParams.GeoRef->TransformUnrealPositionToEarthCenteredEarthFixed(
                eyeToWorld.GetOrigin())));
        for (int32 i = 0; i < 3; ++i) {
            auto axis = geoParams.GeoRef->TransformUnrealDirectionToEarthCenteredEarthFixed(
                eyeToWorld.GetScaledAxis(i == 0   ? EAxis::X
                                         : i == 1 ? EAxis::Y
                                                  : EAxis::Z));
            shaderParams->EyeToEarth.SetAxis(i, FVector3f(axis));
        }

        shaderParams->InvProj =
            FMatrix44f(PostQpqRndrParams.View->ViewMatrices.GetInvProjectionMatrix());

        shaderParams->VolSamplerState =
            TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        shaderParams->TFSamplerState =
            TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

        auto extrnlTexRDG = RegisterExternalTexture(
            grphBldr, rndrParams.VolumeTexture->GetResource()->GetTexture3DRHI(),
            TEXT("Volume Texture") TEXT(" in ") TEXT(__FUNCTION__));
        shaderParams->VolInput = grphBldr.CreateSRV(FRDGTextureSRVDesc(extrnlTexRDG));
        extrnlTexRDG = RegisterExternalTexture(
            grphBldr, rndrParams.TransferFunctionTexture->GetResource()->GetTexture2DRHI(),
            TEXT("TF Texture") TEXT(" in ") TEXT(__FUNCTION__));
        shaderParams->TFInput = grphBldr.CreateSRV(FRDGTextureSRVDesc(extrnlTexRDG));

        shaderParams->DepthInput =
            grphBldr.CreateSRV(FRDGTextureSRVDesc(PostQpqRndrParams.DepthTexture));
        shaderParams->ColorOutput =
            grphBldr.CreateUAV(FRDGTextureUAVDesc(PostQpqRndrParams.ColorTexture));
    }

    FComputeShaderUtils::AddPass(
        grphBldr, RDG_EVENT_NAME("Direct Volume Rendering"),
        ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, shader, shaderParams,
        FIntVector(FMath::DivideAndRoundUp(rndrSz.X, VIS4EARTH_THREAD_PER_GROUP_X),
                   FMath::DivideAndRoundUp(rndrSz.Y, VIS4EARTH_THREAD_PER_GROUP_Y), 1));
}

template void FDVRRenderer::render<FDVRShader>(FPostOpaqueRenderParameters &);
template void FDVRRenderer::render<FPreIntDVRShader>(FPostOpaqueRenderParameters &);
