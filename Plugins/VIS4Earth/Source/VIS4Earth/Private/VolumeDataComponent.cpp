#include "VolumeDataComponent.h"

#include <array>
#include <map>

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "DesktopPlatformModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "VolumeSmoother.h"

struct VolumeDataComponentNamesInUI {
    static constexpr std::array ImportVolumeDim = {TEXT("EditableText_ImportVolumeDimX"),
                                                   TEXT("EditableText_ImportVolumeDimY"),
                                                   TEXT("EditableText_ImportVolumeDimZ")};
    static constexpr std::array ImportVolumeTransformedAxis = {
        TEXT("EditableText_ImportVolumeTransformedAxisX"),
        TEXT("EditableText_ImportVolumeTransformedAxisY"),
        TEXT("EditableText_ImportVolumeTransformedAxisZ")};
};

void UVolumeDataComponent::OnComboBoxString_VolumeSmoothTypeSelectionChanged(
    FString SelectedItem, ESelectInfo::Type SelectionType) {
    auto enumClass = StaticEnum<EVolumeSmoothType>();
    for (int32 i = 0; i < enumClass->GetMaxEnumValue(); ++i)
        if (enumClass->GetNameByIndex(i) == SelectedItem) {
            VolumeSmoothType = static_cast<EVolumeSmoothType>(i);
            break;
        }

    generateSmoothedVolume();
}

void UVolumeDataComponent::OnComboBoxString_VolumeSmoothDimensionSelectionChanged(
    FString SelectedItem, ESelectInfo::Type SelectionType) {
    auto enumClass = StaticEnum<EVolumeSmoothDimension>();
    for (int32 i = 0; i < enumClass->GetMaxEnumValue(); ++i)
        if (enumClass->GetNameByIndex(i) == SelectedItem) {
            VolumeSmoothDimension = static_cast<EVolumeSmoothDimension>(i);
            break;
        }

    generateSmoothedVolume();
}

void UVolumeDataComponent::OnButton_LoadRAWVolumeClicked() {
    ImportVoxelType = static_cast<ESupportedVoxelType>(
        Cast<UComboBoxString>(ui->GetWidgetFromName(TEXT("ComboBoxString_ImportVoxelType")))
            ->GetSelectedIndex());
    VolumeSmoothType = static_cast<EVolumeSmoothType>(
        Cast<UComboBoxString>(ui->GetWidgetFromName(TEXT("ComboBoxString_VolumeSmoothType")))
            ->GetSelectedIndex());
    VolumeSmoothDimension = static_cast<EVolumeSmoothDimension>(
        Cast<UComboBoxString>(ui->GetWidgetFromName(TEXT("ComboBoxString_VolumeSmoothDimension")))
            ->GetSelectedIndex());

    for (int32 i = 0; i < 3; ++i) {
        ImportVolumeTransformedAxis[i] = FCString::Atoi(
            *Cast<UEditableText>(ui->GetWidgetFromName(
                                     VolumeDataComponentNamesInUI::ImportVolumeTransformedAxis[i]))
                 ->GetText()
                 .ToString());
        ImportVolumeDimension[i] = FCString::Atoi(
            *Cast<UEditableText>(
                 ui->GetWidgetFromName(VolumeDataComponentNamesInUI::ImportVolumeDim[i]))
                 ->GetText()
                 .ToString());
    }
    LoadRAWVolume();
}

void UVolumeDataComponent::OnButton_LoadTFClicked() { LoadTF(); }

void UVolumeDataComponent::LoadRAWVolume() {
    FJsonSerializableArray files;
    FDesktopPlatformModule::Get()->OpenFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Select a RAW Volume file"), FPaths::GetProjectFilePath(), TEXT(""),
        TEXT("Volume|*.raw;*.bin;*.RAW"), EFileDialogFlags::None, files);
    if (files.IsEmpty())
        return;

    auto volume = keepVolumeInCPU ? VolumeData::LoadFromFile({.VoxTy = ImportVoxelType,
                                                              .Axis = ImportVolumeTransformedAxis,
                                                              .Dimension = ImportVolumeDimension,
                                                              .FilePath = {files[0]}},
                                                             std::reference_wrapper(volumeCPUData))
                                  : VolumeData::LoadFromFile({.VoxTy = ImportVoxelType,
                                                              .Axis = ImportVolumeTransformedAxis,
                                                              .Dimension = ImportVolumeDimension,
                                                              .FilePath = {files[0]}});
    if (volume.IsType<FString>()) {
        auto &errMsg = volume.Get<FString>();
        processError(errMsg);
        return;
    }

    VolumeTexture = volume.Get<UVolumeTexture *>();
    voxPerVolYxX = static_cast<size_t>(ImportVolumeDimension.X) * ImportVolumeDimension.Y;
    prevVolumeDataDesc.VoxTy = ImportVoxelType;
    prevVolumeDataDesc.Dimension = ImportVolumeDimension;

    generateSmoothedVolume();

    OnVolumeDataChanged.Broadcast(this);
}

