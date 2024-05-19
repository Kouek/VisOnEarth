#include "MCSActor.h"

#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/NamedSlot.h"

void AMCSActor::OnComboBoxString_LineStyleSelectionChanged(FString SelectedItem,
                                                           ESelectInfo::Type SelectionType) {
    auto enumClass = StaticEnum<EMCSLineStyle>();
    for (int32 i = 0; i < enumClass->GetMaxEnumValue(); ++i)
        if (enumClass->GetNameByIndex(i) == SelectedItem) {
            LineStyle = static_cast<EMCSLineStyle>(i);
            break;
        }

    setupRenderer();
}

AMCSActor::AMCSActor() {
    GeoComponent = CreateDefaultSubobject<UGeoComponent>(TEXT("Geographics"));
    RootComponent = GeoComponent;

    VolumeComponent = CreateDefaultSubobject<UVolumeDataComponent>(TEXT("VolumeData"));
    VolumeComponent->SetKeepVolumeInCPU(true);
    VolumeComponent->SetKeepSmoothedVolume(true);

    UIComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("UI"));
    UIComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

    setupRenderer();
    setupSignalsSlots();
}

void AMCSActor::BeginPlay() {
    Super::BeginPlay();
    {
        auto realUIClass =
            LoadClass<UUserWidget>(nullptr, TEXT("WidgetBlueprint'/VIS4Earth/UI_MCS.UI_MCS_C'"));

        UIComponent->SetDrawAtDesiredSize(true);
        UIComponent->SetWidgetClass(realUIClass);
        UIComponent->SetWidgetSpace(EWidgetSpace::Screen);

        auto ui = UIComponent->GetUserWidgetObject();
        VIS4EARTH_UI_COMBOBOXSTRING_ADD_OPTIONS(ui, LineStyle);

        Cast<UComboBoxString>(ui->GetWidgetFromName(TEXT("ComboBoxString_LineStyle")))
            ->SetSelectedIndex(static_cast<int32>(LineStyle));
        Cast<UCheckBox>(ui->GetWidgetFromName(TEXT("CheckBox_UseLerp")))
            ->SetCheckedState(UseLerp ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
        Cast<UCheckBox>(ui->GetWidgetFromName(TEXT("CheckBox_UseSmoothedVolume")))
            ->SetCheckedState(UseSmoothedVolume ? ECheckBoxState::Checked
                                                : ECheckBoxState::Unchecked);
        Cast<UEditableText>(ui->GetWidgetFromName(TEXT("EditableText_HeightRangeMin")))
            ->SetText(FText::FromString(FString::FromInt(HeightRange[0])));
        Cast<UEditableText>(ui->GetWidgetFromName(TEXT("EditableText_HeightRangeMax")))
            ->SetText(FText::FromString(FString::FromInt(HeightRange[1])));
        Cast<UEditableText>(ui->GetWidgetFromName(TEXT("EditableText_IsoValue")))
            ->SetText(FText::FromString(FString::SanitizeFloat(IsoValue)));

        Cast<UNamedSlot>(ui->GetWidgetFromName(TEXT("NamedSlot_UI_VolumeCmpt")))
            ->SetContent(VolumeComponent->GetUI());
        Cast<UNamedSlot>(ui->GetWidgetFromName(TEXT("NamedSlot_UI_GeoCmpt")))
            ->SetContent(GeoComponent->GetUI());

        VIS4EARTH_UI_ADD_SLOT(AMCSActor, this, ui, ComboBoxString, LineStyle, SelectionChanged);
        VIS4EARTH_UI_ADD_SLOT(AMCSActor, this, ui, CheckBox, UseLerp, CheckStateChanged);
        VIS4EARTH_UI_ADD_SLOT(AMCSActor, this, ui, CheckBox, UseSmoothedVolume, CheckStateChanged);
        VIS4EARTH_UI_ADD_SLOT(AMCSActor, this, ui, EditableText, HeightRangeMin, TextChanged);
        VIS4EARTH_UI_ADD_SLOT(AMCSActor, this, ui, EditableText, HeightRangeMax, TextChanged);
        VIS4EARTH_UI_ADD_SLOT(AMCSActor, this, ui, EditableText, IsoValue, TextChanged);
    }
}

void AMCSActor::setupSignalsSlots() {
    GeoComponent->OnGeographicsChanged.AddLambda([this](UGeoComponent *) { setupRenderer(); });
    VolumeComponent->OnTransferFunctionDataChanged.AddLambda(
        [this](UVolumeDataComponent *) { setupRenderer(true); });
    VolumeComponent->OnVolumeDataChanged.AddLambda(
        [this](UVolumeDataComponent *) { setupRenderer(true); });
}

void AMCSActor::checkAndCorrectParameters() {
    if (!VolumeComponent->VolumeTexture)
        return;

    {
        auto [vxMin, vxMax, vxExt] =
            VolumeData::GetVoxelMinMaxExtent(VolumeComponent->GetVolumeVoxelType());
        if (IsoValue < vxMin)
            IsoValue = vxMin;
        if (IsoValue > vxMax)
            IsoValue = vxMax;
    }

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
