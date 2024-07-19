// Author: Kouek Kou

#pragma once

#include "CoreMinimal.h"
#include "Engine/VolumeTexture.h"
#include "RHIGPUReadback.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

#include "Util.h"

#include "Data.h"

class VIS4EARTH_API FVolumeSmoothShader : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FVolumeSmoothShader);
    SHADER_USE_PARAMETER_STRUCT(FVolumeSmoothShader, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, VIS4EARTH_API)
    SHADER_PARAMETER(int, SmoothTy)
    SHADER_PARAMETER(int, SmoothDim)
    SHADER_PARAMETER(FIntVector3, VolDim)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, VolInput)
    SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, VolOutput)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters &Parameters) {
        return true;
    }
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters &Parameters,
                                             FShaderCompilerEnvironment &OutEnvironment) {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("THREAD_PER_GROUP_X"), VIS4EARTH_THREAD_PER_GROUP_X);
        OutEnvironment.SetDefine(TEXT("THREAD_PER_GROUP_Y"), VIS4EARTH_THREAD_PER_GROUP_Y);
        OutEnvironment.SetDefine(TEXT("THREAD_PER_GROUP_Z"), VIS4EARTH_THREAD_PER_GROUP_Z);
    }
};

IMPLEMENT_GLOBAL_SHADER(FVolumeSmoothShader, "/VIS4Earth/VolumeSmoother.usf", "Smooth", SF_Compute);

class VIS4EARTH_API FVolumeSmoother {
  public:
    struct Parameters {
        EVolumeSmoothType SmoothType;
        EVolumeSmoothDimension SmoothDimension;
        TObjectPtr<UVolumeTexture> VolumeTexture;
        TFunction<void(TSharedPtr<TArray<float>> VolDat)> FinishedCallback;
    };
    static void Exec(const Parameters &Params) {
        if (IsInRenderingThread()) {
            exec(GetImmediateCommandList_ForRenderCommand(), Params);
            return;
        }

        ENQUEUE_RENDER_COMMAND(VolumeSmootherExec)
        ([Params](FRHICommandListImmediate &RHICmdList) { exec(RHICmdList, Params); });
    }

  private:
    static void exec(FRHICommandListImmediate &RHICmdList, const Parameters &Params) {
        if (!Params.VolumeTexture)
            return;

        FRDGBuilder grphBldr(RHICmdList);

        auto shaderParams = grphBldr.AllocParameters<FVolumeSmoothShader::FParameters>();
        FUint32Vector3 volDim(Params.VolumeTexture->GetSizeX(), Params.VolumeTexture->GetSizeY(),
                              Params.VolumeTexture->GetSizeZ());
        auto smoothedVolBuf = grphBldr.CreateBuffer(
            FRDGBufferDesc::CreateBufferDesc(sizeof(float), volDim.X * volDim.Y * volDim.Z),
            *VIS4EARTH_GET_NAME_IN_FUNCTION("Smoothed Volume Buffer"));
        {
            shaderParams->SmoothTy = static_cast<int>(Params.SmoothType);
            shaderParams->SmoothDim = static_cast<int>(Params.SmoothDimension);

            shaderParams->VolDim.X = volDim.X;
            shaderParams->VolDim.Y = volDim.Y;
            shaderParams->VolDim.Z = volDim.Z;

            auto extrnlTexRDG = RegisterExternalTexture(
                grphBldr, Params.VolumeTexture->GetResource()->GetTexture3DRHI(),
                *VIS4EARTH_GET_NAME_IN_FUNCTION("Volume Texture"));
            shaderParams->VolInput = grphBldr.CreateSRV(FRDGTextureSRVDesc(extrnlTexRDG));

            shaderParams->VolOutput =
                grphBldr.CreateUAV(FRDGBufferUAVDesc(smoothedVolBuf, EPixelFormat::PF_R32_FLOAT));
        }

        TShaderMapRef<FVolumeSmoothShader> shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        FComputeShaderUtils::AddPass(
            grphBldr, RDG_EVENT_NAME("Volume Smoothing"),
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, shader, shaderParams,
            FIntVector(
                FMath::DivideAndRoundUp(shaderParams->VolDim.X, VIS4EARTH_THREAD_PER_GROUP_X),
                FMath::DivideAndRoundUp(shaderParams->VolDim.Y, VIS4EARTH_THREAD_PER_GROUP_Y),
                FMath::DivideAndRoundUp(shaderParams->VolDim.Z, VIS4EARTH_THREAD_PER_GROUP_Z)));

        auto bufReadback =
            new FRHIGPUBufferReadback(*VIS4EARTH_GET_NAME_IN_FUNCTION("Readback Smoothed Volume"));
        AddEnqueueCopyPass(grphBldr, bufReadback, smoothedVolBuf, smoothedVolBuf->GetSize());

        auto waitTask = [bufReadback, volDim,
                         callback = Params.FinishedCallback](auto &&waitTask) -> void {
            if (!bufReadback->IsReady()) {
                AsyncTask(ENamedThreads::ActualRenderingThread,
                          [waitTask]() { waitTask(waitTask); });
                return;
            }

            auto bufNum = volDim.X * volDim.Y * volDim.Z;
            auto bufSz = sizeof(float) * bufNum;

            auto volDat = MakeShared<TArray<float>>();
            volDat->SetNum(bufNum);
            FMemory::Memcpy(volDat->GetData(), bufReadback->Lock(bufSz), bufSz);

            AsyncTask(ENamedThreads::GameThread, [volDat, callback]() { callback(volDat); });

            bufReadback->Unlock();
            delete bufReadback;
        };
        AsyncTask(ENamedThreads::ActualRenderingThread, [waitTask]() { waitTask(waitTask); });

        grphBldr.Execute();
    }
};
