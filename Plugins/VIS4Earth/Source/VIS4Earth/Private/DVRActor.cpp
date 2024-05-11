#include "DVRActor.h"

#include <array>

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableText.h"
#include "EngineModule.h"
#include "ShaderParameterStruct.h"

#include "Runtime/Renderer/Private/SceneRendering.h"

#include "TFPreIntegrator.h"

void ADVRActor::OnButtonClicked_LoadRAWVolume() {
    fromUIToMembers();
    VolumeComponent->LoadRAWVolume();
}

void ADVRActor::OnButtonClicked_LoadTransferFunction() { VolumeComponent->LoadTF(); }

void ADVRActor::OnCheckBoxStateChanged_UsePreIntegratedTF(bool Checked) {
    UsePreIntegratedTF = Checked;
    generatePreIntegratedTF();
}

ADVRActor::ADVRActor() {
    GeoComponent = CreateDefaultSubobject<UGeoComponent>(TEXT("Geographics"));
    RootComponent = GeoComponent;

    VolumeComponent = CreateDefaultSubobject<UVolumeDataComponent>(TEXT("VolumeData"));

    UI = CreateDefaultSubobject<UWidgetComponent>(TEXT("UI"));
    UI->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

    generatePreIntegratedTF();
    setupRenderer();
    setupSignalsSlots();
}

void ADVRActor::BeginPlay() {
    Super::BeginPlay();

    {
        auto realUIClass =
            LoadClass<UUserWidget>(nullptr, TEXT("WidgetBlueprint'/VIS4Earth/UI_DVR.UI_DVR_C'"));

        UI->SetDrawAtDesiredSize(true);
        UI->SetWidgetClass(realUIClass);
        UI->SetWidgetSpace(EWidgetSpace::Screen);

        fromMembersToUI();

#define ADD_SLOT(Name)                                                                             \
    {                                                                                              \
        auto btn = Cast<UButton>(                                                                  \
            UI->GetUserWidgetObject()->GetWidgetFromName(TEXT("Button_") TEXT(#Name)));            \
        btn->OnClicked.AddDynamic(this, &ADVRActor::OnButtonClicked_##Name);                       \
    }
        ADD_SLOT(LoadRAWVolume)
        ADD_SLOT(LoadTransferFunction)
#undef ADD_SLOT
#define ADD_SLOT(Name)                                                                             \
    {                                                                                              \
        auto checkBox = Cast<UCheckBox>(                                                           \
            UI->GetUserWidgetObject()->GetWidgetFromName(TEXT("CheckBox_") TEXT(#Name)));          \
        checkBox->OnCheckStateChanged.AddDynamic(this, &ADVRActor::OnCheckBoxStateChanged_##Name); \
    }
        ADD_SLOT(UsePreIntegratedTF)
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

struct NamesInUI {
    static constexpr std::array ImportedVolumeDim = {TEXT("EditableText_ImportedVolumeDimX"),
                                                     TEXT("EditableText_ImportedVolumeDimY"),
                                                     TEXT("EditableText_ImportedVolumeDimZ")};
};

void ADVRActor::fromUIToMembers() {
    for (int32 i = 0; i < 3; ++i)
        VolumeComponent->ImportVolumeDimension[i] =
            FCString::Atoi(*Cast<UEditableText>(UI->GetUserWidgetObject()->GetWidgetFromName(
                                                    NamesInUI::ImportedVolumeDim[i]))
                                ->GetText()
                                .ToString());
}

void ADVRActor::fromMembersToUI() {
    for (int32 i = 0; i < 3; ++i)
        Cast<UEditableText>(
            UI->GetUserWidgetObject()->GetWidgetFromName(NamesInUI::ImportedVolumeDim[i]))
            ->SetText(
                FText::FromString(FString::FromInt(VolumeComponent->ImportVolumeDimension[i])));

    Cast<UCheckBox>(
        UI->GetUserWidgetObject()->GetWidgetFromName(TEXT("CheckBox_UsePreIntegratedTF")))
        ->SetCheckedState(UsePreIntegratedTF ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}
