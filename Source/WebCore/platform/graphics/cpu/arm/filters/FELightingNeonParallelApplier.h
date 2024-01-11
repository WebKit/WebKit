/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Zoltan Herczeg
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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

#if (CPU(ARM_NEON) && CPU(ARM_TRADITIONAL) && COMPILER(GCC_COMPATIBLE))

#include "FELightingSoftwareApplier.h"

namespace WebCore {

class FELightingNeonParallelApplier final : public FELightingSoftwareApplier {
    WTF_MAKE_FAST_ALLOCATED;

public:
    using FELightingSoftwareApplier::FELightingSoftwareApplier;

    // Must be aligned to 16 bytes.
    struct FloatArguments {
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

    struct ApplyParameters {
        const unsigned char* pixels;
        float yStart;
        int widthDecreasedByTwo;
        int absoluteHeight;
        // Combination of FLAG constants above.
        int flags;
        int specularExponent;
        int coneExponent;
        FloatArguments* floatArguments;
        const short* paintingConstants;
    };

private:
    static const short* feLightingConstants();
    static int getPowerCoefficients(float exponent);
    static void applyPlatformWorker(ApplyParameters*);

    void applyPlatformParallel(const LightingData&, const LightSource::PaintingData&) const final;
};

} // namespace WebCore

#endif // (CPU(ARM_NEON) && CPU(ARM_TRADITIONAL) && COMPILER(GCC_COMPATIBLE))