void UVolumeDataComponent::LoadTF() {
    FJsonSerializableArray files;
    FDesktopPlatformModule::Get()->OpenFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Select a Transfer Function file"), FPaths::GetProjectFilePath(), TEXT(""),
        TEXT("TF|*.txt"), EFileDialogFlags::None, files);
    if (files.IsEmpty())
        return;

    auto tf = TransferFunctionData::LoadFromFile({.FilePath = {files[0]}});
    if (tf.IsType<FString>()) {
        auto &errMsg = tf.Get<FString>();
        processError(errMsg);

        return;
    }

    auto &[tex, curve] = tf.Get<TTuple<UTexture2D *, UCurveLinearColor *>>();
    TransferFunctionTexture = tex;
    TransferFunctionCurve = curve;

    OnTransferFunctionDataChanged.Broadcast(this);
}

void UVolumeDataComponent::SaveTF() {
    FJsonSerializableArray files;
    FDesktopPlatformModule::Get()->SaveFileDialog(nullptr, TEXT("Select a Transfer Function file"),
                                                  FPaths::GetProjectFilePath(), TEXT("xx_tf.txt"),
                                                  TEXT("TF|*.txt"), EFileDialogFlags::None, files);
    if (files.IsEmpty())
        return;

    auto errMsg = TransferFunctionData::SaveToFile(TransferFunctionCurve, FFilePath(files[0]));
    if (errMsg.IsSet())
        processError(errMsg.GetValue());
}

void UVolumeDataComponent::SyncTFCurveTexture() {
    if (!TransferFunctionTexture || !TransferFunctionCurve)
        return;

    tfPnts.Empty();
    {
        std::array<const FRichCurve *, 4> curves = {
            &TransferFunctionCurve->FloatCurves[0], &TransferFunctionCurve->FloatCurves[1],
            &TransferFunctionCurve->FloatCurves[2], &TransferFunctionCurve->FloatCurves[3]};
        for (int32 i = 0; i < 4; ++i)
            for (auto itr = curves[i]->GetKeyIterator(); itr; ++itr) {
                auto pnt = tfPnts.Find(itr->Time);
                if (!pnt) {
                    auto legalTime = std::clamp(
                        itr->Time, 0.f, static_cast<float>(TransferFunctionData::Resolution - 1));
                    pnt = &tfPnts.Emplace(legalTime,
                                          TransferFunctionCurve->GetLinearColorValue(legalTime));
                }
                (*pnt)[i] = std::clamp(itr->Value, 0.f,
                                       static_cast<float>(TransferFunctionData::Resolution - 1));
            }
    }

    TransferFunctionData::FromFlatArrayToTexture(
        TransferFunctionTexture.Get(), TransferFunctionData::LerpFromPointsToFlatArray(tfPnts));
    TransferFunctionData::FromPointsToCurve(TransferFunctionCurve.Get(), tfPnts);

    OnTransferFunctionDataChanged.Broadcast(this);
}

UVolumeDataComponent::UVolumeDataComponent() { createDefaultTFTexture(); }

