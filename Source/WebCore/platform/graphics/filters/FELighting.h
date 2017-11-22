/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
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

#include "Color.h"
#include "Filter.h"
#include "FilterEffect.h"
#include "LightSource.h"
#include <runtime/Uint8ClampedArray.h>

// Common base class for FEDiffuseLighting and FESpecularLighting

namespace WebCore {

struct FELightingPaintingDataForNeon;

class FELighting : public FilterEffect {
public:
    void determineAbsolutePaintRect() override { setAbsolutePaintRect(enclosingIntRect(maxEffectRect())); }

    float surfaceScale() const { return m_surfaceScale; }
    bool setSurfaceScale(float);

    const Color& lightingColor() const { return m_lightingColor; }
    bool setLightingColor(const Color&);

    float kernelUnitLengthX() const { return m_kernelUnitLengthX; }
    bool setKernelUnitLengthX(float);

    float kernelUnitLengthY() const { return m_kernelUnitLengthY; }
    bool setKernelUnitLengthY(float);

    const LightSource& lightSource() const { return m_lightSource.get(); }

protected:
    static const int s_minimalRectDimension = 100 * 100; // Empirical data limit for parallel jobs

    enum LightingType {
        DiffuseLighting,
        SpecularLighting
    };

    struct LightingData {
        // This structure contains only read-only (SMP safe) data
        Uint8ClampedArray* pixels;
        float surfaceScale;
        int widthMultipliedByPixelSize;
        int widthDecreasedByOne;
        int heightDecreasedByOne;

        inline IntSize topLeftNormal(int offset) const;
        inline IntSize topRowNormal(int offset) const;
        inline IntSize topRightNormal(int offset) const;
        inline IntSize leftColumnNormal(int offset) const;
        inline IntSize interiorNormal(int offset) const;
        inline IntSize rightColumnNormal(int offset) const;
        inline IntSize bottomLeftNormal(int offset) const;
        inline IntSize bottomRowNormal(int offset) const;
        inline IntSize bottomRightNormal(int offset) const;
    };

    template<typename Type>
    friend class ParallelJobs;

    struct PlatformApplyGenericParameters {
        FELighting* filter;
        LightingData data;
        LightSource::PaintingData paintingData;
        int yStart;
        int yEnd;
    };

    static void platformApplyGenericWorker(PlatformApplyGenericParameters*);
    static void platformApplyNeonWorker(FELightingPaintingDataForNeon*);

    FELighting(Filter&, LightingType, const Color&, float, float, float, float, float, float, Ref<LightSource>&&);

    bool drawLighting(Uint8ClampedArray*, int, int);
    inline void inlineSetPixel(int offset, LightingData&, LightSource::PaintingData&,
        int lightX, int lightY, float factorX, float factorY, IntSize normalVector);

    // Not worth to inline every occurence of setPixel.
    void setPixel(int offset, LightingData&, LightSource::PaintingData&,
        int lightX, int lightY, float factorX, float factorY, IntSize normalVector);

    void platformApplySoftware() override;

    inline void platformApply(LightingData&, LightSource::PaintingData&);

    inline void platformApplyGenericPaint(LightingData&, LightSource::PaintingData&, int startX, int startY);
    inline void platformApplyGeneric(LightingData&, LightSource::PaintingData&);

#if CPU(ARM_NEON) && CPU(ARM_TRADITIONAL) && COMPILER(GCC_OR_CLANG)
    static int getPowerCoefficients(float exponent);
    inline void platformApplyNeon(LightingData&, LightSource::PaintingData&);
#endif

    LightingType m_lightingType;
    Ref<LightSource> m_lightSource;

    Color m_lightingColor;
    float m_surfaceScale;
    float m_diffuseConstant;
    float m_specularConstant;
    float m_specularExponent;
    float m_kernelUnitLengthX;
    float m_kernelUnitLengthY;
};

} // namespace WebCore

