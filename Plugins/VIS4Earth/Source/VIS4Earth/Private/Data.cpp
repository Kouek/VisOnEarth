// Author: Kouek Kou

#include "Data.h"

#include <array>

TVariant<UVolumeTexture *, FString> VolumeData::LoadFromFile(const Desc &Desc) {
    using RetType = TVariant<UVolumeTexture *, FString>;

    if (Desc.Dimension.X <= 0 || Desc.Dimension.Y <= 0 || Desc.Dimension.Z <= 0)
        return RetType(TInPlaceType<FString>(), FString::Format(TEXT("Invalid Desc.Dimension {0}."),
                                                                {Desc.Dimension.ToString()}));

    TArray<uint8> buf;
    if (!FFileHelper::LoadFileToArray(buf, *Desc.FilePath.FilePath))
        return RetType(TInPlaceType<FString>(), FString::Format(TEXT("Invalid Desc.FilePath {0}."),
                                                                {Desc.FilePath.FilePath}));

    auto pixFmt = [&]() {
        switch (Desc.VoxTy) {
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
    }();
    auto transform = [&]() {
        using TrRetType = TVariant<FIntVector3, FString>;

        if ([&]() {
                FIntVector3 cnts(0, 0, 0);
                for (int i = 0; i < 3; ++i)
                    if (auto j = std::abs(Desc.Axis[i]) - 1; 0 <= j && j <= 2)
                        ++cnts[j];
                    else
                        return true;
                for (int i = 0; i < 3; ++i)
                    if (cnts[i] != 1)
                        return true;
                return false;
            }())
            return TrRetType(TInPlaceType<FString>(),
                             FString::Format(TEXT("Invalid Desc.Axis {0},{1},{2}."),
                                             {Desc.Axis[0], Desc.Axis[1], Desc.Axis[2]}));

        if (Desc.Axis == decltype(Desc.Axis)(1, 2, 3))
            return TrRetType(TInPlaceType<FIntVector3>(), Desc.Dimension);

        FIntVector3 trAxisMap(std::abs(Desc.Axis[0]) - 1, std::abs(Desc.Axis[1]) - 1,
                              std::abs(Desc.Axis[2]) - 1);
        FIntVector3 trDim(Desc.Dimension[trAxisMap[0]], Desc.Dimension[trAxisMap[1]],
                          Desc.Dimension[trAxisMap[2]]);

        decltype(buf) oldBuf = buf;
        auto trVoxYxX = static_cast<size_t>(trDim.Y) * trDim.X;
        size_t offs = 0;
        FIntVector3 coord;
        for (coord.Z = 0; coord.Z < Desc.Dimension.Z; ++coord.Z)
            for (coord.Y = 0; coord.Y < Desc.Dimension.Y; ++coord.Y)
                for (coord.X = 0; coord.X < Desc.Dimension.X; ++coord.X) {
                    FIntVector3 trCoord(
                        Desc.Axis.X > 0 ? coord[trAxisMap[0]] : trDim.X - 1 - coord[trAxisMap[0]],
                        Desc.Axis.Y > 0 ? coord[trAxisMap[1]] : trDim.Y - 1 - coord[trAxisMap[1]],
                        Desc.Axis.Z > 0 ? coord[trAxisMap[2]] : trDim.Z - 1 - coord[trAxisMap[2]]);
                    buf[trCoord.Z * trVoxYxX + trCoord.Y * trDim.X + trCoord.X] = oldBuf[offs];
                    ++offs;
                }

        return TrRetType(TInPlaceType<FIntVector3>(), trDim);
    };

    auto load = [&]<SupportedVoxelType T>(T *dat) -> RetType {
        auto volSz = sizeof(T) * Desc.Dimension.X * Desc.Dimension.Y * Desc.Dimension.Z;

        if (buf.Num() != volSz)
            return RetType(TInPlaceType<FString>(),
                           FString::Format(TEXT("Invalid contents in Desc.FilePath {0}."),
                                           {Desc.FilePath.FilePath}));
        FIntVector3 trDim;
        if (auto ret = transform(); ret.IsType<FString>())
            return RetType(TInPlaceType<FString>(), ret.Get<FString>());
        else
            trDim = ret.Get<FIntVector3>();

        auto tex = UVolumeTexture::CreateTransient(trDim.X, trDim.Y, trDim.Z, pixFmt, Desc.Name);
        tex->Filter = TextureFilter::TF_Trilinear;
        tex->MipGenSettings = Desc.LODNum == 1 ? TextureMipGenSettings::TMGS_NoMipmaps
                                               : TextureMipGenSettings::TMGS_SimpleAverage;
        tex->AddressMode = TextureAddress::TA_Clamp;

        auto *texDat =
            tex->GetPlatformData()->Mips[0].BulkData.Lock(EBulkDataLockFlags::LOCK_READ_WRITE);
        FMemory::Memcpy(texDat, dat, volSz);
        tex->GetPlatformData()->Mips[0].BulkData.Unlock();

        tex->UpdateResource();

        return RetType(TInPlaceType<UVolumeTexture *>(), tex);
    };

    switch (Desc.VoxTy) {
    case ESupportedVoxelType::EUInt8:
        return load(reinterpret_cast<uint8 *>(buf.GetData()));
    default:
        return RetType(TInPlaceType<FString>(), TEXT("Invalid Desc.VoxTy."));
    }
}

TVariant<TTuple<UTexture2D *, UCurveLinearColor *>, FString>
TransferFunctionData::LoadFromFile(const Desc &Desc) {
    using ValueType = TTuple<UTexture2D *, UCurveLinearColor *>;
    using RetType = TVariant<ValueType, FString>;

    constexpr int Resolution = 256;
    constexpr auto ElemSz = sizeof(FFloat16) * 4;

    FJsonSerializableArray buf;
    if (!FFileHelper::LoadANSITextFileToStrings(*Desc.FilePath.FilePath, nullptr, buf))
        return RetType(TInPlaceType<FString>(), FString::Format(TEXT("Invalid Desc.FilePath {0}."),
                                                                {Desc.FilePath.FilePath}));

    TMap<float, FVector4f> pnts;
    for (int i = 0; i < buf.Num(); ++i) {
        if (buf[i].IsEmpty())
            continue;

        float lnVars[5] = {0.f};
        auto validCnt = sscanf(TCHAR_TO_ANSI(*buf[i]), "%f%f%f%f%f", &lnVars[0], &lnVars[1],
                               &lnVars[2], &lnVars[3], &lnVars[4]);
        if (validCnt != 5 || [&]() {
                for (int i = 0; i < 5; ++i)
                    if (lnVars[i] < 0.f || lnVars[i] >= 255.5f)
                        return true;
                return false;
            }())
            return RetType(
                TInPlaceType<FString>(),
                FString::Format(TEXT("Invalid contents at line {0} in Desc.FilePath {1}."),
                                {i + 1, Desc.FilePath.FilePath}));

        auto &v4 = pnts.Add(lnVars[0]);
        for (int j = 0; j < 4; ++j)
            v4[j] = std::min(std::max(lnVars[j + 1] / 255.f, 0.f), 1.f);
    }

    auto tex = UTexture2D::CreateTransient(Resolution, 1, PF_FloatRGBA, Desc.Name);
    {
        TArray<FFloat16> dat;
        {
            dat.Reserve(Resolution * 4);

            auto pnt = pnts.CreateIterator();
            ++pnt;
            auto prevPnt = pnts.CreateIterator();
            for (int scalar = 0; scalar < Resolution; ++scalar) {
                if (scalar > pnt->Key) {
                    ++prevPnt;
                    ++pnt;
                }
                auto &[s, c] = *pnt;
                auto &[prevS, prevC] = *prevPnt;

                auto k = s == prevS ? 1.f : (scalar - prevS) / (s - prevS);
                auto rgba = (1.f - k) * prevC + k * c;

                dat.Emplace(rgba.X);
                dat.Emplace(rgba.Y);
                dat.Emplace(rgba.Z);
                dat.Emplace(rgba.W);
            }
        }

        tex->Filter = TextureFilter::TF_Bilinear;
        tex->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
        tex->AddressX = tex->AddressY = TextureAddress::TA_Clamp;

        auto *texDat =
            tex->GetPlatformData()->Mips[0].BulkData.Lock(EBulkDataLockFlags::LOCK_READ_WRITE);
        FMemory::Memmove(texDat, dat.GetData(), ElemSz * Resolution);
        tex->GetPlatformData()->Mips[0].BulkData.Unlock();
        tex->UpdateResource();
    }

    auto curve = NewObject<UCurveLinearColor>();
    { 
        std::array<FRichCurve *, 4> curves = {&curve->FloatCurves[0], &curve->FloatCurves[1],
                                              &curve->FloatCurves[2], &curve->FloatCurves[3]};
        for (auto &[scalar, rgba] : pnts)
            for (int32 i = 0; i < 4; ++i)
                curves[i]->AddKey(scalar, rgba[i]);
    }

    return RetType(TInPlaceType<ValueType>(), tex, curve);
}
