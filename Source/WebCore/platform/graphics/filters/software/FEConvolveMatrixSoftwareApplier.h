/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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

#include "FilterEffectApplier.h"
#include "IntPoint.h"
#include "IntSize.h"
#include "PixelBuffer.h"
#include <JavaScriptCore/TypedArrayAdaptersForwardDeclarations.h>
#include <wtf/Vector.h>

namespace WebCore {

class FEConvolveMatrix;
enum class EdgeModeType : uint8_t;

class FEConvolveMatrixSoftwareApplier final : public FilterEffectConcreteApplier<FEConvolveMatrix> {
    WTF_MAKE_FAST_ALLOCATED;
    using Base = FilterEffectConcreteApplier<FEConvolveMatrix>;

public:
    FEConvolveMatrixSoftwareApplier(const FEConvolveMatrix& effect);

private:
    bool apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const final;

    struct PaintingData {
        const PixelBuffer& sourcePixelBuffer;
        PixelBuffer& destinationPixelBuffer;
        int width;
        int height;

        IntSize kernelSize;
        float divisor;
        float bias;
        IntPoint targetOffset;
        EdgeModeType edgeMode;
        bool preserveAlpha;
        Vector<float> kernelMatrix;
    };

    static inline uint8_t clampRGBAValue(float channel, uint8_t max = 255);
    static inline void setDestinationPixels(const PixelBuffer& sourcePixelBuffer, PixelBuffer& destinationPixelBuffer, int& pixel, float* totals, float divisor, float bias, bool preserveAlphaValues);

    static inline int getPixelValue(const PaintingData&, int x, int y);

    static inline void setInteriorPixels(PaintingData&, int clipRight, int clipBottom, int yStart, int yEnd);
    static inline void setOuterPixels(PaintingData&, int x1, int y1, int x2, int y2);
    static void setInteriorPixels(PaintingData&, int clipRight, int clipBottom);
    void applyPlatform(PaintingData&) const;
};

} // namespace WebCore
