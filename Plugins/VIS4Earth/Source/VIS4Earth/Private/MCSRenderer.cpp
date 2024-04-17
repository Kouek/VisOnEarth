#include "MCSRenderer.h"

#include <unordered_map>

#include "EngineModule.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

#include "Runtime/Renderer/Private/SceneRendering.h"

TGlobalResource<FMCSRenderer::FVertexAttrDeclaration> GMCSRendererVertexAttrDeclaration;

class VIS4EARTH_API FMCSShader : public FGlobalShader {
  public:
    SHADER_USE_PARAMETER_STRUCT(FMCSShader, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, VIS4EARTH_API)
    SHADER_PARAMETER(FVector2f, LonRng)
    SHADER_PARAMETER(FVector2f, LatRng)
    SHADER_PARAMETER(FVector2f, HeightRng)
    SHADER_PARAMETER(FMatrix44f, EarthToEye)
    SHADER_PARAMETER_SAMPLER(SamplerState, TFSamplerState)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, TFInput)
    RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters &Parameters) {
        return true;
    }
};

class VIS4EARTH_API FMCSShaderVS : public FMCSShader {
  public:
    DECLARE_GLOBAL_SHADER(FMCSShaderVS)

    FMCSShaderVS() {}
    FMCSShaderVS(const ShaderMetaType::CompiledShaderInitializerType &Initializer)
        : FMCSShader(Initializer) {}
};

class VIS4EARTH_API FMCSShaderPS : public FMCSShader {
  public:
    DECLARE_GLOBAL_SHADER(FMCSShaderPS)

    FMCSShaderPS() {}
    FMCSShaderPS(const ShaderMetaType::CompiledShaderInitializerType &Initializer)
        : FMCSShader(Initializer) {}
};

