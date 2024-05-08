// Author: Kouek Kou

#pragma once

#include <array>

#include "CoreMinimal.h"
#include "Engine/VolumeTexture.h"
#include "RHIGPUReadback.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

#include "Util.h"

#include "Data.h"

class VIS4EARTH_API FTFPreIntegrator {
  public:
    struct Parameters {
        TObjectPtr<UTexture2D> TransferFunctionTexture;
    };
    static TArray<FFloat16> Exec(const Parameters &Params) {
        auto tfDat = reinterpret_cast<std::array<FFloat16, 4> *>(
            Params.TransferFunctionTexture->PlatformData->Mips[0].BulkData.Lock(
                EBulkDataLockFlags::LOCK_READ_ONLY));

        TArray<std::array<float, 4>> tfIntDat;
        tfIntDat.SetNum(TransferFunctionData::Resolution);
        {
            tfIntDat[0][0] = (*tfDat)[0];
            tfIntDat[0][1] = (*tfDat)[1];
            tfIntDat[0][2] = (*tfDat)[2];
            tfIntDat[0][3] = (*tfDat)[3];
            for (int32 i = 1; i < TransferFunctionData::Resolution; ++i) {
                auto a = .5f * (tfDat[i - 1][3] + tfDat[i][3]);
                auto r = .5f * (tfDat[i - 1][0] + tfDat[i][0]) * a;
                auto g = .5f * (tfDat[i - 1][1] + tfDat[i][1]) * a;
                auto b = .5f * (tfDat[i - 1][2] + tfDat[i][2]) * a;

                tfIntDat[i][0] = tfIntDat[i - 1][0] + r;
                tfIntDat[i][1] = tfIntDat[i - 1][1] + g;
                tfIntDat[i][2] = tfIntDat[i - 1][2] + b;
                tfIntDat[i][3] = tfIntDat[i - 1][3] + a;
            }
        }

        TArray<FFloat16> tfPreIntDat;
        tfPreIntDat.SetNum(TransferFunctionData::Resolution * TransferFunctionData::Resolution * 4);
        auto tfPreIntDatPtr = reinterpret_cast<std::array<FFloat16, 4> *>(tfPreIntDat.GetData());
        for (int32 sf = 0; sf < TransferFunctionData::Resolution; ++sf)
            for (int32 sb = 0; sb < TransferFunctionData::Resolution; ++sb) {
                auto sMin = sf;
                auto sMax = sb;
                if (sf > sb)
                    std::swap(sMin, sMax);

                if (sMin == sMax) {
                    auto a = tfDat[sMin][3].GetFloat();
                    (*tfPreIntDatPtr)[0] = tfDat[sMin][0] * a;
                    (*tfPreIntDatPtr)[1] = tfDat[sMin][1] * a;
                    (*tfPreIntDatPtr)[2] = tfDat[sMin][2] * a;
                    (*tfPreIntDatPtr)[3] = 1.f - FMath::Exp(-a);
                } else {
                    auto factor = 1.f / (sMax - sMin);
                    (*tfPreIntDatPtr)[0] = (tfIntDat[sMax][0] - tfIntDat[sMin][0]) * factor;
                    (*tfPreIntDatPtr)[1] = (tfIntDat[sMax][1] - tfIntDat[sMin][1]) * factor;
                    (*tfPreIntDatPtr)[2] = (tfIntDat[sMax][2] - tfIntDat[sMin][2]) * factor;
                    (*tfPreIntDatPtr)[3] =
                        1.f - FMath::Exp((tfIntDat[sMin][3] - tfIntDat[sMax][3]) * factor);
                }

                ++tfPreIntDatPtr;
            }
        Params.TransferFunctionTexture->PlatformData->Mips[0].BulkData.Unlock();

        return tfPreIntDat;
    }
};