void UVolumeDataComponent::BeginPlay() {
    Super::BeginPlay();

    {
        auto realUIClass = LoadClass<UUserWidget>(
            nullptr, TEXT("WidgetBlueprint'/VIS4Earth/UI_VolumeCmpt.UI_VolumeCmpt_C'"));
        ui = CreateWidget(GetWorld(), realUIClass);

        for (int32 i = 0; i < 3; ++i) {
            Cast<UEditableText>(
                ui->GetWidgetFromName(VolumeDataComponentNamesInUI::ImportVolumeTransformedAxis[i]))
                ->SetText(FText::FromString(FString::FromInt(ImportVolumeTransformedAxis[i])));
            Cast<UEditableText>(
                ui->GetWidgetFromName(VolumeDataComponentNamesInUI::ImportVolumeDim[i]))
                ->SetText(FText::FromString(FString::FromInt(ImportVolumeDimension[i])));
        }

        VIS4EARTH_UI_COMBOBOXSTRING_ADD_OPTIONS(ui, ImportVoxelType);
        VIS4EARTH_UI_COMBOBOXSTRING_ADD_OPTIONS(ui, VolumeSmoothType);
        VIS4EARTH_UI_COMBOBOXSTRING_ADD_OPTIONS(ui, VolumeSmoothDimension);

        Cast<UComboBoxString>(ui->GetWidgetFromName(TEXT("ComboBoxString_ImportVoxelType")))
            ->SetSelectedIndex(static_cast<int32>(ImportVoxelType));
        Cast<UComboBoxString>(ui->GetWidgetFromName(TEXT("ComboBoxString_VolumeSmoothType")))
            ->SetSelectedIndex(static_cast<int32>(VolumeSmoothType));
        Cast<UComboBoxString>(ui->GetWidgetFromName(TEXT("ComboBoxString_VolumeSmoothDimension")))
            ->SetSelectedIndex(static_cast<int32>(VolumeSmoothDimension));

        VIS4EARTH_UI_ADD_SLOT(UVolumeDataComponent, this, ui, Button, LoadRAWVolume, Clicked);
        VIS4EARTH_UI_ADD_SLOT(UVolumeDataComponent, this, ui, Button, LoadTF, Clicked);
        VIS4EARTH_UI_ADD_SLOT(UVolumeDataComponent, this, ui, ComboBoxString, VolumeSmoothType,
                              SelectionChanged);
        VIS4EARTH_UI_ADD_SLOT(UVolumeDataComponent, this, ui, ComboBoxString, VolumeSmoothDimension,
                              SelectionChanged);
    }
}

void UVolumeDataComponent::generateSmoothedVolume() {
    if (!VolumeTexture)
        return;
    if (!keepSmoothedVolume) {
        VolumeTextureSmoothed = nullptr;
        return;
    }

    FVolumeSmoother::Exec(
        {.SmoothType = VolumeSmoothType,
         .SmoothDimension = VolumeSmoothDimension,
         .VolumeTexture = VolumeTexture,
         .FinishedCallback = [this](TSharedPtr<TArray<float>> VolDat) {
             VolumeTextureSmoothed = UVolumeTexture::CreateTransient(
                 VolumeTexture->GetSizeX(), VolumeTexture->GetSizeY(), VolumeTexture->GetSizeZ(),
                 EPixelFormat::PF_R32_FLOAT);
             VolumeTextureSmoothed->Filter = TextureFilter::TF_Trilinear;
             VolumeTextureSmoothed->AddressMode = TextureAddress::TA_Clamp;

             auto texDat = VolumeTextureSmoothed->GetPlatformData()->Mips[0].BulkData.Lock(
                 EBulkDataLockFlags::LOCK_READ_WRITE);
             FMemory::Memcpy(texDat, VolDat->GetData(), sizeof(float) * VolDat->Num());
             VolumeTextureSmoothed->GetPlatformData()->Mips[0].BulkData.Unlock();

             VolumeTextureSmoothed->UpdateResource();

             if (keepVolumeInCPU)
                 volumeCPUDataSmoothed = std::move(*VolDat);

             OnVolumeDataChanged.Broadcast(this);
         }});
}

void UVolumeDataComponent::createDefaultTFTexture() {
    if (DefaultTransferFunctionTexture)
        return;

    TArray<FFloat16> dat;
    {
        dat.Reserve(TransferFunctionData::Resolution * 4);
        for (int scalar = 0; scalar < TransferFunctionData::Resolution; ++scalar) {
            auto a = 1.f * scalar / (TransferFunctionData::Resolution - 1);
            dat.Emplace(a);
            dat.Emplace(1.f - std::abs(2.f * a - 1.f));
            dat.Emplace(1.f - a);
            dat.Emplace(a);
        }
    }
    DefaultTransferFunctionTexture = TransferFunctionData::FromFlatArrayToTexture(dat);

    OnTransferFunctionDataChanged.Broadcast(this);
}

void UVolumeDataComponent::processError(const FString &ErrMsg) {
    FNotificationInfo info(FText::FromString(ErrMsg));

    auto notifyItem = FSlateNotificationManager::Get().AddNotification(info);
    notifyItem->SetCompletionState(SNotificationItem::ECompletionState::CS_Fail);
    notifyItem->ExpireAndFadeout();
}