IMPLEMENT_GLOBAL_SHADER(FMCSShaderVS, "/VIS4Earth/MCS.usf", "VS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FMCSShaderPS, "/VIS4Earth/MCS.usf", "PS", SF_Pixel);

void FMCSRenderer::Register() {
    Unregister();

    ENQUEUE_RENDER_COMMAND(RegisterRenderScreenRenderer)
    ([renderer = SharedThis(this)](FRHICommandListImmediate &RHICmdList) {
        renderer->onPostOpaqueRender = GetRendererModule().RegisterPostOpaqueRenderDelegate(
            FPostOpaqueRenderDelegate::CreateRaw(&renderer.Get(), &FMCSRenderer::render));
    });
}

void FMCSRenderer::Unregister() {
    if (!onPostOpaqueRender.IsValid())
        return;

    ENQUEUE_RENDER_COMMAND(RegisterRenderScreenRenderer)
    ([renderer = SharedThis(this)](FRHICommandListImmediate &RHICmdList) {
        GetRendererModule().RemovePostOpaqueRenderDelegate(renderer->onPostOpaqueRender);
        renderer->onPostOpaqueRender.Reset();
    });
}

void FMCSRenderer::MarchingSquare(const MCSParameters &Params) {
    if (!Params.VolumeComponent.IsValid())
        return;

    ENQUEUE_RENDER_COMMAND(UregisterRenderScreenRenderer)
    ([this, Params](FRHICommandListImmediate &RHICmdList) { marchingSquare(Params, RHICmdList); });
}

void FMCSRenderer::render(FPostOpaqueRenderParameters &PostQpqRndrParams) {
    if (!rndrParams.TransferFunctionTexture.IsValid() || !geoParams.GeoRef.IsValid() ||
        !vertexBuffer.IsValid() || primNum == 0)
        return;

    auto &grphBldr = *PostQpqRndrParams.GraphBuilder;

    auto shaderParams = grphBldr.AllocParameters<FMCSShader::FParameters>();
    {
        shaderParams->LonRng = FVector2f(FMath::DegreesToRadians(geoParams.LongtitudeRange));
        shaderParams->LatRng = FVector2f(FMath::DegreesToRadians(geoParams.LatitudeRange));
        shaderParams->HeightRng = FVector2f(geoParams.HeightRange);

        shaderParams->EarthToEye =
            FMatrix44f(geoParams.GeoRef->ComputeEarthCenteredEarthFixedToUnrealTransformation() *
                       PostQpqRndrParams.View->ViewMatrices.GetViewProjectionMatrix());

        shaderParams->TFSamplerState =
            TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

        auto extrnlTexRDG = RegisterExternalTexture(
            grphBldr, rndrParams.TransferFunctionTexture->GetResource()->GetTexture2DRHI(),
            TEXT("TF Texture"));
        shaderParams->TFInput = grphBldr.CreateSRV(FRDGTextureSRVDesc(extrnlTexRDG));

        shaderParams->RenderTargets[0] =
            FRenderTargetBinding(PostQpqRndrParams.ColorTexture, ERenderTargetLoadAction::ELoad);
        shaderParams->RenderTargets.DepthStencil = FDepthStencilBinding(
            PostQpqRndrParams.DepthTexture, ERenderTargetLoadAction::ELoad,
            ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);
    }

    grphBldr.AddPass(
        RDG_EVENT_NAME("Draw Marching Square"), shaderParams, ERDGPassFlags::Raster,
        [shaderParams, vertNum = this->vertNum, primNum = this->primNum,
         vertexBuffer = this->vertexBuffer,
         indexBuffer = this->indexBuffer](FRHICommandList &RHICmdList) {
            TShaderMapRef<FMCSShaderVS> shaderVS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
            TShaderMapRef<FMCSShaderPS> shaderPS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

            FGraphicsPipelineStateInitializer graphicsPSOInit;
            RHICmdList.ApplyCachedRenderTargets(graphicsPSOInit);
            graphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
            graphicsPSOInit.DepthStencilState =
                TStaticDepthStencilState<true, CF_Greater>::GetRHI();
            graphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
            graphicsPSOInit.PrimitiveType = PT_LineList;
            graphicsPSOInit.BoundShaderState.VertexDeclarationRHI =
                GMCSRendererVertexAttrDeclaration.VertexDeclarationRHI;
            graphicsPSOInit.BoundShaderState.VertexShaderRHI = shaderVS.GetVertexShader();
            graphicsPSOInit.BoundShaderState.PixelShaderRHI = shaderPS.GetPixelShader();
            SetGraphicsPipelineState(RHICmdList, graphicsPSOInit, 0);

            SetShaderParameters(RHICmdList, shaderPS, shaderPS.GetPixelShader(), *shaderParams);
            SetShaderParameters(RHICmdList, shaderVS, shaderVS.GetVertexShader(), *shaderParams);

            RHICmdList.SetStreamSource(0, vertexBuffer, 0);
            RHICmdList.DrawIndexedPrimitive(indexBuffer, 0, 0, vertNum, 0, primNum, 1);
        });
}

void FMCSRenderer::marchingSquare(const MCSParameters &Params,
                                  FRHICommandListImmediate &RHICmdList) {
    if (!Params.VolumeComponent.IsValid() || !Params.VolumeComponent->VolumeTexture)
        return;

    FIntVector3 voxPerVol(Params.VolumeComponent->VolumeTexture->GetSizeX(),
                          Params.VolumeComponent->VolumeTexture->GetSizeY(),
                          Params.VolumeComponent->VolumeTexture->GetSizeZ());
    auto voxPerVolYxX = static_cast<size_t>(voxPerVol.Y) * voxPerVol.X;

    std::unordered_map<uint64, uint32> edge2vertIDs;
    int32_t prevHeightVertNum = 0;
    TArray<VertexAttr> vertices;
    TArray<uint32> indices;
    auto addLineSeg = [&](auto &&func, const FIntVector3 &startPos, const FVector4f &scalars,
                          const FVector4f &omegas, uint8 mask, auto &&...masks) {
        for (int32 i = 0; i < 4; ++i) {
            if (((mask >> i) & 0b1) == 0)
                continue;

            // Edge indexed by Start Voxel Position
            // +----------+
            // | /*\      |
            // |  e1      |
            // |  * e0 *> |
            // +----------+
            // *:   startPos
            // *>:  startPos + (1,0)
            // /*\: startPos + (0,1)
            // ID(e0): [ ID(*) : 63bit | 0 : 1bit ]
            // ID(e1): [ ID(*) : 63bit | 1 : 1bit ]
            auto edgeID = static_cast<uint64>(startPos.Y + (i == 2 ? 1 : 0)) * voxPerVol.X +
                          startPos.X + (i == 1 ? 1 : 0);
            edgeID = (edgeID << 1) + (i == 1 || i == 3 ? 1 : 0);
            if (auto itr = edge2vertIDs.find(edgeID); itr != edge2vertIDs.end()) {
                indices.Emplace(itr->second + prevHeightVertNum);
                continue;
            }

            FVector3f pos(startPos.X + (i == 0 || i == 2 ? omegas[i]
                                        : i == 1         ? 1.f
                                                         : 0.f),
                          startPos.Y + (i == 1 || i == 3 ? omegas[i]
                                        : i == 2         ? 1.f
                                                         : 0.f),
                          startPos.Z);
            pos /= FVector3f(voxPerVol);

            auto scalar = [&]() {
                switch (i) {
                case 0:
                    return omegas[0] * scalars[0] + omegas[1] * scalars[1];
                case 1:
                    return omegas[1] * scalars[1] + omegas[2] * scalars[2];
                case 2:
                    return omegas[2] * scalars[3] + omegas[3] * scalars[2];
                case 3:
                    return omegas[3] * scalars[0] + omegas[0] * scalars[3];
                }
                return 0.f;
            }();
            scalar /= VolumeData::GetVoxelMaxValue(Params.VolumeComponent->GetVolumeVoxelType());

            indices.Emplace(vertices.Num());
            vertices.Emplace(pos, scalar);
            edge2vertIDs.emplace(edgeID, indices.Last() - prevHeightVertNum);
        }

        if constexpr (sizeof...(masks) >= 1)
            func(func, startPos, scalars, omegas, masks...);
    };
    auto gen = [&]<SupportedVoxelType T>(T) {
        FIntVector3 pos;
        for (pos.Z = Params.HeightRange[0]; pos.Z <= Params.HeightRange[1]; ++pos.Z) {
            edge2vertIDs.clear(); // hash map only stores vertices on the same height
            prevHeightVertNum = vertices.Num();

            for (pos.Y = 0; pos.Y < voxPerVol.Y - 1; ++pos.Y)
                for (pos.X = 0; pos.X < voxPerVol.X - 1; ++pos.X) {
                    // Voxels in CCW order form a grid
                    // +------------+
                    // |  3 <--- 2  |
                    // |  |     /|\ |
                    // | \|/     |  |
                    // |  0 ---> 1  |
                    // +------------+
                    uint8 cornerState = 0;
                    FVector4f scalars;
                    for (int32 i = 0; i < 4; ++i) {
                        scalars[i] = Params.VolumeComponent->SampleVolumeCPUData<T>(pos);
                        if (scalars[i] >= Params.IsoValue)
                            cornerState |= 1 << i;

                        pos.X += i == 0 ? 1 : i == 2 ? -1 : 0;
                        pos.Y += i == 1 ? 1 : i == 3 ? -1 : 0;
                    }
                    FVector4f omegas(scalars[0] / (scalars[1] + scalars[0]),
                                     scalars[1] / (scalars[2] + scalars[1]),
                                     scalars[3] / (scalars[3] + scalars[2]),
                                     scalars[0] / (scalars[0] + scalars[3]));

                    switch (cornerState) {
                    case 0b0001:
                    case 0b1110:
                        addLineSeg(addLineSeg, pos, scalars, omegas, 0b1001);
                        break;
                    case 0b0010:
                    case 0b1101:
                        addLineSeg(addLineSeg, pos, scalars, omegas, 0b0011);
                        break;
                    case 0b0011:
                    case 0b1100:
                        addLineSeg(addLineSeg, pos, scalars, omegas, 0b1010);
                        break;
                    case 0b0100:
                    case 0b1011:
                        addLineSeg(addLineSeg, pos, scalars, omegas, 0b0110);
                        break;
                    case 0b0101:
                        addLineSeg(addLineSeg, pos, scalars, omegas, 0b0011, 0b1100);
                        break;
                    case 0b1010:
                        addLineSeg(addLineSeg, pos, scalars, omegas, 0b0110, 0b1001);
                        break;
                    case 0b0110:
                    case 0b1001:
                        addLineSeg(addLineSeg, pos, scalars, omegas, 0b0101);
                        break;
                    case 0b0111:
                    case 0b1000:
                        addLineSeg(addLineSeg, pos, scalars, omegas, 0b1100);
                        break;
                    }
                }
        }
    };

    switch (Params.VolumeComponent->GetVolumeVoxelType()) {
    case ESupportedVoxelType::EUInt8:
        gen(uint8(0));
        break;
    }
    if (indices.IsEmpty()) {
        vertNum = primNum = 0;
        return;
    }

    vertNum = vertices.Num();
    auto bufSz = sizeof(VertexAttr) * vertNum;
    if (!vertexBuffer.IsValid() || vertexBuffer->GetSize() != bufSz) {
        FRHIResourceCreateInfo info(TEXT("Marching Square Create Vertex Buffer"));
        vertexBuffer = RHICmdList.CreateVertexBuffer(bufSz, BUF_VertexBuffer | BUF_Static,
                                                     ERHIAccess::VertexOrIndexBuffer, info);
    }
    auto dat = RHICmdList.LockBuffer(vertexBuffer, 0, bufSz, RLM_WriteOnly);
    FMemory::Memmove(dat, vertices.GetData(), bufSz);
    RHICmdList.UnlockBuffer(vertexBuffer);

    primNum = indices.Num() / 2;
    bufSz = sizeof(uint32) * indices.Num();
    if (!indexBuffer.IsValid() || indexBuffer->GetSize() != bufSz) {
        FRHIResourceCreateInfo info(TEXT("Marching Square Create Index Buffer"));
        indexBuffer = RHICmdList.CreateVertexBuffer(bufSz, BUF_VertexBuffer | BUF_Static,
                                                    ERHIAccess::VertexOrIndexBuffer, info);
    }
    dat = RHICmdList.LockBuffer(indexBuffer, 0, bufSz, RLM_WriteOnly);
    FMemory::Memmove(dat, indices.GetData(), bufSz);
    RHICmdList.UnlockBuffer(indexBuffer);
}
