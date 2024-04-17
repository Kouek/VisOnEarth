#include "DVRActor.h"

#include "EngineModule.h"
#include "ShaderParameterStruct.h"

#include "Runtime/Renderer/Private/SceneRendering.h"

ADVRActor::ADVRActor() {
    GeoComponent = CreateDefaultSubobject<UGeoComponent>(TEXT("Geographics"));
    VolumeComponent = CreateDefaultSubobject<UVolumeDataComponent>(TEXT("VolumeData"));

    setupRenderer();
    setupSignalsSlots();
}

void ADVRActor::setupSignalsSlots() {
    GeoComponent->OnGeographicsChanged.AddLambda(
        [actor = TWeakObjectPtr<ADVRActor>(this)](UGeoComponent *) {
            if (!actor.IsValid())
                return;
            actor->setupRenderer();
        });
    VolumeComponent->OnVolumeDataChanged.AddLambda(
        [actor = TWeakObjectPtr<ADVRActor>(this)](UVolumeDataComponent *) {
            if (!actor.IsValid())
                return;
            actor->setupRenderer();
        });
}

void ADVRActor::setupRenderer() {
    if (!renderer.IsValid()) {
        renderer = MakeShared<FDVRRenderer>();
        renderer->Register();
    }

    renderer->SetGeographicalParameters({.LongtitudeRange = GeoComponent->LongtitudeRange,
                                         .LatitudeRange = GeoComponent->LatitudeRange,
                                         .HeightRange = GeoComponent->HeightRange,
                                         .GeoRef = GeoComponent->GeoRef.Get()});
    renderer->SetRenderParameters(
        {.MaxStepCount = MaxStepCount,
         .Step = Step,
         .RelativeLightness = RelativeLightness,
         .VolumeTexture = VolumeComponent->VolumeTexture.Get(),
         .TransferFunctionTexture = VolumeComponent->TransferFunctionTexture
                                        ? VolumeComponent->TransferFunctionTexture.Get()
                                        : VolumeComponent->DefaultTransferFunctionTexture.Get()});
}

void ADVRActor::destroyRenderer() {
    if (!renderer.IsValid())
        return;

    renderer->Unregister();
    renderer.Reset();
}
