// Author : Kouek Kou

#include "DVRRenderer.h"

#include <array>

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "EngineModule.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

#include "Runtime/Renderer/Private/SceneRendering.h"

#include "Util.h"

TGlobalResource<FDVRRenderer::FVertexAttrDeclaration> GDVRRendererVertexAttrDeclaration;

class VIS4EARTH_API FDVRShader : public FGlobalShader {
  public:
    SHADER_USE_PARAMETER_STRUCT(FDVRShader, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, VIS4EARTH_API)
    SHADER_PARAMETER(int32, MaxStepCnt)
    SHADER_PARAMETER(float, Step)
    SHADER_PARAMETER(float, RelativeLightness)
    SHADER_PARAMETER(FVector2f, LonRng)
    SHADER_PARAMETER(FVector2f, LatRng)
    SHADER_PARAMETER(FVector2f, HeightRng)
    SHADER_PARAMETER(FVector3f, EyePosToEarth)
    SHADER_PARAMETER(FMatrix44f, EarthToEye)
    SHADER_PARAMETER_SAMPLER(SamplerState, VolSamplerState)
    SHADER_PARAMETER_SAMPLER(SamplerState, TFSamplerState)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, VolInput)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, TFInput)
    RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()
};

class VIS4EARTH_API FDVRShaderVS : public FDVRShader {
  public:
    DECLARE_GLOBAL_SHADER(FDVRShaderVS);

    FDVRShaderVS() {}
    FDVRShaderVS(const ShaderMetaType::CompiledShaderInitializerType &Initializer)
        : FDVRShader(Initializer) {}
};

class VIS4EARTH_API FDVRShaderPS : public FDVRShader {
  public:
    DECLARE_GLOBAL_SHADER(FDVRShaderPS);

    FDVRShaderPS() {}
    FDVRShaderPS(const ShaderMetaType::CompiledShaderInitializerType &Initializer)
        : FDVRShader(Initializer) {}

    class FUsePreIntTFDim : SHADER_PERMUTATION_BOOL("USE_PREINT_TF");
    using FPermutationDomain = TShaderPermutationDomain<FUsePreIntTFDim>;

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters &Parameters) {
        return true;
    }
};

