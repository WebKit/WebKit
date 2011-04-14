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

#ifndef FELightingNeon_h
#define FELightingNeon_h

#include <wtf/Platform.h>

#if CPU(ARM_NEON) && COMPILER(GCC)

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
    int widthDecreasedByTwo;
    int heightDecreasedByTwo;
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

} // namespace WebCore

#endif // CPU(ARM_NEON) && COMPILER(GCC)

#endif // FELightingNeon_h
