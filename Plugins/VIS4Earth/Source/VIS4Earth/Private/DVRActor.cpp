#include "DVRActor.h"

#include <array>

#include "Components/CheckBox.h"
#include "Components/EditableText.h"
#include "Components/NamedSlot.h"
#include "EngineModule.h"
#include "ShaderParameterStruct.h"

#include "Runtime/Renderer/Private/SceneRendering.h"

#include "TFPreIntegrator.h"
#include "Util.h"

ADVRActor::ADVRActor() {
    GeoComponent = CreateDefaultSubobject<UGeoComponent>(TEXT("Geographics"));
    RootComponent = GeoComponent;

    VolumeComponent = CreateDefaultSubobject<UVolumeDataComponent>(TEXT("VolumeData"));

    UIComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("UI"));
    UIComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

    generatePreIntegratedTF();
    setupRenderer();
    setupSignalsSlots();
}

void ADVRActor::BeginPlay() {
    Super::BeginPlay();

    {
        auto realUIClass =
            LoadClass<UUserWidget>(nullptr, TEXT("WidgetBlueprint'/VIS4Earth/UI_DVR.UI_DVR_C'"));

        UIComponent->SetDrawAtDesiredSize(true);
        UIComponent->SetWidgetClass(realUIClass);
        UIComponent->SetWidgetSpace(EWidgetSpace::Screen);

        auto ui = UIComponent->GetUserWidgetObject();
        Cast<UCheckBox>(ui->GetWidgetFromName(TEXT("CheckBox_UsePreIntegratedTF")))
            ->SetCheckedState(UsePreIntegratedTF ? ECheckBoxState::Checked
                                                 : ECheckBoxState::Unchecked);
        Cast<UEditableText>(ui->GetWidgetFromName(TEXT("EditableText_Step")))
            ->SetText(FText::FromString(FString::SanitizeFloat(Step)));
        Cast<UEditableText>(ui->GetWidgetFromName(TEXT("EditableText_MaxStepCount")))
            ->SetText(FText::FromString(FString::FromInt(MaxStepCount)));
        Cast<UEditableText>(ui->GetWidgetFromName(TEXT("EditableText_RelativeLightness")))
            ->SetText(FText::FromString(FString::SanitizeFloat(RelativeLightness)));

        Cast<UNamedSlot>(ui->GetWidgetFromName(TEXT("NamedSlot_UI_VolumeCmpt")))
            ->SetContent(VolumeComponent->GetUI());
        Cast<UNamedSlot>(ui->GetWidgetFromName(TEXT("NamedSlot_UI_GeoCmpt")))
            ->SetContent(GeoComponent->GetUI());

        VIS4EARTH_UI_ADD_SLOT(ADVRActor, this, ui, CheckBox, UsePreIntegratedTF, CheckStateChanged);
        VIS4EARTH_UI_ADD_SLOT(ADVRActor, this, ui, EditableText, MaxStepCount, TextChanged);
        VIS4EARTH_UI_ADD_SLOT(ADVRActor, this, ui, EditableText, Step, TextChanged);
        VIS4EARTH_UI_ADD_SLOT(ADVRActor, this, ui, EditableText, RelativeLightness, TextChanged);
    }
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
