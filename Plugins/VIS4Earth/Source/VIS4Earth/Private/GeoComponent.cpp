#include "GeoComponent.h"

#include <array>

#include "Components/EditableText.h"
#include "Framework/Notifications/NotificationManager.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#include "Widgets/Notifications/SNotificationList.h"

void UGeoComponent::checkAndCorrectParameters() {
    if (LongtitudeRange[0] < -180.)
        LongtitudeRange[0] = -180.;
    if (LongtitudeRange[0] > +180.)
        LongtitudeRange[0] = +180.;
    if (LongtitudeRange[1] < LongtitudeRange[0])
        LongtitudeRange[1] = LongtitudeRange[0];
    if (LongtitudeRange[1] > +180.)
        LongtitudeRange[1] = +180.;
    if (LongtitudeRange[1] < LongtitudeRange[0])
        LongtitudeRange[1] = LongtitudeRange[0];

    if (LatitudeRange[0] < -90.)
        LatitudeRange[0] = -90.;
    if (LatitudeRange[0] > +90.)
        LatitudeRange[0] = +90.;
    if (LatitudeRange[1] < LatitudeRange[0])
        LatitudeRange[1] = LatitudeRange[0];
    if (LatitudeRange[1] > +90.)
        LatitudeRange[1] = +90.;
    if (LatitudeRange[1] < LatitudeRange[0])
        LatitudeRange[1] = LatitudeRange[0];

    if (HeightRange[0] < 0.)
        HeightRange[0] = 0.;
    if (HeightRange[1] < HeightRange[0])
        HeightRange[1] = HeightRange[0];
}

void UGeoComponent::onGeographicsChanged() {
    checkAndCorrectParameters();

    if (GeoRef.IsValid())
        SetWorldLocation(GeoRef->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
            LongtitudeRange[0], .5 * (LatitudeRange[0] + LatitudeRange[1]), HeightRange[1])));

    OnGeographicsChanged.Broadcast(this);
}

void UGeoComponent::processError(const FString &ErrMsg) {
    FNotificationInfo info(FText::FromString(ErrMsg));

    auto notifyItem = FSlateNotificationManager::Get().AddNotification(info);
    notifyItem->SetCompletionState(SNotificationItem::ECompletionState::CS_Fail);
    notifyItem->ExpireAndFadeout();
}

UStaticMesh *UGeoComponent::GenerateGeoMesh(int32 LongtitudeTessellation,
                                            int32 LatitudeTessellation) {
    if (!GeoRef.IsValid()) {
        processError(TEXT("GeoRef is NOT set."));
        return nullptr;
    }

    if (LongtitudeTessellation < 0 || LatitudeTessellation < 0) {
        processError(FString::Format(
            TEXT("LongtitudeTessellation {0} or LatitudeTessellation {1} is invalid."),
            {LongtitudeTessellation, LatitudeTessellation}));
        return nullptr;
    }

    auto *mesh = NewObject<UStaticMesh>();
    mesh->GetStaticMaterials().Add(FStaticMaterial());

    FMeshDescription meshDesc;

    FStaticMeshAttributes meshAttrs(meshDesc);
    meshAttrs.Register();

    FMeshDescriptionBuilder meshDescBuilder;
    meshDescBuilder.SetMeshDescription(&meshDesc);
    meshDescBuilder.EnablePolyGroups();
    meshDescBuilder.SetNumUVLayers(2);

    struct VertexAttr {
        FVertexID ID;
        FVector Normal;
        FVector TexCoord;
    };
    TArray<VertexAttr> vertices;
    int btmSurfVertStart;
    {
        auto lonExt = LongtitudeRange[1] - LongtitudeRange[0];
        auto latExt = LatitudeRange[1] - LatitudeRange[0];
        auto lonDlt = lonExt / LongtitudeTessellation;
        auto latDlt = latExt / LatitudeTessellation;

        auto genSurfVertices = [&](bool top) {
            auto h = top ? HeightRange[1] : HeightRange[0];
            for (int latIdx = 0; latIdx < LatitudeTessellation; ++latIdx)
                for (int lonIdx = 0; lonIdx < LongtitudeTessellation; ++lonIdx) {
                    vertices.Emplace();
                    vertices.Last().TexCoord.X = 1. * lonIdx / (LongtitudeTessellation - 1);
                    vertices.Last().TexCoord.Y = top ? 1. : 0.;
                    vertices.Last().TexCoord.Z = 1. * latIdx / (LatitudeTessellation - 1);

                    auto lon = LongtitudeRange[0] + vertices.Last().TexCoord.X * lonExt;
                    auto lat = LatitudeRange[0] + vertices.Last().TexCoord.Z * latExt;
                    vertices.Last().TexCoord.Z = 1.f - vertices.Last().TexCoord.Z;

                    auto tmp =
                        GeoRef->TransformLongitudeLatitudeHeightPositionToUnreal({lon, lat, h});
                    vertices.Last().ID = meshDescBuilder.AppendVertex(tmp);

                    tmp.Normalize();
                    vertices.Last().Normal = top ? tmp : -tmp;
                }
        };
        genSurfVertices(true);
        btmSurfVertStart = vertices.Num();
        genSurfVertices(false);
    }

    {
        auto polyGrpID = meshDescBuilder.AppendPolygonGroup();
        auto addTri = [&](std::array<int, 3> triIndices) {
            std::array<FVertexInstanceID, 3> instIDs;
            for (uint8 i = 0; i < 3; ++i) {
                auto vert = vertices[triIndices[i]];
                instIDs[i] = meshDescBuilder.AppendInstance(vert.ID);
                meshDescBuilder.SetInstanceNormal(instIDs[i], vert.Normal);
                meshDescBuilder.SetInstanceUV(instIDs[i],
                                              FVector2D(vert.TexCoord.X, vert.TexCoord.Y), 0);
                meshDescBuilder.SetInstanceUV(instIDs[i], FVector2D(vert.TexCoord.Z, 0.), 1);
            }
            meshDescBuilder.AppendTriangle(instIDs[0], instIDs[1], instIDs[2], polyGrpID);
        };
        auto addTopBotSurf = [&](bool top, int latIdx, int lonIdx) {
            auto start = top ? 0 : btmSurfVertStart;
            std::array<int, 4> quadIndices = {
                start + latIdx * LongtitudeTessellation + lonIdx,
                start + latIdx * LongtitudeTessellation + lonIdx + (top ? 1 : -1),
                start + (latIdx + 1) * LongtitudeTessellation + lonIdx + (top ? 1 : -1),
                start + (latIdx + 1) * LongtitudeTessellation + lonIdx};

            addTri({quadIndices[0], quadIndices[1], quadIndices[2]});
            addTri({quadIndices[0], quadIndices[2], quadIndices[3]});
        };
        for (int latIdx = 0; latIdx < LatitudeTessellation - 1; ++latIdx)
            for (int lonIdx = 0; lonIdx < LongtitudeTessellation - 1; ++lonIdx) {
                addTopBotSurf(true, latIdx, lonIdx);
                addTopBotSurf(false, latIdx, LongtitudeTessellation - 1 - lonIdx);
            }

        auto addSideSurf = [&](int latIdx, int lonIdx, FIntVector2 dir) {
            std::array<int, 4> quadIndices = {
                btmSurfVertStart + latIdx * LongtitudeTessellation + lonIdx,
                btmSurfVertStart + (latIdx + dir.Y) * LongtitudeTessellation + lonIdx + dir.X,
                (latIdx + dir.Y) * LongtitudeTessellation + lonIdx + dir.X,
                latIdx * LongtitudeTessellation + lonIdx};
            addTri({quadIndices[0], quadIndices[1], quadIndices[2]});
            addTri({quadIndices[0], quadIndices[2], quadIndices[3]});
        };
        for (int lonIdx = 0; lonIdx < LongtitudeTessellation - 1; ++lonIdx) {
            addSideSurf(0, lonIdx, {1, 0});
            addSideSurf(LatitudeTessellation - 1, LongtitudeTessellation - 1 - lonIdx, {-1, 0});
        }
        for (int latIdx = 0; latIdx < LatitudeTessellation - 1; ++latIdx) {
            addSideSurf(LatitudeTessellation - 1 - latIdx, 0, {0, -1});
            addSideSurf(latIdx, LongtitudeTessellation - 1, {0, 1});
        }
    }

    UStaticMesh::FBuildMeshDescriptionsParams buildMesDescParams;
    buildMesDescParams.bFastBuild = true;

    TArray<const FMeshDescription *> meshDescs;
    meshDescs.Emplace(&meshDesc);

    mesh->BuildFromMeshDescriptions(meshDescs, buildMesDescParams);

    return mesh;
}

