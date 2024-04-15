#include "VolumeDataComponent.h"

#include <array>
#include <map>

#include "DesktopPlatformModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

void UVolumeDataComponent::LoadRAWVolume() {
    FJsonSerializableArray files;
    FDesktopPlatformModule::Get()->OpenFileDialog(
        nullptr, TEXT("Select a RAW Volume file"), FPaths::GetProjectFilePath(), TEXT(""),
        TEXT("Volume|*.raw;*.bin;*.RAW"), EFileDialogFlags::None, files);
    if (files.IsEmpty())
        return;

    auto volume = VolumeData::LoadFromFile({.LODNum = 2,
                                            .VoxTy = VoxelType,
                                            .Axis = VolumeTransformedAxis,
                                            .Dimension = VolumeDimension,
                                            .FilePath = files[0]});
    if (volume.IsType<FString>()) {
        auto &errMsg = volume.Get<FString>();
        processError(errMsg);

        VoxelType = prevVolumeDataDesc.VoxTy;
        VolumeDimension = prevVolumeDataDesc.Dimension;
        return;
    }

    VolumeTexture = volume.Get<UVolumeTexture *>();
    prevVolumeDataDesc.VoxTy = VoxelType;
    prevVolumeDataDesc.Dimension = VolumeDimension;

    OnVolumeDataChanged.Broadcast(this);
}

void UVolumeDataComponent::LoadTF() {
    FJsonSerializableArray files;
    FDesktopPlatformModule::Get()->OpenFileDialog(nullptr, TEXT("Select a Transfer Function file"),
                                                  FPaths::GetProjectFilePath(), TEXT(""),
                                                  TEXT("TF|*.txt"), EFileDialogFlags::None, files);
    if (files.IsEmpty())
        return;

    auto tf = TransferFunctionData::LoadFromFile({.FilePath = files[0]});
    if (tf.IsType<FString>()) {
        auto &errMsg = tf.Get<FString>();
        processError(errMsg);

        return;
    }

    auto &[tex, curve] = tf.Get<TTuple<UTexture2D *, UCurveLinearColor *>>();
    TransferFunctionTexture = tex;
    TransferFunctionCurve = curve;

    OnVolumeDataChanged.Broadcast(this);
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

    tfPnts.clear();
    {
        std::array<const FRichCurve *, 4> curves = {
            &TransferFunctionCurve->FloatCurves[0], &TransferFunctionCurve->FloatCurves[1],
            &TransferFunctionCurve->FloatCurves[2], &TransferFunctionCurve->FloatCurves[3]};
        for (int32 i = 0; i < 4; ++i)
            for (auto itr = curves[i]->GetKeyIterator(); itr; ++itr) {
                auto pnt = tfPnts.find(itr->Time);
                if (pnt == tfPnts.end()) {
                    auto legalTime = std::clamp(itr->Time, TFMinTime, TFMaxTime);
                    auto [_pnt, _] = tfPnts.emplace(
                        legalTime, TransferFunctionCurve->GetLinearColorValue(legalTime));
                    pnt = _pnt;
                }
                pnt->second[i] = std::clamp(itr->Value, TFMinVal, TFMaxVal);
            }
    }

    TransferFunctionData::FromFlatArrayToTexture(
        TransferFunctionTexture.Get(), TransferFunctionData::LerpFromPointsToFlatArray(tfPnts));
    TransferFunctionData::FromPointsToCurve(TransferFunctionCurve.Get(), tfPnts);

    OnVolumeDataChanged.Broadcast(this);
}

void UVolumeDataComponent::createDefaultTFTexture() {
    if (DefaultTransferFunctionTexture)
        return;

    TArray<FFloat16> dat;
    {
        dat.Reserve(TFResolution * 4);
        for (int scalar = 0; scalar < TFResolution; ++scalar) {
            auto a = 1.f * scalar / (TFResolution - 1);
            dat.Emplace(a);
            dat.Emplace(1.f - std::abs(2.f * a - 1.f));
            dat.Emplace(1.f - a);
            dat.Emplace(a);
        }
    }
    DefaultTransferFunctionTexture = TransferFunctionData::FromFlatArrayToTexture(dat);
}

void UVolumeDataComponent::processError(const FString &ErrMsg) {
    FNotificationInfo info(FText::FromString(ErrMsg));

    auto notifyItem = FSlateNotificationManager::Get().AddNotification(info);
    notifyItem->SetCompletionState(SNotificationItem::ECompletionState::CS_Fail);
    notifyItem->ExpireAndFadeout();
}
