// Author: Kouek Kou

#pragma once

#include <array>
#include <set>
#include <unordered_map>

#include "Components/WidgetComponent.h"
#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"

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
 * -- Implements Marching Cube Isosurface Generatiion and Rendering.
 */
UCLASS()
class VIS4EARTH_API AMCCActor : public AStaticMeshActor {
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
    void OnEditableText_HeightRangeMinTextChanged(const FText &Text) {
        HeightRange[0] = FCString::Atoi(*Text.ToString());
        marchingCube();
    }
    UFUNCTION()
    void OnEditableText_HeightRangeMaxTextChanged(const FText &Text) {
        HeightRange[1] = FCString::Atoi(*Text.ToString());
        marchingCube();
    }
    UFUNCTION()
    void OnEditableText_IsoValueTextChanged(const FText &Text) {
        IsoValue = FCString::Atof(*Text.ToString());
        marchingCube();
    }

    AMCCActor();

  protected:
    virtual void BeginPlay() override;

  private:
    EMCCMeshSmoothType prevMeshSmoothType = EMCCMeshSmoothType::None;

    TObjectPtr<UMaterial> material;
    TObjectPtr<UStaticMesh> mesh;
    TObjectPtr<UStaticMesh> meshSmoothed;

    TArray<FVertexID> indices;
    struct VertexAttr {
        FVector Position;
        FVector Normal = FVector::Zero();
        FVector PositionSmoothed;
        FVector NormalSmoothed;
        double Scalar;
        FVertexID IDSmoothed;

        VertexAttr(const FVector &Pos, double Scalar) : Position(Pos), Scalar(Scalar) {}
    };
    struct HashFVertexID {
        size_t operator()(FVertexID ID) const { return std::hash<int32>()(ID.GetValue()); }
    };
    std::unordered_map<FVertexID, VertexAttr, HashFVertexID> vertAttrs;

    struct Edge {
        std::array<int32, 2> VertIDs;
        Edge(FVertexID V0, FVertexID V1) : VertIDs{V0, V1} {}

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
    void emptyMesh();
    void updateMesh();
    void generateSmoothedMesh(bool ShouldReGen = false);

  private:
#ifdef WITH_EDITOR
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
            generateSmoothedMesh();
            return;
        }
    }
#endif // WITH_EDITOR
};
