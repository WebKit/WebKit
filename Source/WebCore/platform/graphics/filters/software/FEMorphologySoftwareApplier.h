/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2021-2022 Apple, Inc. All rights reserved.
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
#include "ColorTypes.h"
#include "FilterEffectApplier.h"
#include "PixelBuffer.h"
#include <JavaScriptCore/Forward.h>

namespace WebCore {

class FEMorphology;
enum class MorphologyOperatorType : uint8_t;

class FEMorphologySoftwareApplier final : public FilterEffectConcreteApplier<FEMorphology> {
    WTF_MAKE_FAST_ALLOCATED;
    using Base = FilterEffectConcreteApplier<FEMorphology>;

public:
    using Base::Base;

private:
    bool apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const final;

    using ColumnExtrema = Vector<ColorComponents<uint8_t, 4>, 16>;

    struct PaintingData {
        MorphologyOperatorType type;
        int radiusX;
        int radiusY;
        const PixelBuffer* srcPixelBuffer;
        PixelBuffer* dstPixelBuffer;
        int width;
        int height;
    };

    struct ApplyParameters {
        const PaintingData* paintingData;
        int startY;
        int endY;
    };

    static inline int pixelArrayIndex(int x, int y, int width) { return (y * width + x) * 4; }
    static inline PackedColor::RGBA makePixelValueFromColorComponents(const ColorComponents<uint8_t, 4>& components) { return PackedColor::RGBA { makeFromComponents<SRGBA<uint8_t>>(components) }; }

    static inline ColorComponents<uint8_t, 4> makeColorComponentsfromPixelValue(PackedColor::RGBA pixel) { return asColorComponents(asSRGBA(pixel).resolved()); }
    static inline ColorComponents<uint8_t, 4> minOrMax(const ColorComponents<uint8_t, 4>& a, const ColorComponents<uint8_t, 4>& b, MorphologyOperatorType);
    static inline ColorComponents<uint8_t, 4> columnExtremum(const PixelBuffer& srcPixelBuffer, int x, int yStart, int yEnd, int width, MorphologyOperatorType);
    static inline ColorComponents<uint8_t, 4> kernelExtremum(const ColumnExtrema& kernel, MorphologyOperatorType);

    static void applyPlatformGeneric(const PaintingData&, int startY, int endY);
    static void applyPlatformWorker(ApplyParameters*);
    static void applyPlatform(const PaintingData&);
};

} // namespace WebCore
