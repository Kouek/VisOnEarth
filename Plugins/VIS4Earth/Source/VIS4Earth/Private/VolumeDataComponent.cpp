#include "VolumeDataComponent.h"

#include <array>
#include <map>

#include "DesktopPlatformModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "VolumeSmoother.h"

void UVolumeDataComponent::LoadRAWVolume() {
    FJsonSerializableArray files;
    FDesktopPlatformModule::Get()->OpenFileDialog(
        nullptr, TEXT("Select a RAW Volume file"), FPaths::GetProjectFilePath(), TEXT(""),
        TEXT("Volume|*.raw;*.bin;*.RAW"), EFileDialogFlags::None, files);
    if (files.IsEmpty())
        return;

    auto volume = keepVolumeInCPU ? VolumeData::LoadFromFile({.VoxTy = ImportVoxelType,
                                                              .Axis = ImportVolumeTransformedAxis,
                                                              .Dimension = ImportVolumeDimension,
                                                              .FilePath = files[0]},
                                                             std::reference_wrapper(volumeCPUData))
                                  : VolumeData::LoadFromFile({.VoxTy = ImportVoxelType,
                                                              .Axis = ImportVolumeTransformedAxis,
                                                              .Dimension = ImportVolumeDimension,
                                                              .FilePath = files[0]});
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

    tfPnts.Empty();
    {
        std::array<const FRichCurve *, 4> curves = {
            &TransferFunctionCurve->FloatCurves[0], &TransferFunctionCurve->FloatCurves[1],
            &TransferFunctionCurve->FloatCurves[2], &TransferFunctionCurve->FloatCurves[3]};
        for (int32 i = 0; i < 4; ++i)
            for (auto itr = curves[i]->GetKeyIterator(); itr; ++itr) {
                auto pnt = tfPnts.Find(itr->Time);
                if (!pnt) {
                    auto legalTime = std::clamp(itr->Time, TFMinTime, TFMaxTime);
                    pnt = &tfPnts.Emplace(legalTime,
                                          TransferFunctionCurve->GetLinearColorValue(legalTime));
                }
                (*pnt)[i] = std::clamp(itr->Value, TFMinVal, TFMaxVal);
            }
    }

    TransferFunctionData::FromFlatArrayToTexture(
        TransferFunctionTexture.Get(), TransferFunctionData::LerpFromPointsToFlatArray(tfPnts));
    TransferFunctionData::FromPointsToCurve(TransferFunctionCurve.Get(), tfPnts);

    OnVolumeDataChanged.Broadcast(this);
}

void UVolumeDataComponent::generateSmoothedVolume() {
    if (!VolumeTexture || !keepSmoothedVolume)
        return;

    FVolumeSmoother::Exec(
        {.SmoothType = SmoothType,
         .SmoothDimension = SmoothDimension,
         .VolumeTexture = VolumeTexture,
         .FinishedCallback = [this](TSharedPtr<TArray<float>> VolDat) {
             VolumeTextureSmoothed = UVolumeTexture::CreateTransient(
                 VolumeTexture->GetSizeX(), VolumeTexture->GetSizeY(), VolumeTexture->GetSizeZ(),
                 EPixelFormat::PF_R32_FLOAT);
             VolumeTextureSmoothed->Filter = TextureFilter::TF_Trilinear;
             VolumeTextureSmoothed->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
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
