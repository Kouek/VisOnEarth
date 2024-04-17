// Author: Kouek Kou

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "CesiumGeoreference.h"

#include "Data.h"

#include "VolumeDataComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnVolumeDataChanged, UVolumeDataComponent *);

UCLASS()
class VIS4EARTH_API UVolumeDataComponent : public UActorComponent {
    GENERATED_BODY()

  public:
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    ESupportedVoxelType ImportVoxelType = VolumeData::Desc::DefVoxTy;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    FIntVector ImportVolumeTransformedAxis = VolumeData::Desc::DefAxis;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    FIntVector ImportVolumeDimension = VolumeData::Desc::DefDimension;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UVolumeTexture> VolumeTexture;
    UPROPERTY(VisibleAnywhere, Transient, Category = "VIS4Earth")
    TObjectPtr<UTexture2D> TransferFunctionTexture;
    UPROPERTY(VisibleAnywhere, Transient, Category = "VIS4Earth")
    TObjectPtr<UTexture2D> DefaultTransferFunctionTexture;
    UPROPERTY(VisibleAnywhere, Transient, Category = "VIS4Earth")
    TObjectPtr<UCurveLinearColor> TransferFunctionCurve;

    UFUNCTION(CallInEditor, Category = "VIS4Earth")
    void LoadRAWVolume();
    UFUNCTION(CallInEditor, Category = "VIS4Earth")
    void LoadTF();
    UFUNCTION(CallInEditor, Category = "VIS4Earth")
    void SaveTF();
    UFUNCTION(CallInEditor, Category = "VIS4Earth")
    void SyncTFCurveTexture();

    FOnVolumeDataChanged OnVolumeDataChanged;

    UVolumeDataComponent() { createDefaultTFTexture(); }

    void SetKeepVolumeInCPU(bool Keep) { keepVolumeInCPU = Keep; }
    const TArray<uint8> &GetVolumeCPUData() const { return volumeCPUData; }
    ESupportedVoxelType GetVolumeVoxelType() const { return prevVolumeDataDesc.VoxTy; }
    FIntVector GetVoxelPerVolume() const { return prevVolumeDataDesc.Dimension; }

    template <SupportedVoxelType T> T SampleVolumeCPUData(const FIntVector3 &Pos) {
        return *(reinterpret_cast<const T *>(volumeCPUData.GetData()) + Pos.Z * voxPerVolYxX +
                 Pos.Y * VolumeTexture->GetSizeX() + Pos.X);
    }

    void PostLoad() override {
        Super::PostLoad();
        createDefaultTFTexture();
    }


  private:
    static constexpr auto TFResolution = 256;
    static constexpr auto TFElemSz = sizeof(FFloat16) * 4;
    static constexpr auto TFMinTime = 0.f;
    static constexpr auto TFMaxTime = 255.f;
    static constexpr auto TFMinVal = 0.f;
    static constexpr auto TFMaxVal = 1.f;

    size_t voxPerVolYxX;
    bool keepVolumeInCPU = false;
    VolumeData::Desc prevVolumeDataDesc;
    TArray<uint8> volumeCPUData;
    std::map<float, FVector4f> tfPnts;

    void createDefaultTFTexture();
    void processError(const FString &ErrMsg);
};