IMPLEMENT_GLOBAL_SHADER(FDVRShaderVS, "/VIS4Earth/DVR.usf", "VS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FDVRShaderPS, "/VIS4Earth/DVR.usf", "PS", SF_Pixel);

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

void FDVRRenderer::SetRenderParameters(const RenderParameters &Params) {
    bool needGenMesh = !vertexBuffer.IsValid() || Params.Tessellation != rndrParams.Tessellation;

    rndrParams = Params;

    if (needGenMesh) {
        ENQUEUE_RENDER_COMMAND(UnregisterRenderScreenRenderer)
        ([renderer = SharedThis(this)](FRHICommandListImmediate &RHICmdList) {
            renderer->generateMesh(RHICmdList);
        });
    }
}

void FDVRRenderer::render(FPostOpaqueRenderParameters &PostQpqRndrParams) {
    if (!rndrParams.VolumeTexture.IsValid() || !rndrParams.TransferFunctionTexture.IsValid() ||
        !geoParams.GeoRef.IsValid() || !vertexBuffer.IsValid() || primNum == 0)
        return;
    if (!rndrParams.VolumeTexture->GetResource() ||
        !rndrParams.TransferFunctionTexture->GetResource())
        return;

    auto &grphBldr = *PostQpqRndrParams.GraphBuilder;

    auto shaderParams = grphBldr.AllocParameters<FDVRShader::FParameters>();
    {
        shaderParams->MaxStepCnt = rndrParams.MaxStepCount;
        shaderParams->Step = rndrParams.Step;
        shaderParams->RelativeLightness = rndrParams.RelativeLightness;

        shaderParams->LonRng = FVector2f(FMath::DegreesToRadians(geoParams.LongtitudeRange));
        shaderParams->LatRng = FVector2f(FMath::DegreesToRadians(geoParams.LatitudeRange));
        shaderParams->HeightRng = FVector2f(geoParams.HeightRange);

        shaderParams->EyePosToEarth =
            FVector3f(geoParams.GeoRef->TransformUnrealPositionToEarthCenteredEarthFixed(
                PostQpqRndrParams.View->ViewLocation));
        shaderParams->EarthToEye =
            FMatrix44f(geoParams.GeoRef->ComputeEarthCenteredEarthFixedToUnrealTransformation() *
                       PostQpqRndrParams.View->ViewMatrices.GetViewProjectionMatrix());

        shaderParams->VolSamplerState =
            TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        shaderParams->TFSamplerState =
            TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

        auto extrnlTexRDG = RegisterExternalTexture(
            grphBldr, rndrParams.VolumeTexture->GetResource()->GetTexture3DRHI(),
            *VIS4EARTH_GET_NAME_IN_FUNCTION("Volume Texture"));
        shaderParams->VolInput = grphBldr.CreateSRV(FRDGTextureSRVDesc(extrnlTexRDG));
        extrnlTexRDG = RegisterExternalTexture(
            grphBldr, rndrParams.TransferFunctionTexture->GetResource()->GetTexture2DRHI(),
            *VIS4EARTH_GET_NAME_IN_FUNCTION("TF Texture"));
        shaderParams->TFInput = grphBldr.CreateSRV(FRDGTextureSRVDesc(extrnlTexRDG));

        shaderParams->RenderTargets[0] =
            FRenderTargetBinding(PostQpqRndrParams.ColorTexture, ERenderTargetLoadAction::ELoad);
        shaderParams->RenderTargets.DepthStencil = FDepthStencilBinding(
            PostQpqRndrParams.DepthTexture, ERenderTargetLoadAction::ELoad,
            ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);
    }

    grphBldr.AddPass(
        RDG_EVENT_NAME("Direct Volume Rendering"), shaderParams, ERDGPassFlags::Raster,
        [shaderParams,
         viewportSz = FIntVector2(PostQpqRndrParams.ViewportRect.Width(),
                                  PostQpqRndrParams.ViewportRect.Height()),
         usePreIntegratedTF = rndrParams.UsePreIntegratedTF, vertNum = this->vertNum,
         primNum = this->primNum, vertexBuffer = this->vertexBuffer,
         indexBuffer = this->indexBuffer](FRHICommandList &RHICmdList) {
            RHICmdList.SetViewport(0.f, 0.f, 0.f, viewportSz.X, viewportSz.Y, 1.f);

            TShaderMapRef<FDVRShaderVS> shaderVS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
            TShaderMapRef<FDVRShaderPS> shaderPS(GetGlobalShaderMap(GMaxRHIFeatureLevel), [&]() {
                FDVRShaderPS::FPermutationDomain permuVec;
                permuVec.Set<FDVRShaderPS::FUsePreIntTFDim>(usePreIntegratedTF);
                return permuVec;
            }());

            FGraphicsPipelineStateInitializer graphicsPSOInit;
            RHICmdList.ApplyCachedRenderTargets(graphicsPSOInit);
            graphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
            graphicsPSOInit.DepthStencilState =
                TStaticDepthStencilState<false, CF_Greater>::GetRHI();
            graphicsPSOInit.BlendState =
                TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
            graphicsPSOInit.PrimitiveType = PT_TriangleList;
            graphicsPSOInit.BoundShaderState.VertexDeclarationRHI =
                GDVRRendererVertexAttrDeclaration.VertexDeclarationRHI;
            graphicsPSOInit.BoundShaderState.VertexShaderRHI = shaderVS.GetVertexShader();
            graphicsPSOInit.BoundShaderState.PixelShaderRHI = shaderPS.GetPixelShader();
            SetGraphicsPipelineState(RHICmdList, graphicsPSOInit, 0);

            SetShaderParameters(RHICmdList, shaderPS, shaderPS.GetPixelShader(), *shaderParams);
            SetShaderParameters(RHICmdList, shaderVS, shaderVS.GetVertexShader(), *shaderParams);

            RHICmdList.SetStreamSource(0, vertexBuffer, 0);
            RHICmdList.DrawIndexedPrimitive(indexBuffer, 0, 0, vertNum, 0, primNum, 1);
        });
}

void FDVRRenderer::generateMesh(FRHICommandListImmediate &RHICmdList) {
    TArray<VertexAttr> vertices;
    TArray<uint32> indices;

    int32 btmSurfVertStart;
    {
        auto genSurfVertices = [&](bool top) {
            for (int32 latIdx = 0; latIdx < rndrParams.Tessellation.Y; ++latIdx)
                for (int32 lonIdx = 0; lonIdx < rndrParams.Tessellation.X; ++lonIdx) {
                    vertices.Emplace();
                    auto &position = vertices.Last().Position;
                    position.X = 1. * lonIdx / (rndrParams.Tessellation.X - 1);
                    position.Y = 1. * latIdx / (rndrParams.Tessellation.Y - 1);
                    position.Z = top ? 1. : 0.;
                }
        };
        genSurfVertices(true);
        btmSurfVertStart = vertices.Num();
        genSurfVertices(false);
    }

    {
        auto addTri = [&](std::array<int32, 3> triIndices) {
            indices.Emplace(triIndices[0]);
            indices.Emplace(triIndices[1]);
            indices.Emplace(triIndices[2]);
        };
        auto addTopBotSurf = [&](bool top, int32 latIdx, int32 lonIdx) {
            auto start = top ? 0 : btmSurfVertStart;
            std::array<int32, 4> quadIndices = {
                start + latIdx * rndrParams.Tessellation.X + lonIdx,
                start + latIdx * rndrParams.Tessellation.X + lonIdx + (top ? 1 : -1),
                start + (latIdx + 1) * rndrParams.Tessellation.X + lonIdx + (top ? 1 : -1),
                start + (latIdx + 1) * rndrParams.Tessellation.X + lonIdx};

            addTri({quadIndices[0], quadIndices[1], quadIndices[2]});
            addTri({quadIndices[0], quadIndices[2], quadIndices[3]});
        };
        for (int32 latIdx = 0; latIdx < rndrParams.Tessellation.Y - 1; ++latIdx)
            for (int32 lonIdx = 0; lonIdx < rndrParams.Tessellation.X - 1; ++lonIdx) {
                addTopBotSurf(true, latIdx, lonIdx);
                addTopBotSurf(false, latIdx, rndrParams.Tessellation.X - 1 - lonIdx);
            }

        auto addSideSurf = [&](int32 latIdx, int32 lonIdx, FIntVector2 dir) {
            std::array<int32, 4> quadIndices = {
                btmSurfVertStart + latIdx * rndrParams.Tessellation.X + lonIdx,
                btmSurfVertStart + (latIdx + dir.Y) * rndrParams.Tessellation.X + lonIdx + dir.X,
                (latIdx + dir.Y) * rndrParams.Tessellation.X + lonIdx + dir.X,
                latIdx * rndrParams.Tessellation.X + lonIdx};
            addTri({quadIndices[0], quadIndices[1], quadIndices[2]});
            addTri({quadIndices[0], quadIndices[2], quadIndices[3]});
        };
        for (int32 lonIdx = 0; lonIdx < rndrParams.Tessellation.X - 1; ++lonIdx) {
            addSideSurf(0, lonIdx, {1, 0});
            addSideSurf(rndrParams.Tessellation.X - 1, rndrParams.Tessellation.X - 1 - lonIdx,
                        {-1, 0});
        }
        for (int32 latIdx = 0; latIdx < rndrParams.Tessellation.X - 1; ++latIdx) {
            addSideSurf(rndrParams.Tessellation.X - 1 - latIdx, 0, {0, -1});
            addSideSurf(latIdx, rndrParams.Tessellation.X - 1, {0, 1});
        }
    }

    if (indices.IsEmpty()) {
        vertNum = primNum = 0;
        return;
    }

    vertNum = vertices.Num();
    auto bufSz = sizeof(VertexAttr) * vertNum;
    if (!vertexBuffer.IsValid() || vertexBuffer->GetSize() != bufSz) {
        FRHIResourceCreateInfo info(*VIS4EARTH_GET_NAME_IN_FUNCTION("Create Vertex Buffer"));
        vertexBuffer = RHICmdList.CreateVertexBuffer(bufSz, BUF_VertexBuffer | BUF_Static,
                                                     ERHIAccess::VertexOrIndexBuffer, info);
    }
    auto dat = RHICmdList.LockBuffer(vertexBuffer, 0, bufSz, RLM_WriteOnly);
    FMemory::Memmove(dat, vertices.GetData(), bufSz);
    RHICmdList.UnlockBuffer(vertexBuffer);

    primNum = indices.Num() / 3;
    bufSz = sizeof(uint32) * indices.Num();
    if (!indexBuffer.IsValid() || indexBuffer->GetSize() != bufSz) {
        FRHIResourceCreateInfo info(*VIS4EARTH_GET_NAME_IN_FUNCTION("Create Index Buffer"));
        indexBuffer =
            RHICmdList.CreateIndexBuffer(sizeof(uint32), bufSz, BUF_VertexBuffer | BUF_Static,
                                         ERHIAccess::VertexOrIndexBuffer, info);
    }
    dat = RHICmdList.LockBuffer(indexBuffer, 0, bufSz, RLM_WriteOnly);
    FMemory::Memmove(dat, indices.GetData(), bufSz);
    RHICmdList.UnlockBuffer(indexBuffer);
}
