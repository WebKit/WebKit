/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Renata Hodovan <reni@inf.u-szeged.hu>
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#include "FilterEffectApplier.h"
#include "FloatPoint.h"
#include "IntRect.h"
#include "PixelBuffer.h"
#include <JavaScriptCore/Forward.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class FETurbulence;
enum class TurbulenceType : uint8_t;

class FETurbulenceSoftwareApplier final : public FilterEffectConcreteApplier<FETurbulence> {
    WTF_MAKE_TZONE_ALLOCATED(FETurbulenceSoftwareApplier);
    using Base = FilterEffectConcreteApplier<FETurbulence>;

public:
    using Base::Base;

    static const int s_blockSize = 256;
    static const int s_latticeSize = 2 * s_blockSize + 2;

    struct PaintingData {
        TurbulenceType type;
        float baseFrequencyX;
        float baseFrequencyY;
        int numOctaves;
        bool stitchTiles;

        using LatticeSelector = std::array<int, s_latticeSize>;
        LatticeSelector latticeSelector;

        using ChannelGradient = std::array<std::array<float, 2>, 2 * s_blockSize + 2>;
        std::array<ChannelGradient, 4> gradient;

        PaintingData(TurbulenceType, float baseFrequencyX, float baseFrequencyY, int numOctaves, long seed, bool stitchTiles);
    };

    struct StitchData {
        int width { 0 }; // How much to subtract to wrap for stitching.
        int height { 0 };
        int wrapX { 0 }; // Minimum value to wrap.
        int wrapY { 0 };
    };

    static StitchData computeStitching(IntSize tileSize, float& baseFrequencyX, float& baseFrequencyY, bool stitchTiles);

private:
    bool apply(const Filter&, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const final;

    struct ApplyParameters {
        IntRect filterRegion;
        FloatSize filterScale;
        PixelBuffer* pixelBuffer;
        PaintingData* paintingData;
        StitchData stitchData;
        int startY;
        int endY;
    };

    static long random(long& seed);

    static inline float smoothCurve(float t) { return t * t * (3 - 2 * t); }
    static inline float linearInterpolation(float t, float a, float b) { return a + t * (b - a); }

    static ColorComponents<float, 4> noise2D(const PaintingData&, const StitchData&, const FloatPoint& noiseVector);
    static ColorComponents<uint8_t, 4> toIntBasedColorComponents(const ColorComponents<float, 4>& floatComponents);
    static ColorComponents<uint8_t, 4> calculateTurbulenceValueForPoint(const PaintingData&, StitchData, const FloatPoint&);

    static void applyPlatformGeneric(const IntRect& filterRegion, const FloatSize& filterScale, PixelBuffer&, const PaintingData&, StitchData, int startY, int endY);
    static void applyPlatformWorker(ApplyParameters*);
    static void applyPlatform(const IntRect& filterRegion, const FloatSize& filterScale, PixelBuffer&, PaintingData&, StitchData&);
};

} // namespace WebCore
