// Author: Kouek Kou

#pragma once

#include "CoreMinimal.h"
#include "Engine/VolumeTexture.h"

#include "Util.h"

#include "CesiumGeoreference.h"

class VIS4EARTH_API FGeoRenderer {
  public:
    virtual ~FGeoRenderer() {}

    struct GeoParameters {
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(FVector2d, LongtitudeRange,
                                         {100.05 VIS4EARTH_COMMA 129.95})
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(FVector2d, LatitudeRange, {-4.95 VIS4EARTH_COMMA 29.95})
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(FVector2d, HeightRange, {300000. VIS4EARTH_COMMA 900000.})
        TWeakObjectPtr<ACesiumGeoreference> GeoRef;
    };
    void SetGeographicalParameters(const GeoParameters &Params) { geoParams = Params; }

    virtual void Register() = 0;
    virtual void Unregister() = 0;

  protected:
    GeoParameters geoParams;
    FDelegateHandle onPostOpaqueRender;

    virtual void render(FPostOpaqueRenderParameters &PostQpqRndrParams) = 0;
};
