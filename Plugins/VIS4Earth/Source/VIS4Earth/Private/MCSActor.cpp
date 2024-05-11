#include "MCSActor.h"

AMCSActor::AMCSActor() {
    GeoComponent = CreateDefaultSubobject<UGeoComponent>(TEXT("Geographics"));
    RootComponent = GeoComponent;

    VolumeComponent = CreateDefaultSubobject<UVolumeDataComponent>(TEXT("VolumeData"));
    VolumeComponent->SetKeepVolumeInCPU(true);
    VolumeComponent->SetKeepSmoothedVolume(true);

    setupRenderer();
    setupSignalsSlots();
}

void AMCSActor::setupSignalsSlots() {
    GeoComponent->OnGeographicsChanged.AddLambda([this](UGeoComponent *) { setupRenderer(); });
    VolumeComponent->OnTransferFunctionDataChanged.AddLambda(
        [this](UVolumeDataComponent *) { setupRenderer(true); });
    VolumeComponent->OnVolumeDataChanged.AddLambda(
        [this](UVolumeDataComponent *) { setupRenderer(true); });
}

void AMCSActor::checkAndCorrectParameters() {
    {
        auto [vxMin, vxMax, vxExt] =
            VolumeData::GetVoxelMinMaxExtent(VolumeComponent->GetVolumeVoxelType());
        if (IsoValue < vxMin)
            IsoValue = vxMin;
        if (IsoValue > vxMax)
            IsoValue = vxMax;
    }

    if (!VolumeComponent->VolumeTexture)
        return;
    FIntVector3 voxPerVol(VolumeComponent->VolumeTexture->GetSizeX(),
                          VolumeComponent->VolumeTexture->GetSizeY(),
                          VolumeComponent->VolumeTexture->GetSizeZ());
    auto voxPerVolYxX = static_cast<size_t>(voxPerVol.Y) * voxPerVol.X;
    if (HeightRange[0] < 0)
        HeightRange[0] = 0;
    if (HeightRange[0] >= voxPerVol.Z)
        HeightRange[0] = voxPerVol.Z - 1;
    if (HeightRange[1] < HeightRange[0])
        HeightRange[1] = HeightRange[0];
    if (HeightRange[1] >= voxPerVol.Z)
        HeightRange[1] = voxPerVol.Z - 1;
}

void AMCSActor::setupRenderer(bool shouldMarchSquare) {
    checkAndCorrectParameters();

    if (!renderer.IsValid()) {
        renderer = MakeShared<FMCSRenderer>();
        renderer->Register();
    }

    renderer->SetGeographicalParameters({.LongtitudeRange = GeoComponent->LongtitudeRange,
                                         .LatitudeRange = GeoComponent->LatitudeRange,
                                         .HeightRange = GeoComponent->HeightRange,
                                         .GeoRef = GeoComponent->GeoRef.Get()});
    renderer->SetRenderParameters(
        {.LineStyle = LineStyle,
         .TransferFunctionTexture = VolumeComponent->TransferFunctionTexture
                                        ? VolumeComponent->TransferFunctionTexture.Get()
                                        : VolumeComponent->DefaultTransferFunctionTexture.Get()});

    if (shouldMarchSquare)
        renderer->MarchingSquare({.UseLerp = UseLerp,
                                  .UseSmoothedVolume = UseSmoothedVolume,
                                  .HeightRange = HeightRange,
                                  .IsoValue = IsoValue,
                                  .VolumeComponent = VolumeComponent});
}

void AMCSActor::destroyRenderer() {
    if (!renderer.IsValid())
        return;

    renderer->Unregister();
    renderer.Reset();
}
