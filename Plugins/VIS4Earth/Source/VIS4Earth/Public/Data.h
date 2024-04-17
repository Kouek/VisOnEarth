// Author: Kouek Kou

#pragma once

#include <functional>

#include <map>

#include "CoreMinimal.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/VolumeTexture.h"

#include "Util.h"

template <typename T>
concept SupportedVoxelType =
    std::is_same_v<T, uint8> || std::is_same_v<T, uint16> || std::is_same_v<T, float>;

UENUM()
enum class ESupportedVoxelType : uint8 {
    ENone = 0 UMETA(DisplayName = "None"),
    EUInt8 UMETA(DisplayName = "Unsigned Int 8 bit"),
    EUInt16 UMETA(DisplayName = "Unsigned Int 16 bit"),
    EFloat32 UMETA(DisplayName = "Float 32 bit")
};

class VolumeData {
  public:
    struct Desc {
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(ESupportedVoxelType, VoxTy, ESupportedVoxelType::ENone)
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(FIntVector3, Axis, {1 VIS4EARTH_COMMA 2 VIS4EARTH_COMMA 3})
        VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(FIntVector3, Dimension, FIntVector::ZeroValue)
        FFilePath FilePath;
        FName Name;
    };

    static TVariant<UVolumeTexture *, FString>
    LoadFromFile(const Desc &Desc, TOptional<std::reference_wrapper<TArray<uint8>>> VolumeOut = {});

    static EPixelFormat GetVoxelPixelFormat(ESupportedVoxelType Type) {
        switch (Type) {
        case ESupportedVoxelType::EUInt8:
            return PF_R8;
        case ESupportedVoxelType::EUInt16:
            return PF_R16_UINT;
        case ESupportedVoxelType::EFloat32:
            return PF_R32_FLOAT;
        default:
            break;
        }
        return PF_Unknown;
    }

    static float GetVoxelMaxValue(ESupportedVoxelType Type) {
        switch (Type) {
        case ESupportedVoxelType::EUInt8:
            return std::numeric_limits<uint8>::max();
        case ESupportedVoxelType::EUInt16:
            return std::numeric_limits<uint16>::max();
        case ESupportedVoxelType::EFloat32:
            return std::numeric_limits<float>::max();
        default:
            break;
        }
        return 0.f;
    }
};

class TransferFunctionData {
  public:
    struct Desc {
        FFilePath FilePath;
        FName Name;
    };

    static TVariant<TTuple<UTexture2D *, UCurveLinearColor *>, FString>
    LoadFromFile(const Desc &Desc);
    static TOptional<FString> SaveToFile(const UCurveLinearColor *Curve, const FFilePath &FilePath);

    static UTexture2D *FromFlatArrayToTexture(const TArray<FFloat16> &Dat,
                                              const FName &Name = NAME_None);
    static void FromFlatArrayToTexture(UTexture2D *Tex, const TArray<FFloat16> &Dat);
    static UCurveLinearColor *FromPointsToCurve(const std::map<float, FVector4f> &Pnts);
    static void FromPointsToCurve(UCurveLinearColor *Curve, const std::map<float, FVector4f> &Pnts);
    static TArray<FFloat16> LerpFromPointsToFlatArray(const std::map<float, FVector4f> &Pnts);

  private:
    static constexpr auto Resolution = 256;
    static constexpr auto ElemSz = sizeof(FFloat16) * 4;
};
