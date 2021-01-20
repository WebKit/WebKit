/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Renata Hodovan <reni@inf.u-szeged.hu>
 * Copyright (C) 2017 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "ColorComponents.h"
#include "FilterEffect.h"
#include "Filter.h"

namespace WebCore {

enum class TurbulenceType {
    Unknown,
    FractalNoise,
    Turbulence
};

class FETurbulence : public FilterEffect {
public:
    static Ref<FETurbulence> create(Filter&, TurbulenceType, float, float, int, float, bool);

    TurbulenceType type() const { return m_type; }
    bool setType(TurbulenceType);

    float baseFrequencyX() const { return m_baseFrequencyX; }
    bool setBaseFrequencyX(float);

    float baseFrequencyY() const { return m_baseFrequencyY; }
    bool setBaseFrequencyY(float);

    float seed() const { return m_seed; }
    bool setSeed(float);

    int numOctaves() const { return m_numOctaves; }
    bool setNumOctaves(int);

    bool stitchTiles() const { return m_stitchTiles; }
    bool setStitchTiles(bool);

private:
    static const int s_blockSize = 256;
    static const int s_blockMask = s_blockSize - 1;

    static const int s_minimalRectDimension = (100 * 100); // Empirical data limit for parallel jobs.

    struct PaintingData {
        PaintingData(long paintingSeed, const IntSize& paintingSize, float baseFrequencyX, float baseFrequencyY)
            : seed(paintingSeed)
            , filterSize(paintingSize)
            , baseFrequencyX(baseFrequencyX)
            , baseFrequencyY(baseFrequencyY)
        {
        }

        long seed;
        int latticeSelector[2 * s_blockSize + 2];
        float gradient[4][2 * s_blockSize + 2][2];
        IntSize filterSize;
        float baseFrequencyX;
        float baseFrequencyY;

        inline long random();
    };

    struct StitchData {
        StitchData() = default;

        int width { 0 }; // How much to subtract to wrap for stitching.
        int wrapX { 0 }; // Minimum value to wrap.
        int height { 0 };
        int wrapY { 0 };
    };

    template<typename Type>
    friend class ParallelJobs;

    struct FillRegionParameters {
        FETurbulence* filter;
        Uint8ClampedArray* pixelArray;
        PaintingData* paintingData;
        StitchData stitchData;
        int startY;
        int endY;
    };

    static void fillRegionWorker(FillRegionParameters*);

    FETurbulence(Filter&, TurbulenceType, float, float, int, float, bool);

    const char* filterName() const final { return "FETurbulence"; }

    void platformApplySoftware() override;
    void determineAbsolutePaintRect() override { setAbsolutePaintRect(enclosingIntRect(maxEffectRect())); }
    WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType) const override;

    void initPaint(PaintingData&);
    StitchData computeStitching(IntSize tileSize, float& baseFrequencyX, float& baseFrequencyY) const;
    ColorComponents<float> noise2D(const PaintingData&, const StitchData&, const FloatPoint&) const;
    ColorComponents<uint8_t> calculateTurbulenceValueForPoint(const PaintingData&, StitchData, const FloatPoint&) const;
    void fillRegion(Uint8ClampedArray&, const PaintingData&, StitchData, int startY, int endY) const;

    static void fillRegionWorker(void*);

    TurbulenceType m_type;
    float m_baseFrequencyX;
    float m_baseFrequencyY;
    int m_numOctaves;
    float m_seed;
    bool m_stitchTiles;
};

} // namespace WebCore

