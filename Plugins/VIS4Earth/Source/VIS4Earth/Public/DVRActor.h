// Author: Kouek Kou

#pragma once

#include "Components/WidgetComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

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
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    int LongtitudeTessellation = FDVRRenderer::RenderParameters::DefTessellation.X;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    int LatitudeTessellation = FDVRRenderer::RenderParameters::DefTessellation.Y;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UGeoComponent> GeoComponent;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UVolumeDataComponent> VolumeComponent;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UTexture2D> PreIntegratedTF;

    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UWidgetComponent> UIComponent;
    UFUNCTION()
    void OnCheckBox_UsePreIntegratedTFCheckStateChanged(bool Checked) {
        UsePreIntegratedTF = Checked;
        generatePreIntegratedTF();
    }
    UFUNCTION()
    void OnEditableText_MaxStepCountTextCommitted(const FText &Text, ETextCommit::Type Type) {
        MaxStepCount = FCString::Atoi(*Text.ToString());
        setupRenderer();
    }
    UFUNCTION()
    void OnEditableText_StepTextCommitted(const FText &Text, ETextCommit::Type Type) {
        Step = FCString::Atof(*Text.ToString());
        setupRenderer();
    }
    UFUNCTION()
    void OnEditableText_RelativeLightnessTextCommitted(const FText &Text, ETextCommit::Type Type) {
        RelativeLightness = FCString::Atof(*Text.ToString());
        setupRenderer();
    }

    ADVRActor();
    ~ADVRActor() { destroyRenderer(); }

    virtual void PostLoad() override {
        Super::PostLoad();

        generatePreIntegratedTF();
        setupRenderer();
    }

    virtual void Destroyed() override {
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

#if WITH_EDITOR
  public:
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent &PropChngedEv) override {
        Super::PostEditChangeProperty(PropChngedEv);

        if (!PropChngedEv.MemberProperty)
            return;

        auto name = PropChngedEv.MemberProperty->GetFName();
        if (name == GET_MEMBER_NAME_CHECKED(ADVRActor, MaxStepCount) ||
            name == GET_MEMBER_NAME_CHECKED(ADVRActor, Step) ||
            name == GET_MEMBER_NAME_CHECKED(ADVRActor, RelativeLightness) ||
            name == GET_MEMBER_NAME_CHECKED(ADVRActor, LongtitudeTessellation) ||
            name == GET_MEMBER_NAME_CHECKED(ADVRActor, LatitudeTessellation)) {
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
