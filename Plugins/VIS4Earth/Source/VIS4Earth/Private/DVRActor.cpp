#include "DVRActor.h"

#include "EngineModule.h"
#include "ShaderParameterStruct.h"

#include "Runtime/Renderer/Private/SceneRendering.h"

#include "TFPreIntegrator.h"

ADVRActor::ADVRActor() {
    GeoComponent = CreateDefaultSubobject<UGeoComponent>(TEXT("Geographics"));
    VolumeComponent = CreateDefaultSubobject<UVolumeDataComponent>(TEXT("VolumeData"));

    generatePreIntegratedTF();
    setupRenderer();
    setupSignalsSlots();
}

void ADVRActor::setupSignalsSlots() {
    GeoComponent->OnGeographicsChanged.AddLambda([this](UGeoComponent *) { setupRenderer(); });
    VolumeComponent->OnVolumeDataChanged.AddLambda(
        [this](UVolumeDataComponent *) { setupRenderer(); });
    VolumeComponent->OnTransferFunctionDataChanged.AddLambda([this](UVolumeDataComponent *) {
        generatePreIntegratedTF();
        setupRenderer();
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
        {.UsePreIntegratedTF = UsePreIntegratedTF,
         .MaxStepCount = MaxStepCount,
         .Step = Step,
         .RelativeLightness = RelativeLightness,
         .VolumeTexture = VolumeComponent->VolumeTexture.Get(),
         .TransferFunctionTexture = UsePreIntegratedTF ? PreIntegratedTF.Get()
                                    : VolumeComponent->TransferFunctionTexture
                                        ? VolumeComponent->TransferFunctionTexture.Get()
                                        : VolumeComponent->DefaultTransferFunctionTexture.Get()});
}

void ADVRActor::destroyRenderer() {
    if (!renderer.IsValid())
        return;

    renderer->Unregister();
    renderer.Reset();
}

void ADVRActor::generatePreIntegratedTF() {
    if (!VolumeComponent->TransferFunctionTexture &&
        !VolumeComponent->DefaultTransferFunctionTexture)
        return;
    if (!UsePreIntegratedTF) {
        PreIntegratedTF = nullptr;
        setupRenderer();
        return;
    }

    auto tfDat = FTFPreIntegrator::Exec(
        {.TransferFunctionTexture = VolumeComponent->TransferFunctionTexture
                                        ? VolumeComponent->TransferFunctionTexture
                                        : VolumeComponent->DefaultTransferFunctionTexture});

    PreIntegratedTF = UTexture2D::CreateTransient(TransferFunctionData::Resolution,
                                                  TransferFunctionData::Resolution, PF_FloatRGBA);
    PreIntegratedTF->Filter = TextureFilter::TF_Bilinear;
    PreIntegratedTF->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
    PreIntegratedTF->AddressX = PreIntegratedTF->AddressY = TextureAddress::TA_Clamp;

    auto texDat = PreIntegratedTF->GetPlatformData()->Mips[0].BulkData.Lock(
        EBulkDataLockFlags::LOCK_READ_WRITE);
    FMemory::Memmove(texDat, tfDat.GetData(),
                     TransferFunctionData::ElemSz * TransferFunctionData::Resolution *
                         TransferFunctionData::Resolution);
    PreIntegratedTF->GetPlatformData()->Mips[0].BulkData.Unlock();
    PreIntegratedTF->UpdateResource();

    setupRenderer();
}
