// Author: Kouek Kou

#pragma once

#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"

#include "CesiumGeoreference.h"

#include "GeoRenderer.h"

#include "GeoComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGeographicsChanged, UGeoComponent *);

UCLASS()
class VIS4EARTH_API UGeoComponent : public USceneComponent {
    GENERATED_BODY()

  public:
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    FVector2D LongtitudeRange = FGeoRenderer::GeoParameters::DefLongtitudeRange;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    FVector2D LatitudeRange = FGeoRenderer::GeoParameters::DefLatitudeRange;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    FVector2D HeightRange = FGeoRenderer::GeoParameters::DefHeightRange;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    TWeakObjectPtr<ACesiumGeoreference> GeoRef;

    UFUNCTION(BlueprintCallable, Category = "VIS4Earth")
    UStaticMesh *GenerateGeoMesh(int32 LongtitudeTessellation = 10,
                                 int32 LatitudeTessellation = 10);

    UUserWidget *GetUI() const { return ui.Get(); }

    UFUNCTION()
    void OnEditableText_LongtitudeRangeMinTextChanged(const FText &Text) {
        LongtitudeRange[0] = FCString::Atof(*Text.ToString());
        onGeographicsChanged();
    }
    UFUNCTION()
    void OnEditableText_LongtitudeRangeMaxTextChanged(const FText &Text) {
        LongtitudeRange[1] = FCString::Atof(*Text.ToString());
        onGeographicsChanged();
    }
    UFUNCTION()
    void OnEditableText_LatitudeRangeMinTextChanged(const FText &Text) {
        LatitudeRange[0] = FCString::Atof(*Text.ToString());
        onGeographicsChanged();
    }
    UFUNCTION()
    void OnEditableText_LatitudeRangeMaxTextChanged(const FText &Text) {
        LatitudeRange[1] = FCString::Atof(*Text.ToString());
        onGeographicsChanged();
    }
    UFUNCTION()
    void OnEditableText_HeightRangeMinTextChanged(const FText &Text) {
        HeightRange[0] = FCString::Atof(*Text.ToString());
        onGeographicsChanged();
    }
    UFUNCTION()
    void OnEditableText_HeightRangeMaxTextChanged(const FText &Text) {
        HeightRange[1] = FCString::Atof(*Text.ToString());
        onGeographicsChanged();
    }

    FOnGeographicsChanged OnGeographicsChanged;

  protected:
    virtual void BeginPlay() override;

  private:
    TObjectPtr<UUserWidget> ui;

    void checkAndCorrectParameters();
    void onGeographicsChanged();

    static void processError(const FString &ErrMsg);

#ifdef WITH_EDITOR
  public:
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent &PropChngedEv) override {
        Super::PostEditChangeProperty(PropChngedEv);

        if (!PropChngedEv.MemberProperty)
            return;

        auto name = PropChngedEv.MemberProperty->GetFName();
        if (name == GET_MEMBER_NAME_CHECKED(UGeoComponent, GeoRef) ||
            name == GET_MEMBER_NAME_CHECKED(UGeoComponent, LatitudeRange) ||
            name == GET_MEMBER_NAME_CHECKED(UGeoComponent, LongtitudeRange) ||
            name == GET_MEMBER_NAME_CHECKED(UGeoComponent, HeightRange)) {
            onGeographicsChanged();
            return;
        }
    }
#endif // WITH_EDITOR
};