struct GeoComponentNamesInUI {
    static constexpr std::array LongtitudeRange = {TEXT("EditableText_LongtitudeRangeMin"),
                                                   TEXT("EditableText_LongtitudeRangeMax")};
    static constexpr std::array LatitudeRange = {TEXT("EditableText_LatitudeRangeMin"),
                                                 TEXT("EditableText_LatitudeRangeMax")};
    static constexpr std::array HeightRange = {TEXT("EditableText_HeightRangeMin"),
                                               TEXT("EditableText_HeightRangeMax")};
};

void UGeoComponent::BeginPlay() {
    Super::BeginPlay();

    {
        auto realUIClass = LoadClass<UUserWidget>(
            nullptr, TEXT("WidgetBlueprint'/VIS4Earth/UI_GeoCmpt.UI_GeoCmpt_C'"));
        ui = CreateWidget(GetWorld(), realUIClass);

        for (int32 i = 0; i < 2; ++i) {
            Cast<UEditableText>(ui->GetWidgetFromName(GeoComponentNamesInUI::LongtitudeRange[i]))
                ->SetText(FText::FromString(FString::SanitizeFloat(LongtitudeRange[i])));
            Cast<UEditableText>(ui->GetWidgetFromName(GeoComponentNamesInUI::LatitudeRange[i]))
                ->SetText(FText::FromString(FString::SanitizeFloat(LatitudeRange[i])));
            Cast<UEditableText>(ui->GetWidgetFromName(GeoComponentNamesInUI::HeightRange[i]))
                ->SetText(FText::FromString(FString::SanitizeFloat(HeightRange[i])));
        }

        VIS4EARTH_UI_ADD_SLOT(UGeoComponent, this, ui, EditableText, LongtitudeRangeMin,
                              TextChanged);
        VIS4EARTH_UI_ADD_SLOT(UGeoComponent, this, ui, EditableText, LongtitudeRangeMax,
                              TextChanged);
        VIS4EARTH_UI_ADD_SLOT(UGeoComponent, this, ui, EditableText, LatitudeRangeMin, TextChanged);
        VIS4EARTH_UI_ADD_SLOT(UGeoComponent, this, ui, EditableText, LatitudeRangeMax, TextChanged);
        VIS4EARTH_UI_ADD_SLOT(UGeoComponent, this, ui, EditableText, HeightRangeMin, TextChanged);
        VIS4EARTH_UI_ADD_SLOT(UGeoComponent, this, ui, EditableText, HeightRangeMax, TextChanged);
    }
}
