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
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(int, MaxStepCount, 1000)
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(float, Step, 10000.f)
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(float, RelativeLightness, 1.f)
        TWeakObjectPtr<UVolumeTexture> VolumeTexture;
        TWeakObjectPtr<UTexture2D> TransferFunctionTexture;
    };
    void SetRenderParameters(const RenderParameters &Params) { rndrParams = Params; }

  private:
    RenderParameters rndrParams;

    virtual void render(FPostOpaqueRenderParameters &PostQpqRndrParams) override;
};
