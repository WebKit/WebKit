/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Zoltan Herczeg
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if CPU(ARM_NEON) && CPU(ARM_TRADITIONAL) && COMPILER(GCC_COMPATIBLE)

#include "FELightingSoftwareApplier.h"
#include "ImageBuffer.h"
#include "PointLightSource.h"
#include "SpotLightSource.h"
#include <wtf/ObjectIdentifier.h>
#include <wtf/ParallelJobs.h>

namespace WebCore {

// Otherwise: Distant Light.
#define FLAG_POINT_LIGHT                 0x01
#define FLAG_SPOT_LIGHT                  0x02
#define FLAG_CONE_EXPONENT_IS_1          0x04

// Otherwise: Diffuse light.
#define FLAG_SPECULAR_LIGHT              0x10
#define FLAG_DIFFUSE_CONST_IS_1          0x20
#define FLAG_SPECULAR_EXPONENT_IS_1      0x40

// Must be aligned to 16 bytes.
struct FELightingFloatArgumentsForNeon {
    float surfaceScale;
    float minusSurfaceScaleDividedByFour;
    float diffuseConstant;
    float padding1;

    float coneCutOffLimit;
    float coneFullLight;
    float coneCutOffRange;
    float constOne;

    float lightX;
    float lightY;
    float lightZ;
    float padding2;

    float directionX;
    float directionY;
    float directionZ;
    float padding3;

    float colorRed;
    float colorGreen;
    float colorBlue;
    float padding4;
};

struct FELightingPaintingDataForNeon {
    unsigned char* pixels;
    float yStart;
    int widthDecreasedByTwo;
    int absoluteHeight;
    // Combination of FLAG constants above.
    int flags;
    int specularExponent;
    int coneExponent;
    FELightingFloatArgumentsForNeon* floatArguments;
    short* paintingConstants;
};

short* feLightingConstantsForNeon();

extern "C" {
void neonDrawLighting(FELightingPaintingDataForNeon*);
}

inline void FELightingSoftwareApplier::applyPlatformNeon(const FELightingSoftwareApplier::LightingData& data, const LightSource::PaintingData& paintingData)
{
    WebCore::FELightingFloatArgumentsForNeon alignas(16) floatArguments;
    WebCore::FELightingPaintingDataForNeon neonData = {
        data.pixels->bytes(),
        1,
        data.width - 2,
        data.height - 2,
        0,
        0,
        0,
        &floatArguments,
        feLightingConstantsForNeon()
    };

    // Set light source arguments.
    floatArguments.constOne = 1;

    auto color = data.lightingColor.toColorTypeLossy<SRGBA<uint8_t>>().resolved();

    floatArguments.colorRed = color.red;
    floatArguments.colorGreen = color.green;
    floatArguments.colorBlue = color.blue;
    floatArguments.padding4 = 0;

    if (data.lightSource->type() == LS_POINT) {
        neonData.flags |= FLAG_POINT_LIGHT;
        const auto& pointLightSource = *static_cast<const PointLightSource*>(data.lightSource);
        floatArguments.lightX = pointLightSource.position().x();
        floatArguments.lightY = pointLightSource.position().y();
        floatArguments.lightZ = pointLightSource.position().z();
        floatArguments.padding2 = 0;
    } else if (data.lightSource->type() == LS_SPOT) {
        neonData.flags |= FLAG_SPOT_LIGHT;
        const auto& spotLightSource = *static_cast<const SpotLightSource*>(data.lightSource);
        floatArguments.lightX = spotLightSource.position().x();
        floatArguments.lightY = spotLightSource.position().y();
        floatArguments.lightZ = spotLightSource.position().z();
        floatArguments.padding2 = 0;

        floatArguments.directionX = paintingData.directionVector.x();
        floatArguments.directionY = paintingData.directionVector.y();
        floatArguments.directionZ = paintingData.directionVector.z();
        floatArguments.padding3 = 0;

        floatArguments.coneCutOffLimit = paintingData.coneCutOffLimit;
        floatArguments.coneFullLight = paintingData.coneFullLight;
        floatArguments.coneCutOffRange = paintingData.coneCutOffLimit - paintingData.coneFullLight;
        neonData.coneExponent = getPowerCoefficients(spotLightSource.specularExponent());
        if (spotLightSource.specularExponent() == 1)
            neonData.flags |= FLAG_CONE_EXPONENT_IS_1;
    } else {
        ASSERT(data.lightSource->type() == LS_DISTANT);
        floatArguments.lightX = paintingData.initialLightingData.lightVector.x();
        floatArguments.lightY = paintingData.initialLightingData.lightVector.y();
        floatArguments.lightZ = paintingData.initialLightingData.lightVector.z();
        floatArguments.padding2 = 1;
    }

    // Set lighting arguments.
    floatArguments.surfaceScale = data.surfaceScale;
    floatArguments.minusSurfaceScaleDividedByFour = -data.surfaceScale / 4;
    if (data.filterType == FilterEffect::Type::FEDiffuseLighting)
        floatArguments.diffuseConstant = data.diffuseConstant;
    else {
        neonData.flags |= FLAG_SPECULAR_LIGHT;
        floatArguments.diffuseConstant = data.specularConstant;
        neonData.specularExponent = getPowerCoefficients(data.specularExponent);
        if (data.specularExponent == 1)
            neonData.flags |= FLAG_SPECULAR_EXPONENT_IS_1;
    }
    if (floatArguments.diffuseConstant == 1)
        neonData.flags |= FLAG_DIFFUSE_CONST_IS_1;

    static constexpr int minimalRectDimension = 100 * 100; // Empirical data limit for parallel jobs
    int optimalThreadNumber = ((data.width - 2) * (data.height - 2)) / minimalRectDimension;
    if (optimalThreadNumber > 1) {
        // Initialize parallel jobs
        ParallelJobs<FELightingPaintingDataForNeon> parallelJobs(&FELightingSoftwareApplier::platformApplyNeonWorker, optimalThreadNumber);

        // Fill the parameter array
        int job = parallelJobs.numberOfJobs();
        if (job > 1) {
            int yStart = 1;
            int yStep = (data.height - 2) / job;
            for (--job; job >= 0; --job) {
                FELightingPaintingDataForNeon& params = parallelJobs.parameter(job);
                params = neonData;
                params.yStart = yStart;
                params.pixels += (yStart - 1) * data.width * 4;
                if (job > 0) {
                    params.absoluteHeight = yStep;
                    yStart += yStep;
                } else
                    params.absoluteHeight = (data.height - 1) - yStart;
            }
            parallelJobs.execute();
            return;
        }
    }

    neonDrawLighting(&neonData);
}

} // namespace WebCore

#endif // CPU(ARM_NEON) && COMPILER(GCC_COMPATIBLE)
