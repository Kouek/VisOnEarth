// Author: Kouek Kou

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeoComponent.h"
#include "VolumeDataComponent.h"

#include "MCSRenderer.h"

#include "MCSActor.generated.h"

/*
 * Class: AMCSActor
 * Function:
 * -- Implements Marching Square Isopleth Generatiion and Rendering.
 */
UCLASS()
class VIS4EARTH_API AMCSActor : public AActor {
    GENERATED_BODY()

  public:
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    EMCSLineStyle LineStyle = FMCSRenderer::RenderParameters::DefLineStyle;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    bool UseLerp = FMCSRenderer::MCSParameters::DefUseLerp;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    bool UseSmoothedVolume = FMCSRenderer::MCSParameters::DefUseSmoothedVolume;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    FIntVector2 HeightRange = FMCSRenderer::MCSParameters::DefHeightRange;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    float IsoValue = FMCSRenderer::MCSParameters::DefIsoValue;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UGeoComponent> GeoComponent;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UVolumeDataComponent> VolumeComponent;

    AMCSActor();
    ~AMCSActor() { destroyRenderer(); }

    void Destroyed() override {
        Super::Destroyed();
        destroyRenderer();
    }

  private:
    TSharedPtr<FMCSRenderer> renderer;

    void setupSignalsSlots();
    void checkAndCorrectParameters();
    void setupRenderer(bool shouldMarchSquare = false);
    void destroyRenderer();

  private:
#ifdef WITH_EDITOR
  public:
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent &PropChngedEv) override {
        Super::PostEditChangeProperty(PropChngedEv);

        if (!PropChngedEv.MemberProperty)
            return;

        auto name = PropChngedEv.MemberProperty->GetFName();
        if (name == GET_MEMBER_NAME_CHECKED(AMCSActor, LineStyle)) {
            setupRenderer();
            return;
        }
        if (name == GET_MEMBER_NAME_CHECKED(AMCSActor, UseLerp) ||
            name == GET_MEMBER_NAME_CHECKED(AMCSActor, UseSmoothedVolume) ||
            name == GET_MEMBER_NAME_CHECKED(AMCSActor, HeightRange) ||
            name == GET_MEMBER_NAME_CHECKED(AMCSActor, IsoValue)) {
            setupRenderer(true);
            return;
        }
    }
#endif // WITH_EDITOR
};
