/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
 * Copyright (C) 2018-2024 Apple, Inc. All rights reserved.
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
#include "FilterEffect.h"
#include "FilterEffectApplier.h"
#include "FilterImageVector.h"
#include "LightSource.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class FELighting;

class FELightingSoftwareApplier : public FilterEffectConcreteApplier<FELighting> {
    WTF_MAKE_TZONE_ALLOCATED(FELightingSoftwareApplier);
    using Base = FilterEffectConcreteApplier<FELighting>;

protected:
    using Base::Base;

    static constexpr int minimalRectDimension = 100 * 100; // Empirical data limit for parallel jobs
    static constexpr int cPixelSize = 4;
    static constexpr int cAlphaChannelOffset = 3;
    static constexpr uint8_t cOpaqueAlpha = static_cast<uint8_t>(0xFF);

    // These factors and the normal coefficients come from the table under https://www.w3.org/TR/SVG/filters.html#feDiffuseLightingElement.
    static constexpr float cFactor1div2 = -1 / 2.f;
    static constexpr float cFactor1div3 = -1 / 3.f;
    static constexpr float cFactor1div4 = -1 / 4.f;
    static constexpr float cFactor2div3 = -2 / 3.f;

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
        const Filter* filter;
        const FilterImage* result;
        FilterEffect::Type filterType;
        Color lightingColor;
        float surfaceScale;
        float diffuseConstant;
        float specularConstant;
        float specularExponent;
        const LightSource* lightSource;
        const DestinationColorSpace* operatingColorSpace;

        PixelBuffer* pixels;
        int widthMultipliedByPixelSize;
        int width;
        int height;

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

    static void setPixelInternal(int offset, const LightingData&, const LightSource::PaintingData&, int x, int y, float factorX, float factorY, IntSize normal2DVector, float alpha);
    static void setPixel(int offset, const LightingData&, const LightSource::PaintingData&, int x, int y, float factorX, float factorY, IntSize normal2DVector);

    virtual void applyPlatformParallel(const LightingData&, const LightSource::PaintingData&) const = 0;
    void applyPlatform(const LightingData&) const;
    bool apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const final;
};

} // namespace WebCore
