// Author: Kouek Kou

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeoComponent.h"
#include "VolumeDataComponent.h"

#include "DVRRenderer.h"

#include "DVRActor.generated.h"

/*
 * Class: ADVRActor
 * Function:
 * -- Implements time-variable direct volume rendering.
 */
UCLASS()
class VIS4EARTH_API ADVRActor : public AActor {
    GENERATED_BODY()

  public:
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    int MaxStepCount = FDVRRenderer::RenderParameters::DefMaxStepCount;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    float Step = FDVRRenderer::RenderParameters::DefStep;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    float RelativeLightness = FDVRRenderer::RenderParameters::DefRelativeLightness;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UGeoComponent> GeoComponent;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UVolumeDataComponent> VolumeComponent;

    ADVRActor();

    void Destroyed() override {
        Super::Destroyed();
        destroyRenderer();
    }

  private:
    TSharedPtr<FDVRRenderer> renderer;

    void setupSignalsSlots();
    void setupRenderer();
    void destroyRenderer();

#ifdef WITH_EDITOR
  public:
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent &PropChngedEv) override {
        Super::PostEditChangeProperty(PropChngedEv);

        if (!PropChngedEv.MemberProperty)
            return;

        auto name = PropChngedEv.MemberProperty->GetFName();
        if (name == GET_MEMBER_NAME_CHECKED(ADVRActor, MaxStepCount) ||
            name == GET_MEMBER_NAME_CHECKED(ADVRActor, Step) ||
            name == GET_MEMBER_NAME_CHECKED(ADVRActor, RelativeLightness)) {
            setupRenderer();
            return;
        }
    }
#endif // WITH_EDITOR
};
