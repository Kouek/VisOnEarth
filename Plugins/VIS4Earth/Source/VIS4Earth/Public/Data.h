// Author: Kouek Kou

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/VolumeTexture.h"

#include "Util.h"

template <typename T>
concept SupportedVoxelType =
    std::is_same_v<T, uint8> || std::is_same_v<T, uint16> || std::is_same_v<T, float>;

UENUM()
enum class ESupportedVoxelType : uint8 {
    None = 0 UMETA(DisplayName = "None"),
    UInt8 UMETA(DisplayName = "Unsigned Int 8 bit"),
    UInt16 UMETA(DisplayName = "Unsigned Int 16 bit"),
    Float32 UMETA(DisplayName = "Float 32 bit")
};
UENUM()
enum class ESmoothType : uint8 {
    Avg = 0 UMETA(DisplayName = "Average"),
    Max UMETA(DisplayName = "Maximum")
};
UENUM()
enum class ESmoothDimension : uint8 {
    XYZ = 0 UMETA(DisplayName = "Smooth over XYZ Sapce"),
    XY UMETA(DisplayName = "Smooth over XY Plane")
};

class VolumeData {
  public:
    struct LoadFromFileDesc {
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(ESupportedVoxelType, VoxTy, ESupportedVoxelType::None)
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(FIntVector3, Axis, {1 VIS4EARTH_COMMA 2 VIS4EARTH_COMMA 3})
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(FIntVector3, Dimension, FIntVector::ZeroValue)
        FFilePath FilePath;
        FName Name;
    };
    static TVariant<UVolumeTexture *, FString>
    LoadFromFile(const LoadFromFileDesc &Desc,
                 TOptional<std::reference_wrapper<TArray<uint8>>> VolumeOut = {});

    struct SmoothFromFlatArrayDesc {
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(ESmoothType, SmoothTy, ESmoothType::Avg)
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(ESmoothDimension, SmoothDim, ESmoothDimension::XYZ)
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(ESupportedVoxelType, VoxTy, ESupportedVoxelType::None)
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(FIntVector3, Dimension, FIntVector::ZeroValue)
        const TArray<uint8> &VolDat;
        FName Name;
    };
    static TVariant<UVolumeTexture *, FString>
    SmoothFromFlatArray(const SmoothFromFlatArrayDesc &Desc,
                        TOptional<std::reference_wrapper<TArray<uint8>>> SmoothedVolOut = {});

    static EPixelFormat GetVoxelPixelFormat(ESupportedVoxelType Type) {
        switch (Type) {
        case ESupportedVoxelType::UInt8:
            return PF_R8;
        case ESupportedVoxelType::UInt16:
            return PF_R16_UINT;
        case ESupportedVoxelType::Float32:
            return PF_R32_FLOAT;
        default:
            break;
        }
        return PF_Unknown;
    }

    static size_t GetVoxelSize(ESupportedVoxelType Type) {
        switch (Type) {
        case ESupportedVoxelType::UInt8:
            return sizeof(uint8);
        case ESupportedVoxelType::UInt16:
            return sizeof(uint16);
        case ESupportedVoxelType::Float32:
            return sizeof(float);
        default:
            break;
        }
        return 0;
    }

    static TTuple<float, float, float> GetVoxelMinMaxExtent(ESupportedVoxelType Type) {
        switch (Type) {
        case ESupportedVoxelType::UInt8:
            return MakeTuple(0.f, static_cast<float>(std::numeric_limits<uint8>::max()),
                             static_cast<float>(std::numeric_limits<uint8>::max()));
        case ESupportedVoxelType::UInt16:
            return MakeTuple(0.f, static_cast<float>(std::numeric_limits<uint16>::max()),
                             static_cast<float>(std::numeric_limits<uint16>::max()));
        case ESupportedVoxelType::Float32:
            return MakeTuple(0.f, 1.f, 1.f);
        default:
            break;
        }
        return MakeTuple(0.f, 0.f, 1.f);
    }
};

class TransferFunctionData {
  public:
    struct Desc {
        FFilePath FilePath;
        FName Name;
    };

    static constexpr auto Resolution = 256;
    static constexpr auto ElemSz = sizeof(FFloat16) * 4;

    static TVariant<TTuple<UTexture2D *, UCurveLinearColor *>, FString>
    LoadFromFile(const Desc &Desc);
    static TOptional<FString> SaveToFile(const UCurveLinearColor *Curve, const FFilePath &FilePath);

    static UTexture2D *FromFlatArrayToTexture(const TArray<FFloat16> &Dat,
                                              const FName &Name = NAME_None);
    static void FromFlatArrayToTexture(UTexture2D *Tex, const TArray<FFloat16> &Dat);
    static UCurveLinearColor *FromPointsToCurve(const TMap<float, FVector4f> &Pnts);
    static void FromPointsToCurve(UCurveLinearColor *Curve, const TMap<float, FVector4f> &Pnts);
    static TArray<FFloat16> LerpFromPointsToFlatArray(const TMap<float, FVector4f> &Pnts);
};
