// Author: Kouek Kou

#pragma once

#include "GeoRenderer.h"

#include "Util.h"
#include "VolumeDataComponent.h"

class VIS4EARTH_API FMCSRenderer : public FGeoRenderer {
  public:
    ~FMCSRenderer() { Unregister(); }

    virtual void Register() override;
    virtual void Unregister() override;

    struct RenderParameters {
        TWeakObjectPtr<UTexture2D> TransferFunctionTexture;
    };
    void SetRenderParameters(const RenderParameters &Params) { rndrParams = Params; }

    struct MCSParameters {
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(FIntVector2, HeightRange, {0 VIS4EARTH_COMMA 0})
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(float, IsoValue, 0.f)
        TWeakObjectPtr<UVolumeDataComponent> VolumeComponent;
    };
    void MarchingSquare(const MCSParameters &Params);

  private:
    uint32 vertNum = 0, primNum = 0;
    RenderParameters rndrParams;
    FBufferRHIRef vertexBuffer;
    FBufferRHIRef indexBuffer;

    virtual void render(FPostOpaqueRenderParameters &PostQpqRndrParams) override;

    void marchingSquare(const MCSParameters &Params, FRHICommandListImmediate &RHICmdList);

  public:
    struct VertexAttr {
        FVector3f Position; // position in [0,1]^3
        float Scalar;
    };
    class FVertexAttrDeclaration : public FRenderResource {
      public:
        FVertexDeclarationRHIRef VertexDeclarationRHI;

        void virtual InitRHI(FRHICommandListBase &RHICmdList) override {
            FVertexDeclarationElementList elemeList;
            uint16 stride = sizeof(VertexAttr);
            elemeList.Emplace(0, STRUCT_OFFSET(VertexAttr, Position), VET_Float3, 0, stride);
            elemeList.Emplace(0, STRUCT_OFFSET(VertexAttr, Scalar), VET_Float1, 1, stride);

            VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(elemeList);
        }

        void virtual ReleaseRHI() override { VertexDeclarationRHI.SafeRelease(); }
    };
};
