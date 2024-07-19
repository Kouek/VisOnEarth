// Author: Kouek Kou

#pragma once

#include "Components/ActorComponent.h"
#include "Components/WidgetComponent.h"
#include "CoreMinimal.h"

#include "CesiumGeoreference.h"

#include "Data.h"

#include "VolumeDataComponent.generated.h"

UCLASS()
class VIS4EARTH_API UVolumeDataComponent : public UActorComponent {
    GENERATED_BODY()

  public:
    UPROPERTY(EditAnywhere, Category = "VIS4Earth|Smooth")
    EVolumeSmoothType VolumeSmoothType = EVolumeSmoothType::Max;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth|Smooth")
    EVolumeSmoothDimension VolumeSmoothDimension = EVolumeSmoothDimension::XYZ;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    ESupportedVoxelType ImportVoxelType = VolumeData::LoadFromFileDesc::DefVoxTy;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    FIntVector ImportVolumeTransformedAxis = VolumeData::LoadFromFileDesc::DefAxis;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    FIntVector ImportVolumeDimension = VolumeData::LoadFromFileDesc::DefDimension;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UVolumeTexture> VolumeTexture;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UVolumeTexture> VolumeTextureSmoothed;
    UPROPERTY(VisibleAnywhere, Transient, Category = "VIS4Earth")
    TObjectPtr<UTexture2D> TransferFunctionTexture;
    UPROPERTY(VisibleAnywhere, Transient, Category = "VIS4Earth")
    TObjectPtr<UTexture2D> DefaultTransferFunctionTexture;
    UPROPERTY(VisibleAnywhere, Transient, Category = "VIS4Earth")
    TObjectPtr<UCurveLinearColor> TransferFunctionCurve;

    UFUNCTION()
    void OnComboBoxString_VolumeSmoothTypeSelectionChanged(FString SelectedItem,
                                                           ESelectInfo::Type SelectionType);
    UFUNCTION()
    void OnComboBoxString_VolumeSmoothDimensionSelectionChanged(FString SelectedItem,
                                                                ESelectInfo::Type SelectionType);
    UFUNCTION()
    void OnButton_LoadRAWVolumeClicked();
    UFUNCTION()
    void OnButton_LoadTFClicked();

    UFUNCTION(CallInEditor, Category = "VIS4Earth")
    void LoadRAWVolume();
    UFUNCTION(CallInEditor, Category = "VIS4Earth")
    void LoadTF();
    UFUNCTION(CallInEditor, Category = "VIS4Earth")
    void SaveTF();
    UFUNCTION(CallInEditor, Category = "VIS4Earth")
    void SyncTFCurveTexture();

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnVolumeDataChanged, UVolumeDataComponent *);
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnTransferFunctionDataChanged, UVolumeDataComponent *);

    FOnVolumeDataChanged OnVolumeDataChanged;
    FOnTransferFunctionDataChanged OnTransferFunctionDataChanged;

    UVolumeDataComponent();

    UUserWidget *GetUI() const { return ui.Get(); }

    void SetKeepVolumeInCPU(bool Keep) {
        keepVolumeInCPU = Keep;
        generateSmoothedVolume();
    }
    void SetKeepSmoothedVolume(bool Keep) {
        keepSmoothedVolume = Keep;
        generateSmoothedVolume();
    }

    const TArray<uint8> &GetVolumeCPUData() const { return volumeCPUData; }
    ESupportedVoxelType GetVolumeVoxelType() const { return prevVolumeDataDesc.VoxTy; }
    FIntVector GetVoxelPerVolume() const { return prevVolumeDataDesc.Dimension; }

    template <SupportedVoxelType T> T SampleVolumeCPUData(const FIntVector3 &Pos) {
        return *(reinterpret_cast<const T *>(volumeCPUData.GetData()) + Pos.Z * voxPerVolYxX +
                 Pos.Y * VolumeTexture->GetSizeX() + Pos.X);
    }
    float SampleVolumeCPUDataSmoothed(const FIntVector3 &Pos) {
        return *(reinterpret_cast<const float *>(volumeCPUDataSmoothed.GetData()) +
                 Pos.Z * voxPerVolYxX + Pos.Y * VolumeTexture->GetSizeX() + Pos.X);
    }

    virtual void PostLoad() override {
        Super::PostLoad();
        createDefaultTFTexture();
    }

  protected:
    virtual void BeginPlay() override;

  private:
    size_t voxPerVolYxX;
    bool keepVolumeInCPU = false;
    bool keepSmoothedVolume = false;
    VolumeData::LoadFromFileDesc prevVolumeDataDesc;

    TObjectPtr<UUserWidget> ui;

    TArray<uint8> volumeCPUData;
    TArray<float> volumeCPUDataSmoothed;
    TMap<float, FVector4f> tfPnts;

    void generateSmoothedVolume();
    void generatePreIntegratedTF();
    void createDefaultTFTexture();

    static void processError(const FString &ErrMsg);

#if WITH_EDITOR
  public:
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent &PropChngedEv) override {
        Super::PostEditChangeProperty(PropChngedEv);

        if (!PropChngedEv.MemberProperty)
            return;

        auto name = PropChngedEv.MemberProperty->GetFName();
        if (name == GET_MEMBER_NAME_CHECKED(UVolumeDataComponent, VolumeSmoothType) ||
            name == GET_MEMBER_NAME_CHECKED(UVolumeDataComponent, VolumeSmoothDimension)) {
            generateSmoothedVolume();
            return;
        }
    }
#endif // WITH_EDITOR
};
