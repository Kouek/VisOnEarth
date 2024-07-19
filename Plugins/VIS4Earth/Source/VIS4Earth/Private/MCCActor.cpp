#include "MCCActor.h"

#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/NamedSlot.h"

#include "MCCTable.h"

void AMCCActor::OnComboBoxString_MeshSmoothTypeSelectionChanged(FString SelectedItem,
                                                                ESelectInfo::Type SelectionType) {
    auto enumClass = StaticEnum<EMCCMeshSmoothType>();
    for (int32 i = 0; i < enumClass->GetMaxEnumValue(); ++i)
        if (enumClass->GetNameByIndex(i) == SelectedItem) {
            MeshSmoothType = static_cast<EMCCMeshSmoothType>(i);
            break;
        }

    marchingCube();
}

AMCCActor::AMCCActor() {
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("StaticMesh"));

    GeoComponent = CreateDefaultSubobject<UGeoComponent>(TEXT("Geographics"));
    RootComponent = GeoComponent;

    VolumeComponent = CreateDefaultSubobject<UVolumeDataComponent>(TEXT("VolumeData"));
    VolumeComponent->SetKeepVolumeInCPU(true);
    VolumeComponent->SetKeepSmoothedVolume(true);

    UIComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("UI"));
    UIComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

    setupSignalsSlots();
}

