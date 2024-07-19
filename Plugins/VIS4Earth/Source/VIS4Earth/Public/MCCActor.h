// Author: Kouek Kou

#pragma once

#include <array>
#include <set>
#include <unordered_map>

#include "Components/WidgetComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"

#include "GeoComponent.h"
#include "VolumeDataComponent.h"

#include "MCCActor.generated.h"

UENUM()
enum class EMCCMeshSmoothType : uint8 {
    None = 0 UMETA(DisplayName = "None"),
    Laplacian UMETA(DisplayName = "Laplacian"),
    Curvature UMETA(DisplayName = "Curvature"),
};

/*
 * Class: AMCCActor
 * Function:
 * -- Implements Marching Cube Isosurface Generation and Rendering.
 */
UCLASS()
class VIS4EARTH_API AMCCActor : public AActor {
    GENERATED_BODY()

  public:
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    bool UseLerp = true;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    bool UseSmoothedVolume = false;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    EMCCMeshSmoothType MeshSmoothType = EMCCMeshSmoothType::None;
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    FIntVector2 HeightRange = {0, 0};
    UPROPERTY(EditAnywhere, Category = "VIS4Earth")
    float IsoValue = 0.f;

    UPROPERTY(VisibleAnywhere, Transient, Category = "VIS4Earth")
    TObjectPtr<UMaterialInstanceDynamic> MaterialInstanceDynamic;
    UPROPERTY(VisibleAnywhere, Transient, Category = "VIS4Earth")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UGeoComponent> GeoComponent;
    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UVolumeDataComponent> VolumeComponent;

    UPROPERTY(VisibleAnywhere, Category = "VIS4Earth")
    TObjectPtr<UWidgetComponent> UIComponent;
    UFUNCTION()
    void OnCheckBox_UseLerpCheckStateChanged(bool Checked) {
        UseLerp = Checked;
        marchingCube();
    }
    UFUNCTION()
    void OnCheckBox_UseSmoothedVolumeCheckStateChanged(bool Checked) {
        UseSmoothedVolume = Checked;
        marchingCube();
    }
    UFUNCTION()
    void OnComboBoxString_MeshSmoothTypeSelectionChanged(FString SelectedItem,
                                                         ESelectInfo::Type SelectionType);
    UFUNCTION()
    void OnEditableText_HeightRangeMinTextCommitted(const FText &Text, ETextCommit::Type Type) {
        HeightRange[0] = FCString::Atoi(*Text.ToString());
        marchingCube();
    }
    UFUNCTION()
    void OnEditableText_HeightRangeMaxTextCommitted(const FText &Text, ETextCommit::Type Type) {
        HeightRange[1] = FCString::Atoi(*Text.ToString());
        marchingCube();
    }
    UFUNCTION()
    void OnEditableText_IsoValueTextCommitted(const FText &Text, ETextCommit::Type Type) {
        IsoValue = FCString::Atof(*Text.ToString());
        marchingCube();
    }

    AMCCActor();

    virtual void PostLoad() override {
        Super::PostLoad();

        marchingCube();
    }

  protected:
    virtual void BeginPlay() override;

  private:
    enum class EMeshSectionIndex { Normal = 0, Smoothed = 1 };

    EMCCMeshSmoothType prevMeshSmoothType = EMCCMeshSmoothType::None;

    TArray<FVector> positions;
    TArray<FVector> normals;
    TArray<FVector> positionsSmoothed;
    TArray<FVector> normalsSmoothed;
    TArray<FVector2D> uvs;
    TArray<int32> indices;

    struct Edge {
        std::array<int32, 2> VertIDs;
        Edge(int32 V0, int32 V1) : VertIDs{V0, V1} {}

        struct Less {
            bool operator()(const Edge &E0, const Edge &E1) const {
                return std::less<decltype(VertIDs)>()(E0.VertIDs, E1.VertIDs);
            }
        };
    };
    std::set<Edge, Edge::Less> edges;

    void setupSignalsSlots();
    void checkAndCorrectParameters();
    void marchingCube();
    void updateMaterialInstanceDynamic();
    void emptyMesh();
    void updateMesh();
    void generateSmoothedMeshThenUpdateMesh(bool ShouldReGen = false);

#if WITH_EDITOR
  public:
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent &PropChngedEv) override {
        Super::PostEditChangeProperty(PropChngedEv);

        if (!PropChngedEv.MemberProperty)
            return;

        auto name = PropChngedEv.MemberProperty->GetFName();
        if (name == GET_MEMBER_NAME_CHECKED(AMCCActor, UseLerp) ||
            name == GET_MEMBER_NAME_CHECKED(AMCCActor, UseSmoothedVolume) ||
            name == GET_MEMBER_NAME_CHECKED(AMCCActor, HeightRange) ||
            name == GET_MEMBER_NAME_CHECKED(AMCCActor, IsoValue)) {
            marchingCube();
            return;
        }
        if (name == GET_MEMBER_NAME_CHECKED(AMCCActor, MeshSmoothType)) {
            generateSmoothedMeshThenUpdateMesh();
            return;
        }
    }
#endif // WITH_EDITOR
};
