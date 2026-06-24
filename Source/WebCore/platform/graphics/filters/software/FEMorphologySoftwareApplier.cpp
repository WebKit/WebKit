/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) Apple Inc. 2017-2026 All rights reserved.
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

#include "config.h"
#include "FEMorphologySoftwareApplier.h"

#include "FEMorphology.h"
#include "Filter.h"
#include "FilterEffectSoftwareParallelApplier.h"
#include "PixelBuffer.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEMorphologySoftwareApplier);

inline ColorComponents<uint8_t, 4> FEMorphologySoftwareApplier::minOrMax(const ColorComponents<uint8_t, 4>& a, const ColorComponents<uint8_t, 4>& b, MorphologyOperatorType type)
{
    if (type == MorphologyOperatorType::Erode)
        return perComponentMin(a, b);

    return perComponentMax(a, b);
}

inline ColorComponents<uint8_t, 4> FEMorphologySoftwareApplier::columnExtremum(const PixelBuffer& sourceBuffer, int x, int yStart, int yEnd, int width, MorphologyOperatorType type)
{
    auto extremum = makeColorComponentsfromPixelValue(PackedColor::RGBA { reinterpretCastSpanStartTo<const unsigned>(sourceBuffer.bytes().subspan(pixelArrayIndex(x, yStart, width))) });

    for (int y = yStart + 1; y < yEnd; ++y) {
        auto pixel = makeColorComponentsfromPixelValue(PackedColor::RGBA { reinterpretCastSpanStartTo<const unsigned>(sourceBuffer.bytes().subspan(pixelArrayIndex(x, y, width))) });
        extremum = minOrMax(extremum, pixel, type);
    }
    return extremum;
}

inline ColorComponents<uint8_t, 4> FEMorphologySoftwareApplier::kernelExtremum(const ColumnExtrema& kernel, MorphologyOperatorType type)
{
    auto extremum = kernel[0];
    for (size_t i = 1; i < kernel.size(); ++i)
        extremum = minOrMax(extremum, kernel[i], type);

    return extremum;
}

void FEMorphologySoftwareApplier::applyPlatformGeneric(PixelBuffer& sourceBuffer, PixelBuffer& destinationBuffer, const IntSize& sourceSize, const IntRect& destinationRect, MorphologyOperatorType type, const IntSize& radius)
{
    const int radiusX = radius.width();
    const int radiusY = radius.height();
    const int startY = destinationRect.y();
    const int endY = destinationRect.maxY();
    const int sourceWidth = sourceSize.width();
    const int sourceHeight = sourceSize.height();

    ASSERT(radiusX <= sourceWidth || radiusY <= sourceHeight);
    ASSERT(destinationBuffer.size().width() <= sourceBuffer.size().width());
    ASSERT(destinationBuffer.size().height() <= sourceBuffer.size().height());

    ColumnExtrema extrema;
    extrema.reserveInitialCapacity(2 * radiusX + 1);

    for (int y = startY; y < endY; ++y) {
        int yRadiusStart = std::max(0, y - radiusY);
        int yRadiusEnd = std::min(sourceHeight, y + radiusY + 1);

        extrema.shrink(0);

        // We start at the left edge, so compute extreme for the radiusX columns.
        for (int x = 0; x < radiusX; ++x)
            extrema.append(columnExtremum(sourceBuffer, x, yRadiusStart, yRadiusEnd, sourceWidth, type));

        // Kernel is filled, get extrema of next column
        for (int x = 0; x < sourceWidth; ++x) {
            if (x < sourceWidth - radiusX)
                extrema.append(columnExtremum(sourceBuffer, x + radiusX, yRadiusStart, yRadiusEnd, sourceWidth, type));

            if (x > radiusX)
                extrema.removeAt(0);

            unsigned& destPixel = reinterpretCastSpanStartTo<unsigned>(destinationBuffer.bytes().subspan(pixelArrayIndex(x, y - startY, sourceWidth)));
            destPixel = makePixelValueFromColorComponents(kernelExtremum(extrema, type)).value;
        }
    }
}

void FEMorphologySoftwareApplier::applyPlatformWorker(ApplyParameters* params)
{
    RELEASE_ASSERT(params && params->sourceBuffer && params->destinationBuffer);
    Ref sourceBuffer = *params->sourceBuffer;
    Ref destinationBuffer = *params->destinationBuffer;
    applyPlatformGeneric(sourceBuffer, destinationBuffer, params->sourceSize, params->destinationRect, params->type, params->radius);
}

bool FEMorphologySoftwareApplier::applyPlatform(PixelBuffer& sourceBuffer, PixelBuffer& destinationBuffer, MorphologyOperatorType type, const IntSize& radius)
{
    // Empirically, runtime is approximately linear over reasonable kernel sizes with a slope of about 0.65.
    float kernelFactor = sqrt(radius.width() * radius.height()) * 0.65;
    int kernelSizeY = 2 * radius.height();
    int extraHeight = 3 * kernelSizeY * 0.5f;

    static const int minimalArea = (160 * 160); // Empirical data limit for parallel jobs

    IntSize paintSize = sourceBuffer.size();
    IntRect paintRect = { { }, paintSize };
    unsigned maxNumThreads = paintSize.height() / 8;
    unsigned optimalThreadNumber = std::min<unsigned>((paintSize.unclampedArea() * kernelFactor) / (minimalArea + extraHeight * paintSize.width()), maxNumThreads);

    if (optimalThreadNumber > 1) {
        ApplyParameters params {
            .sourceBuffer = sourceBuffer,
            .destinationBuffer = destinationBuffer,
            .sourceSize = paintSize,
            .destinationRect = paintRect,
            .type = type,
            .radius = radius
        };

        if (applyPlatformParallel(&FEMorphologySoftwareApplier::applyPlatformWorker, optimalThreadNumber, params, extraHeight))
            return true;

        // Fallback to single thread model
    }

    applyPlatformGeneric(sourceBuffer, destinationBuffer, paintSize, paintRect, type, radius);
    return true;
}

bool FEMorphologySoftwareApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();

    auto destinationBuffer = result.pixelBuffer(AlphaPremultiplication::Premultiplied);
    if (!destinationBuffer)
        return false;

    auto isDegenerate = [](const IntSize& absoluteRadius) -> bool {
        return absoluteRadius.width() < 0 || absoluteRadius.height() < 0 || absoluteRadius.isZero();
    };

    auto effectDrawingRect = result.absoluteImageRectRelativeTo(input);

    auto radius = filter.resolvedSize({ m_effect->radiusX(), m_effect->radiusY() });
    auto absoluteRadius = flooredIntSize(filter.scaledByFilterScale(radius));

    if (isDegenerate(absoluteRadius)) {
        input.copyPixelBuffer(*destinationBuffer, effectDrawingRect);
        return true;
    }

    int radiusX = std::min(effectDrawingRect.width() - 1, absoluteRadius.width());
    int radiusY = std::min(effectDrawingRect.height() - 1, absoluteRadius.height());

    if (isDegenerate({ radiusX, radiusY })) {
        input.copyPixelBuffer(*destinationBuffer, effectDrawingRect);
        return true;
    }

    RefPtr sourceBuffer = input.getPixelBuffer(AlphaPremultiplication::Premultiplied, effectDrawingRect, m_effect->operatingColorSpace());
    if (!sourceBuffer)
        return false;

    return applyPlatform(*sourceBuffer, *destinationBuffer, m_effect->morphologyOperator(), { radiusX, radiusY });
}

} // namespace WebCore
