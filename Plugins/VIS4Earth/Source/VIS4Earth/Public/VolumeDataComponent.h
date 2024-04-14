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
    ESupportedVoxelType VoxelType = VolumeData::Desc::DefVoxTy;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    FIntVector VolumeTransformedAxis = VolumeData::Desc::DefAxis;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    FIntVector VolumeDimension = VolumeData::Desc::DefDimension;
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

    FOnVolumeDataChanged OnVolumeDataChanged;

    UVolumeDataComponent() { createDefaultTFTexture(); }

    void PostLoad() override {
        Super::PostLoad();
        createDefaultTFTexture();
    }

  private:
    VolumeData::Desc prevVolumeDataDesc;

    void createDefaultTFTexture();
    void fromTFCurveToTexture();
    void processError(const FString &ErrMsg);
};