void AMCCActor::BeginPlay() {
    Super::BeginPlay();

    {
        auto realUIClass =
            LoadClass<UUserWidget>(nullptr, TEXT("WidgetBlueprint'/VIS4Earth/UI_MCC.UI_MCC_C'"));

        UIComponent->SetDrawAtDesiredSize(true);
        UIComponent->SetWidgetClass(realUIClass);
        UIComponent->SetWidgetSpace(EWidgetSpace::Screen);

        auto ui = UIComponent->GetUserWidgetObject();
        VIS4EARTH_UI_COMBOBOXSTRING_ADD_OPTIONS(ui, MeshSmoothType);

        Cast<UCheckBox>(ui->GetWidgetFromName(TEXT("CheckBox_UseLerp")))
            ->SetCheckedState(UseLerp ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
        Cast<UCheckBox>(ui->GetWidgetFromName(TEXT("CheckBox_UseSmoothedVolume")))
            ->SetCheckedState(UseSmoothedVolume ? ECheckBoxState::Checked
                                                : ECheckBoxState::Unchecked);
        Cast<UComboBoxString>(ui->GetWidgetFromName(TEXT("ComboBoxString_MeshSmoothType")))
            ->SetSelectedIndex(static_cast<int32>(MeshSmoothType));
        Cast<UEditableText>(ui->GetWidgetFromName(TEXT("EditableText_HeightRangeMin")))
            ->SetText(FText::FromString(FString::FromInt(HeightRange[0])));
        Cast<UEditableText>(ui->GetWidgetFromName(TEXT("EditableText_HeightRangeMax")))
            ->SetText(FText::FromString(FString::FromInt(HeightRange[1])));
        Cast<UEditableText>(ui->GetWidgetFromName(TEXT("EditableText_IsoValue")))
            ->SetText(FText::FromString(FString::SanitizeFloat(IsoValue)));

        Cast<UNamedSlot>(ui->GetWidgetFromName(TEXT("NamedSlot_UI_VolumeCmpt")))
            ->SetContent(VolumeComponent->GetUI());
        Cast<UNamedSlot>(ui->GetWidgetFromName(TEXT("NamedSlot_UI_GeoCmpt")))
            ->SetContent(GeoComponent->GetUI());

        VIS4EARTH_UI_ADD_SLOT(AMCCActor, this, ui, CheckBox, UseLerp, CheckStateChanged);
        VIS4EARTH_UI_ADD_SLOT(AMCCActor, this, ui, CheckBox, UseSmoothedVolume, CheckStateChanged);
        VIS4EARTH_UI_ADD_SLOT(AMCCActor, this, ui, ComboBoxString, MeshSmoothType,
                              SelectionChanged);
        VIS4EARTH_UI_ADD_SLOT(AMCCActor, this, ui, EditableText, HeightRangeMin, TextCommitted);
        VIS4EARTH_UI_ADD_SLOT(AMCCActor, this, ui, EditableText, HeightRangeMax, TextCommitted);
        VIS4EARTH_UI_ADD_SLOT(AMCCActor, this, ui, EditableText, IsoValue, TextCommitted);
    }
}

void AMCCActor::setupSignalsSlots() {
    GeoComponent->OnGeographicsChanged.AddLambda([this](UGeoComponent *) { marchingCube(); });
    VolumeComponent->OnTransferFunctionDataChanged.AddLambda(
        [this](UVolumeDataComponent *) { updateMaterialInstanceDynamic(); });
    VolumeComponent->OnVolumeDataChanged.AddLambda(
        [this](UVolumeDataComponent *) { marchingCube(); });
}

void AMCCActor::checkAndCorrectParameters() {
    if (!VolumeComponent->VolumeTexture)
        return;

    {
        auto [vxMin, vxMax, vxExt] =
            VolumeData::GetVoxelMinMaxExtent(VolumeComponent->GetVolumeVoxelType());
        if (IsoValue < vxMin)
            IsoValue = vxMin;
        if (IsoValue > vxMax)
            IsoValue = vxMax;
    }

    FIntVector3 voxPerVol(VolumeComponent->VolumeTexture->GetSizeX(),
                          VolumeComponent->VolumeTexture->GetSizeY(),
                          VolumeComponent->VolumeTexture->GetSizeZ());
    auto voxPerVolYxX = static_cast<size_t>(voxPerVol.Y) * voxPerVol.X;
    if (HeightRange[0] < 0)
        HeightRange[0] = 0;
    if (HeightRange[0] >= voxPerVol.Z)
        HeightRange[0] = voxPerVol.Z - 1;
    if (HeightRange[1] < HeightRange[0])
        HeightRange[1] = HeightRange[0];
    if (HeightRange[1] >= voxPerVol.Z)
        HeightRange[1] = voxPerVol.Z - 1;
}

void AMCCActor::marchingCube() {
    checkAndCorrectParameters();

    if (!VolumeComponent->VolumeTexture) {
        emptyMesh();
        return;
    }

    edges.clear();
    auto gen = [&]<SupportedVoxelType T>(T) {
        FIntVector3 voxPerVol(VolumeComponent->VolumeTexture->GetSizeX(),
                              VolumeComponent->VolumeTexture->GetSizeY(),
                              VolumeComponent->VolumeTexture->GetSizeZ());
        auto [vxMin, vxMax, vxExt] =
            VolumeData::GetVoxelMinMaxExtent(VolumeComponent->GetVolumeVoxelType());
        auto lonExt = GeoComponent->LongtitudeRange[1] - GeoComponent->LongtitudeRange[0];
        auto latExt = GeoComponent->LatitudeRange[1] - GeoComponent->LatitudeRange[0];
        auto hExt = GeoComponent->HeightRange[1] - GeoComponent->HeightRange[0];

        auto hashEdge = [](const FIntVector3 &edgeID) {
            size_t hash = edgeID.X;
            hash = (hash << 32) | edgeID.Y;
            hash = (hash << 2) | edgeID.Z;
            return std::hash<size_t>()(hash);
        };
        std::array<std::unordered_map<FIntVector3, FVertexID, decltype(hashEdge)>, 2> edge2vertIDs;

        positions.Empty();
        normals.Empty();
        uvs.Empty();
        indices.Empty();

        edges.clear();
        FIntVector3 startPos;
        for (startPos.Z = HeightRange[0]; startPos.Z < HeightRange[1]; ++startPos.Z) {
            if (startPos.Z != HeightRange[0]) {
                edge2vertIDs[0] = std::move(edge2vertIDs[1]);
                edge2vertIDs[1].clear(); // hash map only stores vertices of 2 consecutive heights
            }

            for (startPos.Y = 0; startPos.Y < voxPerVol.Y - 1; ++startPos.Y)
                for (startPos.X = 0; startPos.X < voxPerVol.X - 1; ++startPos.X) {
                    // Voxels in CCW order form a grid
                    // +-----------------+
                    // |       3 <--- 2  |
                    // |       |     /|\ |
                    // |      \|/     |  |
                    // |       0 ---> 1  |
                    // |      /          |
                    // |  7 <--- 6       |
                    // |  | /   /|\      |
                    // | \|/_    |       |
                    // |  4 ---> 5       |
                    // +-----------------+
                    uint8 cornerState = 0;
                    std::array<float, 8> scalars;
                    for (int32 i = 0; i < 8; ++i) {
                        if (UseSmoothedVolume) {
                            scalars[i] = VolumeComponent->SampleVolumeCPUDataSmoothed(startPos);
                            scalars[i] = scalars[i] * vxExt + vxMin; // [0, 1] -> [vxMin, vxMax]
                        } else
                            scalars[i] = VolumeComponent->SampleVolumeCPUData<T>(startPos);
                        if (scalars[i] >= IsoValue)
                            cornerState |= 1 << i;

                        startPos.X += i == 0 || i == 4 ? 1 : i == 2 || i == 6 ? -1 : 0;
                        startPos.Y += i == 1 || i == 5 ? 1 : i == 3 || i == 7 ? -1 : 0;
                        startPos.Z += i == 3 ? 1 : i == 7 ? -1 : 0;
                    }
                    std::array omegas = {scalars[0] / (scalars[1] + scalars[0]),
                                         scalars[1] / (scalars[2] + scalars[1]),
                                         scalars[3] / (scalars[3] + scalars[2]),
                                         scalars[0] / (scalars[0] + scalars[3]),
                                         scalars[4] / (scalars[5] + scalars[4]),
                                         scalars[5] / (scalars[6] + scalars[5]),
                                         scalars[7] / (scalars[7] + scalars[6]),
                                         scalars[4] / (scalars[4] + scalars[7]),
                                         scalars[0] / (scalars[0] + scalars[4]),
                                         scalars[1] / (scalars[1] + scalars[5]),
                                         scalars[2] / (scalars[2] + scalars[6]),
                                         scalars[3] / (scalars[3] + scalars[7])};

                    // Edge indexed by Start Voxel Position
                    // +----------+
                    // | /*\  *|  |
                    // |  |  /    |
                    // | e1 e2    |
                    // |  * e0 *> |
                    // +----------+
                    // *:   startPos
                    // *>:  startPos + (1,0,0)
                    // /*\: startPos + (0,1,0)
                    // *|:  startPos + (0,0,1)
                    // ID(e0) = (startPos.xy, 00)
                    // ID(e1) = (startPos.xy, 01)
                    // ID(e2) = (startPos.xy, 10)
                    for (uint32 i = 0; i < GVertNumTable[cornerState]; i += 3) {
                        for (int32 ii = 0; ii < 3; ++ii) {
                            auto ei = GEdgeTable[cornerState][i + ii];
                            FIntVector3 edgeID(
                                startPos.X + (ei == 1 || ei == 5 || ei == 9 || ei == 10 ? 1 : 0),
                                startPos.Y + (ei == 2 || ei == 6 || ei == 10 || ei == 11 ? 1 : 0),
                                ei >= 8                                    ? 2
                                : ei == 1 || ei == 3 || ei == 5 || ei == 7 ? 1
                                                                           : 0);
                            auto edge2vertIDIdx = ei >= 4 && ei < 8 ? 1 : 0;
                            if (auto itr = edge2vertIDs[edge2vertIDIdx].find(edgeID);
                                itr != edge2vertIDs[edge2vertIDIdx].end()) {
                                indices.Emplace(itr->second);
                                continue;
                            }

                            FVector pos(
                                startPos.X + (ei == 0 || ei == 2 || ei == 4 || ei == 6
                                                  ? (UseLerp ? omegas[ei] : .5f)
                                              : ei == 1 || ei == 5 || ei == 9 || ei == 10 ? 1.f
                                                                                          : 0.f),
                                startPos.Y + (ei == 1 || ei == 3 || ei == 5 || ei == 7
                                                  ? (UseLerp ? omegas[ei] : .5f)
                                              : ei == 2 || ei == 6 || ei == 10 || ei == 11 ? 1.f
                                                                                           : 0.f),
                                startPos.Z + (ei >= 8   ? (UseLerp ? omegas[ei] : .5f)
                                              : ei >= 4 ? 1.f
                                                        : 0.f));
                            pos /= FVector(voxPerVol);
                            pos = [&]() {
                                auto lon = GeoComponent->LongtitudeRange[0] + pos.X * lonExt;
                                auto lat = GeoComponent->LatitudeRange[0] + pos.Y * latExt;
                                auto h = GeoComponent->HeightRange[0] + pos.Z * hExt;

                                return GeoComponent->GeoRef
                                    ->TransformLongitudeLatitudeHeightPositionToUnreal(
                                        {lon, lat, h});
                            }();

                            auto scalar = [&]() {
                                switch (ei) {
                                case 0:
                                    return omegas[0] * scalars[0] + (1.f - omegas[0]) * scalars[1];
                                case 1:
                                    return omegas[1] * scalars[1] + (1.f - omegas[1]) * scalars[2];
                                case 2:
                                    return omegas[2] * scalars[3] + (1.f - omegas[2]) * scalars[2];
                                case 3:
                                    return omegas[3] * scalars[0] + (1.f - omegas[3]) * scalars[3];
                                case 4:
                                    return omegas[4] * scalars[4] + (1.f - omegas[4]) * scalars[5];
                                case 5:
                                    return omegas[5] * scalars[5] + (1.f - omegas[5]) * scalars[6];
                                case 6:
                                    return omegas[6] * scalars[7] + (1.f - omegas[6]) * scalars[6];
                                case 7:
                                    return omegas[7] * scalars[4] + (1.f - omegas[7]) * scalars[7];
                                default:
                                    return omegas[ei] * scalars[ei - 8] +
                                           (1.f - omegas[ei]) * scalars[ei - 4];
                                }
                            }();
                            scalar = (scalar - vxMin) / vxExt; // [vxMin, vxMax] -> [0, 1]

                            auto id = positions.Emplace(pos);
                            normals.Emplace(FVector::Zero());
                            uvs.Emplace(scalar, 0.f);
                            indices.Emplace(id);
                            edge2vertIDs[edge2vertIDIdx].emplace(edgeID, indices.Last());
                        }

                        std::array<FVertexID, 3> triVertIDs = {indices[indices.Num() - 3],
                                                               indices[indices.Num() - 2],
                                                               indices[indices.Num() - 1]};
                        auto norm = [&]() {
                            auto e0 = positions[triVertIDs[1]] - positions[triVertIDs[0]];
                            auto e1 = positions[triVertIDs[2]] - positions[triVertIDs[0]];
                            auto norm = FVector::CrossProduct(e0, e1);
                            norm.Normalize();

                            return norm;
                        }();
                        normals[triVertIDs[0]] += norm;
                        normals[triVertIDs[1]] += norm;
                        normals[triVertIDs[2]] += norm;

                        edges.emplace(triVertIDs[0], triVertIDs[1]);
                        edges.emplace(triVertIDs[1], triVertIDs[0]);
                        edges.emplace(triVertIDs[1], triVertIDs[2]);
                        edges.emplace(triVertIDs[2], triVertIDs[1]);
                        edges.emplace(triVertIDs[2], triVertIDs[0]);
                        edges.emplace(triVertIDs[0], triVertIDs[2]);
                    }
                }
        }

        for (auto &normal : normals)
            normal.Normalize();
        for (int32 i = 0; i < indices.Num(); i += 3)
            // From CCW to CW
            std::swap(indices[i + 1], indices[i + 2]);
    };

    switch (VolumeComponent->GetVolumeVoxelType()) {
    case ESupportedVoxelType::UInt8:
        gen(uint8(0));
        break;
    }
    if (indices.IsEmpty()) {
        emptyMesh();
        return;
    }

    MeshComponent->CreateMeshSection(static_cast<int>(EMeshSectionIndex::Normal), positions,
                                     indices, normals, uvs, TArray<FColor>(),
                                     TArray<FProcMeshTangent>(), false);

    generateSmoothedMeshThenUpdateMesh();
}

void AMCCActor::emptyMesh() { MeshComponent->ClearAllMeshSections(); }

void AMCCActor::updateMesh() {
    if (MeshComponent->GetNumSections() == 0)
        return;

    auto setSectionVisibility = [&](EMeshSectionIndex idx, bool visibility) {
        auto *section = MeshComponent->GetProcMeshSection(static_cast<int>(idx));
        if (section)
            section->bSectionVisible = visibility;
    };

    switch (MeshSmoothType) {
    case EMCCMeshSmoothType::None: {
        setSectionVisibility(EMeshSectionIndex::Normal, true);
        setSectionVisibility(EMeshSectionIndex::Smoothed, false);
    } break;
    default:
        setSectionVisibility(EMeshSectionIndex::Normal, false);
        setSectionVisibility(EMeshSectionIndex::Smoothed, true);
    }

    updateMaterialInstanceDynamic();
}

void AMCCActor::updateMaterialInstanceDynamic() {
    if (!MaterialInstanceDynamic) {
        auto material = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr,
                                                         TEXT("Material'/VIS4Earth/M_MCC.M_MCC'")));
        MaterialInstanceDynamic = MeshComponent->CreateDynamicMaterialInstance(0, material);
    }

    if (MaterialInstanceDynamic) {
        MaterialInstanceDynamic->SetTextureParameterValue(
            TEXT("TF"), VolumeComponent->TransferFunctionTexture
                            ? VolumeComponent->TransferFunctionTexture
                            : VolumeComponent->DefaultTransferFunctionTexture);
        MeshComponent->SetMaterial(0, MaterialInstanceDynamic);
        MeshComponent->SetMaterial(1, MaterialInstanceDynamic);
    } else {
        UE_LOG(LogStats, Error, TEXT("AMCCActor lost MaterialInstanceDynamic."));
    }
}

