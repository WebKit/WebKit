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
#include <JavaScriptCore/Uint8ClampedArray.h>

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
    
    struct AlphaWindow {
        uint8_t alpha[3][3] { };
        
        // The implementations are lined up to make comparing indices easier.
        uint8_t topLeft() const             { return alpha[0][0]; }
        uint8_t left() const                { return alpha[1][0]; }
        uint8_t bottomLeft() const          { return alpha[2][0]; }

        uint8_t top() const                 { return alpha[0][1]; }
        uint8_t center() const              { return alpha[1][1]; }
        uint8_t bottom() const              { return alpha[2][1]; }

        void setTop(uint8_t value)          { alpha[0][1] = value; }
        void setCenter(uint8_t value)       { alpha[1][1] = value; }
        void setBottom(uint8_t value)       { alpha[2][1] = value; }

        void setTopRight(uint8_t value)     { alpha[0][2] = value; }
        void setRight(uint8_t value)        { alpha[1][2] = value; }
        void setBottomRight(uint8_t value)  { alpha[2][2] = value; }

        static void shiftRow(uint8_t alpha[3])
        {
            alpha[0] = alpha[1];
            alpha[1] = alpha[2];
        }
        
        void shift()
        {
            shiftRow(alpha[0]);
            shiftRow(alpha[1]);
            shiftRow(alpha[2]);
        }
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
        inline IntSize interiorNormal(int offset, AlphaWindow&) const;
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

    const char* filterName() const final { return "FELighting"; }

    bool drawLighting(Uint8ClampedArray&, int, int);

    void setPixel(int offset, const LightingData&, const LightSource::PaintingData&, int x, int y, float factorX, float factorY, IntSize normalVector);
    void setPixelInternal(int offset, const LightingData&, const LightSource::PaintingData&, int x, int y, float factorX, float factorY, IntSize normalVector, float alpha);

    void platformApplySoftware() override;

    void platformApply(const LightingData&, const LightSource::PaintingData&);

    void platformApplyGenericPaint(const LightingData&, const LightSource::PaintingData&, int startX, int startY);
    void platformApplyGeneric(const LightingData&, const LightSource::PaintingData&);

#if CPU(ARM_NEON) && CPU(ARM_TRADITIONAL) && COMPILER(GCC_COMPATIBLE)
    static int getPowerCoefficients(float exponent);
    inline void platformApplyNeon(const LightingData&, const LightSource::PaintingData&);
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

