// Author: Kouek Kou

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/WidgetComponent.h"

#include "GeoComponent.h"
#include "VolumeDataComponent.h"

#include "DVRRenderer.h"

#include "DVRActor.generated.h"

/*
 * Class: ADVRActor
 * Function:
 * -- Implements Time-Variable Direct Volume Rendering.
 */
UCLASS()
class VIS4EARTH_API ADVRActor : public AActor {
    GENERATED_BODY()

  public:
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    bool UsePreIntegratedTF = FDVRRenderer::RenderParameters::DefUsePreIntegratedTF;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    int MaxStepCount = FDVRRenderer::RenderParameters::DefMaxStepCount;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    float Step = FDVRRenderer::RenderParameters::DefStep;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    float RelativeLightness = FDVRRenderer::RenderParameters::DefRelativeLightness;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UGeoComponent> GeoComponent;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UVolumeDataComponent> VolumeComponent;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UTexture2D> PreIntegratedTF;

    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UWidgetComponent> UI;
    UFUNCTION()
    void OnButtonClicked_LoadRAWVolume();
    UFUNCTION()
    void OnButtonClicked_LoadTransferFunction();
    UFUNCTION()
    void OnCheckBoxStateChanged_UsePreIntegratedTF(bool Checked);

    ADVRActor();
    ~ADVRActor() { destroyRenderer(); }

    void PostLoad() override {
        Super::PostLoad();

        generatePreIntegratedTF();
        setupRenderer();
    }

    void Destroyed() override {
        Super::Destroyed();

        destroyRenderer();
    }

  protected:
    virtual void BeginPlay() override;

  private:
    TSharedPtr<FDVRRenderer> renderer;

    void setupSignalsSlots();
    void setupRenderer();
    void destroyRenderer();
    void generatePreIntegratedTF();

    void fromUIToMembers();
    void fromMembersToUI();

#ifdef WITH_EDITOR
  public:
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent &PropChngedEv) override {
        Super::PostEditChangeProperty(PropChngedEv);

        if (!PropChngedEv.MemberProperty)
            return;

        auto name = PropChngedEv.MemberProperty->GetFName();
        if (name == GET_MEMBER_NAME_CHECKED(ADVRActor, MaxStepCount) ||
            name == GET_MEMBER_NAME_CHECKED(ADVRActor, Step) ||
            name == GET_MEMBER_NAME_CHECKED(ADVRActor, RelativeLightness)) {
            setupRenderer();
            return;
        }

        if (name == GET_MEMBER_NAME_CHECKED(ADVRActor, UsePreIntegratedTF) ||
            name == GET_MEMBER_NAME_CHECKED(ADVRActor, Step) && PreIntegratedTF) {
            generatePreIntegratedTF();
            return;
        }
    }
#endif // WITH_EDITOR
};