void AMCCActor::generateSmoothedMeshThenUpdateMesh(bool ShouldReGen) {
    if (MeshSmoothType == EMCCMeshSmoothType::None ||
        (prevMeshSmoothType == MeshSmoothType && !ShouldReGen)) {
        updateMesh();
        return;
    }

    auto laplacian = [&]() {
        positionsSmoothed = positions;
        normalsSmoothed = normals;

        for (int32 vertID = 0; vertID < positions.Num(); ++vertID) {
            auto itr = edges.lower_bound(Edge{vertID, 0});

            auto &positionSmoothed = positionsSmoothed[vertID];
            auto &normalSmoothed = normalsSmoothed[vertID];
            int32 adjNum = 1;
            while (itr != edges.end() && itr->VertIDs[0] == vertID) {
                positionSmoothed += positions[itr->VertIDs[1]];
                normalSmoothed += normals[itr->VertIDs[1]];

                ++itr;
                ++adjNum;
            }
            positionSmoothed /= adjNum;
            normalSmoothed.Normalize();
        }
    };
    auto curvature = [&]() {
        positionsSmoothed.SetNum(positions.Num());
        normalsSmoothed.SetNum(positions.Num());

        for (int32 vertID = 0; vertID < positions.Num(); ++vertID) {
            auto itr = edges.lower_bound(Edge{vertID, 0});

            const auto &position = positions[vertID];
            const auto &normal = normals[vertID];
            auto &positionSmoothed = positionsSmoothed[vertID];
            auto &normalSmoothed = normalsSmoothed[vertID];
            auto projLen = 0.;
            int32 adjNum = 1;
            while (itr != edges.end() && itr->VertIDs[0] == vertID) {
                projLen += FVector::DotProduct(positions[itr->VertIDs[1]] - position, normal);

                ++itr;
                ++adjNum;
            }
            projLen /= adjNum;
            positionSmoothed = position + projLen * normal;
            normalSmoothed = normal;
        }
    };

    switch (MeshSmoothType) {
    case EMCCMeshSmoothType::Laplacian:
        laplacian();
        break;
    case EMCCMeshSmoothType::Curvature:
        curvature();
        break;
    }

    MeshComponent->CreateMeshSection(static_cast<int>(EMeshSectionIndex::Smoothed), positions,
                                     indices, normals, uvs, TArray<FColor>(),
                                     TArray<FProcMeshTangent>(), false);

    updateMesh();
    prevMeshSmoothType = MeshSmoothType;
}
