/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) Apple Inc. 2017-2018 All rights reserved.
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
#include "FEMorphology.h"

#include "ColorComponents.h"
#include "ColorTypes.h"
#include "Filter.h"
#include "PixelBuffer.h"
#include <wtf/ParallelJobs.h>
#include <wtf/Vector.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

FEMorphology::FEMorphology(Filter& filter, MorphologyOperatorType type, float radiusX, float radiusY)
    : FilterEffect(filter, Type::Morphology)
    , m_type(type)
    , m_radiusX(radiusX)
    , m_radiusY(radiusY)
{
}

Ref<FEMorphology> FEMorphology::create(Filter& filter, MorphologyOperatorType type, float radiusX, float radiusY)
{
    return adoptRef(*new FEMorphology(filter, type, radiusX, radiusY));
}

bool FEMorphology::setMorphologyOperator(MorphologyOperatorType type)
{
    if (m_type == type)
        return false;
    m_type = type;
    return true;
}

bool FEMorphology::setRadiusX(float radiusX)
{
    if (m_radiusX == radiusX)
        return false;
    m_radiusX = radiusX;
    return true;
}

bool FEMorphology::setRadiusY(float radiusY)
{
    if (m_radiusY == radiusY)
        return false;
    m_radiusY = radiusY;
    return true;
}

void FEMorphology::determineAbsolutePaintRect()
{
    FloatRect paintRect = inputEffect(0)->absolutePaintRect();
    Filter& filter = this->filter();
    paintRect.inflate(filter.scaledByFilterResolution({ m_radiusX, m_radiusY }));
    if (clipsToBounds())
        paintRect.intersect(maxEffectRect());
    else
        paintRect.unite(maxEffectRect());
    setAbsolutePaintRect(enclosingIntRect(paintRect));
}

static inline int pixelArrayIndex(int x, int y, int width)
{
    return (y * width + x) * 4;
}

inline ColorComponents<uint8_t, 4> makeColorComponentsfromPixelValue(PackedColor::RGBA pixel)
{
    return asColorComponents(asSRGBA(pixel));
}

inline PackedColor::RGBA makePixelValueFromColorComponents(const ColorComponents<uint8_t, 4>& components)
{
    return PackedColor::RGBA { makeFromComponents<SRGBA<uint8_t>>(components) };
}

template<MorphologyOperatorType type>
ALWAYS_INLINE ColorComponents<uint8_t, 4> minOrMax(const ColorComponents<uint8_t, 4>& a, const ColorComponents<uint8_t, 4>& b)
{
    if (type == FEMORPHOLOGY_OPERATOR_ERODE)
        return perComponentMin(a, b);

    return perComponentMax(a, b);
}

template<MorphologyOperatorType type>
ALWAYS_INLINE ColorComponents<uint8_t, 4> columnExtremum(const Uint8ClampedArray& srcPixelArray, int x, int yStart, int yEnd, int width)
{
    auto extremum = makeColorComponentsfromPixelValue(PackedColor::RGBA { *reinterpret_cast<const unsigned*>(srcPixelArray.data() + pixelArrayIndex(x, yStart, width)) });

    for (int y = yStart + 1; y < yEnd; ++y) {
        auto pixel = makeColorComponentsfromPixelValue(PackedColor::RGBA { *reinterpret_cast<const unsigned*>(srcPixelArray.data() + pixelArrayIndex(x, y, width)) });
        extremum = minOrMax<type>(extremum, pixel);
    }
    return extremum;
}

ALWAYS_INLINE ColorComponents<uint8_t, 4> columnExtremum(const Uint8ClampedArray& srcPixelArray, int x, int yStart, int yEnd, int width, MorphologyOperatorType type)
{
    if (type == FEMORPHOLOGY_OPERATOR_ERODE)
        return columnExtremum<FEMORPHOLOGY_OPERATOR_ERODE>(srcPixelArray, x, yStart, yEnd, width);

    return columnExtremum<FEMORPHOLOGY_OPERATOR_DILATE>(srcPixelArray, x, yStart, yEnd, width);
}

using ColumnExtrema = Vector<ColorComponents<uint8_t, 4>, 16>;

template<MorphologyOperatorType type>
ALWAYS_INLINE ColorComponents<uint8_t, 4> kernelExtremum(const ColumnExtrema& kernel)
{
    auto extremum = kernel[0];
    for (size_t i = 1; i < kernel.size(); ++i)
        extremum = minOrMax<type>(extremum, kernel[i]);

    return extremum;
}

ALWAYS_INLINE ColorComponents<uint8_t, 4> kernelExtremum(const ColumnExtrema& kernel, MorphologyOperatorType type)
{
    if (type == FEMORPHOLOGY_OPERATOR_ERODE)
        return kernelExtremum<FEMORPHOLOGY_OPERATOR_ERODE>(kernel);

    return kernelExtremum<FEMORPHOLOGY_OPERATOR_DILATE>(kernel);
}

