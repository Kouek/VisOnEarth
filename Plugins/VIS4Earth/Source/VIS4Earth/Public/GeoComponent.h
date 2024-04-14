// Author: Kouek Kou

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"

#include "CesiumGeoreference.h"

#include "GeoRenderer.h"

#include "GeoComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGeographicsChanged, UGeoComponent *);

UCLASS()
class VIS4EARTH_API UGeoComponent : public UActorComponent {
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

    FOnGeographicsChanged OnGeographicsChanged;

  private:
    void checkAndCorrectParameters();
    void processError(const FString &ErrMsg);

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
            checkAndCorrectParameters();
            OnGeographicsChanged.Broadcast(this);
            return;
        }
    }
#endif // WITH_EDITOR
};
