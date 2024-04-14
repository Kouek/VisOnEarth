#include "VolumeDataComponent.h"

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

void UVolumeDataComponent::createDefaultTFTexture() {
    if (DefaultTransferFunctionTexture)
        return;

    constexpr int Resolution = 256;
    constexpr auto ElemSz = sizeof(FFloat16) * 4;

    TArray<FFloat16> dat;
    {
        dat.Reserve(Resolution * 4);
        for (int scalar = 0; scalar < Resolution; ++scalar) {
            auto a = 1.f * scalar / (Resolution - 1);
            dat.Emplace(a);
            dat.Emplace(1.f - std::abs(2.f * a - 1.f));
            dat.Emplace(1.f - a);
            dat.Emplace(a);
        }
    }

    DefaultTransferFunctionTexture = UTexture2D::CreateTransient(Resolution, 1, PF_FloatRGBA);
    DefaultTransferFunctionTexture->Filter = TextureFilter::TF_Bilinear;
    DefaultTransferFunctionTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
    DefaultTransferFunctionTexture->AddressX = DefaultTransferFunctionTexture->AddressY =
        TextureAddress::TA_Clamp;

    auto *texDat = DefaultTransferFunctionTexture->GetPlatformData()->Mips[0].BulkData.Lock(
        EBulkDataLockFlags::LOCK_READ_WRITE);
    FMemory::Memmove(texDat, dat.GetData(), ElemSz * Resolution);
    DefaultTransferFunctionTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
    DefaultTransferFunctionTexture->UpdateResource();
}

void UVolumeDataComponent::fromTFCurveToTexture() {}

void UVolumeDataComponent::processError(const FString &ErrMsg) {
    FNotificationInfo info(FText::FromString(ErrMsg));

    auto notifyItem = FSlateNotificationManager::Get().AddNotification(info);
    notifyItem->SetCompletionState(SNotificationItem::ECompletionState::CS_Fail);
    notifyItem->ExpireAndFadeout();
}