void FEMorphology::platformApplyGeneric(const PaintingData& paintingData, int startY, int endY)
{
    ASSERT(endY > startY);

    const auto& srcPixelArray = *paintingData.srcPixelArray;
    auto& dstPixelArray = *paintingData.dstPixelArray;

    const int radiusX = paintingData.radiusX;
    const int radiusY = paintingData.radiusY;
    const int width = paintingData.width;
    const int height = paintingData.height;

    ASSERT(radiusX <= width || radiusY <= height);
    ASSERT(startY >= 0 && endY <= height && startY < endY);

    ColumnExtrema extrema;
    extrema.reserveInitialCapacity(2 * radiusX + 1);

    for (int y = startY; y < endY; ++y) {
        int yRadiusStart = std::max(0, y - radiusY);
        int yRadiusEnd = std::min(height, y + radiusY + 1);

        extrema.shrink(0);

        // We start at the left edge, so compute extreme for the radiusX columns.
        for (int x = 0; x < radiusX; ++x)
            extrema.append(columnExtremum(srcPixelArray, x, yRadiusStart, yRadiusEnd, width, m_type));

        // Kernel is filled, get extrema of next column
        for (int x = 0; x < width; ++x) {
            if (x < width - radiusX)
                extrema.append(columnExtremum(srcPixelArray, x + radiusX, yRadiusStart, yRadiusEnd, width, m_type));

            if (x > radiusX)
                extrema.remove(0);

            unsigned* destPixel = reinterpret_cast<unsigned*>(dstPixelArray.data() + pixelArrayIndex(x, y, width));
            *destPixel = makePixelValueFromColorComponents(kernelExtremum(extrema, m_type)).value;
        }
    }
}

void FEMorphology::platformApplyWorker(PlatformApplyParameters* param)
{
    param->filter->platformApplyGeneric(*param->paintingData, param->startY, param->endY);
}

void FEMorphology::platformApply(const PaintingData& paintingData)
{
    // Empirically, runtime is approximately linear over reasonable kernel sizes with a slope of about 0.65.
    float kernelFactor = sqrt(paintingData.radiusX * paintingData.radiusY) * 0.65;

    static const int minimalArea = (160 * 160); // Empirical data limit for parallel jobs
    
    unsigned maxNumThreads = paintingData.height / 8;
    unsigned optimalThreadNumber = std::min<unsigned>((paintingData.width * paintingData.height * kernelFactor) / minimalArea, maxNumThreads);
    if (optimalThreadNumber > 1) {
        WTF::ParallelJobs<PlatformApplyParameters> parallelJobs(&WebCore::FEMorphology::platformApplyWorker, optimalThreadNumber);
        auto numOfThreads = parallelJobs.numberOfJobs();
        if (numOfThreads > 1) {
            // Split the job into "jobSize"-sized jobs but there a few jobs that need to be slightly larger since
            // jobSize * jobs < total size. These extras are handled by the remainder "jobsWithExtra".
            int jobSize = paintingData.height / numOfThreads;
            int jobsWithExtra = paintingData.height % numOfThreads;
            int currentY = 0;
            for (int job = numOfThreads - 1; job >= 0; --job) {
                PlatformApplyParameters& param = parallelJobs.parameter(job);
                param.filter = this;
                param.startY = currentY;
                currentY += job < jobsWithExtra ? jobSize + 1 : jobSize;
                param.endY = currentY;
                param.paintingData = &paintingData;
            }
            parallelJobs.execute();
            return;
        }
        // Fallback to single thread model
    }

    platformApplyGeneric(paintingData, 0, paintingData.height);
}

bool FEMorphology::platformApplyDegenerate(Uint8ClampedArray& dstPixelArray, const IntRect& imageRect, int radiusX, int radiusY)
{
    if (radiusX < 0 || radiusY < 0 || (!radiusX && !radiusY)) {
        FilterEffect* in = inputEffect(0);
        in->copyPremultipliedResult(dstPixelArray, imageRect, operatingColorSpace());
        return true;
    }
    return false;
}

void FEMorphology::platformApplySoftware()
{
    FilterEffect* in = inputEffect(0);

    auto& destinationPixelBuffer = createPremultipliedImageResult();
    if (!destinationPixelBuffer)
        return;

    auto& destinationPixelArray = destinationPixelBuffer->data();

    setIsAlphaImage(in->isAlphaImage());

    IntRect effectDrawingRect = requestedRegionOfInputPixelBuffer(in->absolutePaintRect());

    IntSize radius = flooredIntSize(FloatSize(m_radiusX, m_radiusY));
    if (platformApplyDegenerate(destinationPixelArray, effectDrawingRect, radius.width(), radius.height()))
        return;

    Filter& filter = this->filter();
    auto sourcePixelArray = in->premultipliedResult(effectDrawingRect, operatingColorSpace());
    if (!sourcePixelArray)
        return;

    radius = flooredIntSize(filter.scaledByFilterResolution({ m_radiusX, m_radiusY }));
    int radiusX = std::min(effectDrawingRect.width() - 1, radius.width());
    int radiusY = std::min(effectDrawingRect.height() - 1, radius.height());

    if (platformApplyDegenerate(destinationPixelArray, effectDrawingRect, radiusX, radiusY))
        return;
    
    PaintingData paintingData;
    paintingData.srcPixelArray = sourcePixelArray.get();
    paintingData.dstPixelArray = &destinationPixelArray;
    paintingData.width = ceilf(effectDrawingRect.width() * filter.filterScale());
    paintingData.height = ceilf(effectDrawingRect.height() * filter.filterScale());
    paintingData.radiusX = ceilf(radiusX * filter.filterScale());
    paintingData.radiusY = ceilf(radiusY * filter.filterScale());

    platformApply(paintingData);
}

static TextStream& operator<<(TextStream& ts, const MorphologyOperatorType& type)
{
    switch (type) {
    case FEMORPHOLOGY_OPERATOR_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case FEMORPHOLOGY_OPERATOR_ERODE:
        ts << "ERODE";
        break;
    case FEMORPHOLOGY_OPERATOR_DILATE:
        ts << "DILATE";
        break;
    }
    return ts;
}

TextStream& FEMorphology::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feMorphology";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " operator=\"" << morphologyOperator() << "\" "
       << "radius=\"" << radiusX() << ", " << radiusY() << "\"]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    return ts;
}

} // namespace WebCore
