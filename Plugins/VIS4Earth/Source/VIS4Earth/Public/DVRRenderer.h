// Author: Kouek Kou

#pragma once

#include "GeoRenderer.h"

#include "Util.h"

class VIS4EARTH_API FDVRRenderer : public FGeoRenderer {
  public:
    ~FDVRRenderer() { Unregister(); }

    virtual void Register() override;
    virtual void Unregister() override;

    struct RenderParameters {
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(bool, UsePreIntegratedTF, false)
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(FIntVector2, Tessellation, {100 VIS4EARTH_COMMA 100})
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(int, MaxStepCount, 1000)
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(float, Step,
                                         .01f * (FGeoRenderer::GeoParameters::DefHeightRange[1] -
                                                 FGeoRenderer::GeoParameters::DefHeightRange[0]))
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(float, RelativeLightness, 1.f)
        TWeakObjectPtr<UVolumeTexture> VolumeTexture;
        TWeakObjectPtr<UTexture2D> TransferFunctionTexture;
    };
    void SetRenderParameters(const RenderParameters &Params);

  private:
    uint32 vertNum = 0, primNum = 0;
    RenderParameters rndrParams;
    FBufferRHIRef vertexBuffer;
    FBufferRHIRef indexBuffer;

    virtual void render(FPostOpaqueRenderParameters &PostQpqRndrParams) override;

    void generateMesh(FRHICommandListImmediate &RHICmdList);

  public:
    struct VertexAttr {
        FVector3f Position; // position in [0,1]^3
    };
    class FVertexAttrDeclaration : public FRenderResource {
      public:
        FVertexDeclarationRHIRef VertexDeclarationRHI;

        void virtual InitRHI(FRHICommandListBase &RHICmdList) override {
            FVertexDeclarationElementList elemeList;
            uint16 stride = sizeof(VertexAttr);
            elemeList.Emplace(0, STRUCT_OFFSET(VertexAttr, Position), VET_Float3, 0, stride);

            VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(elemeList);
        }

        void virtual ReleaseRHI() override { VertexDeclarationRHI.SafeRelease(); }
    };
};
