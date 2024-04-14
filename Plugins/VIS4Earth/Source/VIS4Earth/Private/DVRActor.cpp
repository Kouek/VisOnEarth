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
    GeoComponent->OnGeographicsChanged.AddLambda([&](UGeoComponent *) { setupRenderer(); });
    VolumeComponent->OnVolumeDataChanged.AddLambda(
        [&](UVolumeDataComponent *) { setupRenderer(); });
}

void ADVRActor::setupRenderer() {
    if (!renderer.IsValid()) {
        renderer = MakeShared<FDVRRenderer>();

        ENQUEUE_RENDER_COMMAND(RegisterRenderScreenRenderer)
        ([renderer = this->renderer](FRHICommandListImmediate &RHICmdList) {
            renderer->Register();
        });
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

    ENQUEUE_RENDER_COMMAND(UnregisterRenderScreenRenderer)
    ([renderer = this->renderer](FRHICommandListImmediate &RHICmdList) { renderer->Unregister(); });
    renderer.Reset();
}
