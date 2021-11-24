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
#include "FilterEffectApplier.h"
#include "PixelBuffer.h"
#include <wtf/ParallelJobs.h>
#include <wtf/Vector.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEMorphology> FEMorphology::create(MorphologyOperatorType type, float radiusX, float radiusY)
{
    return adoptRef(*new FEMorphology(type, radiusX, radiusY));
}

FEMorphology::FEMorphology(MorphologyOperatorType type, float radiusX, float radiusY)
    : FilterEffect(FilterEffect::Type::FEMorphology)
    , m_type(type)
    , m_radiusX(radiusX)
    , m_radiusY(radiusY)
{
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

void FEMorphology::determineAbsolutePaintRect(const Filter& filter)
{
    FloatRect paintRect = inputEffect(0)->absolutePaintRect();
    paintRect.inflate(filter.scaledByFilterScale({ m_radiusX, m_radiusY }));
    if (clipsToBounds())
        paintRect.intersect(maxEffectRect());
    else
        paintRect.unite(maxEffectRect());
    setAbsolutePaintRect(enclosingIntRect(paintRect));
}

// FIXME: Move the class FEMorphologySoftwareApplier to separate source and header files.
class FEMorphologySoftwareApplier : public FilterEffectConcreteApplier<FEMorphology> {
    using Base = FilterEffectConcreteApplier<FEMorphology>;

public:
    using Base::Base;

    bool apply(const Filter&, const FilterEffectVector& inputEffects) override;

private:
    using ColumnExtrema = Vector<ColorComponents<uint8_t, 4>, 16>;

    struct PaintingData {
        MorphologyOperatorType type;
        int radiusX;
        int radiusY;
        const Uint8ClampedArray* srcPixelArray;
        Uint8ClampedArray* dstPixelArray;
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

    static inline ColorComponents<uint8_t, 4> makeColorComponentsfromPixelValue(PackedColor::RGBA pixel) { return asColorComponents(asSRGBA(pixel)); }
    static inline ColorComponents<uint8_t, 4> minOrMax(const ColorComponents<uint8_t, 4>& a, const ColorComponents<uint8_t, 4>& b, MorphologyOperatorType);
    static inline ColorComponents<uint8_t, 4> columnExtremum(const Uint8ClampedArray& srcPixelArray, int x, int yStart, int yEnd, int width, MorphologyOperatorType);
    static inline ColorComponents<uint8_t, 4> kernelExtremum(const ColumnExtrema& kernel, MorphologyOperatorType);

    static void applyPlatformGeneric(const PaintingData&, int startY, int endY);
    static void applyPlatformWorker(ApplyParameters*);
    static void applyPlatform(const PaintingData&);
};

inline ColorComponents<uint8_t, 4> FEMorphologySoftwareApplier::minOrMax(const ColorComponents<uint8_t, 4>& a, const ColorComponents<uint8_t, 4>& b, MorphologyOperatorType type)
{
    if (type == FEMORPHOLOGY_OPERATOR_ERODE)
        return perComponentMin(a, b);

    return perComponentMax(a, b);
}

inline ColorComponents<uint8_t, 4> FEMorphologySoftwareApplier::columnExtremum(const Uint8ClampedArray& srcPixelArray, int x, int yStart, int yEnd, int width, MorphologyOperatorType type)
{
    auto extremum = makeColorComponentsfromPixelValue(PackedColor::RGBA { *reinterpret_cast<const unsigned*>(srcPixelArray.data() + pixelArrayIndex(x, yStart, width)) });

    for (int y = yStart + 1; y < yEnd; ++y) {
        auto pixel = makeColorComponentsfromPixelValue(PackedColor::RGBA { *reinterpret_cast<const unsigned*>(srcPixelArray.data() + pixelArrayIndex(x, y, width)) });
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

void FEMorphologySoftwareApplier::applyPlatformGeneric(const PaintingData& paintingData, int startY, int endY)
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
            extrema.append(columnExtremum(srcPixelArray, x, yRadiusStart, yRadiusEnd, width, paintingData.type));

        // Kernel is filled, get extrema of next column
        for (int x = 0; x < width; ++x) {
            if (x < width - radiusX)
                extrema.append(columnExtremum(srcPixelArray, x + radiusX, yRadiusStart, yRadiusEnd, width, paintingData.type));

            if (x > radiusX)
                extrema.remove(0);

            unsigned* destPixel = reinterpret_cast<unsigned*>(dstPixelArray.data() + pixelArrayIndex(x, y, width));
            *destPixel = makePixelValueFromColorComponents(kernelExtremum(extrema, paintingData.type)).value;
        }
    }
}

void FEMorphologySoftwareApplier::applyPlatformWorker(ApplyParameters* params)
{
    applyPlatformGeneric(*params->paintingData, params->startY, params->endY);
}

void FEMorphologySoftwareApplier::applyPlatform(const PaintingData& paintingData)
{
    // Empirically, runtime is approximately linear over reasonable kernel sizes with a slope of about 0.65.
    float kernelFactor = sqrt(paintingData.radiusX * paintingData.radiusY) * 0.65;

    static const int minimalArea = (160 * 160); // Empirical data limit for parallel jobs
    
    unsigned maxNumThreads = paintingData.height / 8;
    unsigned optimalThreadNumber = std::min<unsigned>((paintingData.width * paintingData.height * kernelFactor) / minimalArea, maxNumThreads);
    if (optimalThreadNumber > 1) {
        ParallelJobs<ApplyParameters> parallelJobs(&applyPlatformWorker, optimalThreadNumber);
        auto numOfThreads = parallelJobs.numberOfJobs();
        if (numOfThreads > 1) {
            // Split the job into "jobSize"-sized jobs but there a few jobs that need to be slightly larger since
            // jobSize * jobs < total size. These extras are handled by the remainder "jobsWithExtra".
            int jobSize = paintingData.height / numOfThreads;
            int jobsWithExtra = paintingData.height % numOfThreads;
            int currentY = 0;
            for (int job = numOfThreads - 1; job >= 0; --job) {
                ApplyParameters& param = parallelJobs.parameter(job);
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

    applyPlatformGeneric(paintingData, 0, paintingData.height);
}

bool FEMorphologySoftwareApplier::apply(const Filter& filter, const FilterEffectVector& inputEffects)
{
    FilterEffect* in = inputEffects[0].get();

    auto destinationPixelBuffer = m_effect.pixelBufferResult(AlphaPremultiplication::Premultiplied);
    if (!destinationPixelBuffer)
        return false;

    m_effect.setIsAlphaImage(in->isAlphaImage());

    auto isDegenerate = [](int radiusX, int radiusY) -> bool {
        return radiusX < 0 || radiusY < 0 || (!radiusX && !radiusY);
    };

    IntRect effectDrawingRect = m_effect.requestedRegionOfInputPixelBuffer(in->absolutePaintRect());
    IntSize radius = flooredIntSize(FloatSize(m_effect.radiusX(), m_effect.radiusY()));

    if (isDegenerate(radius.width(), radius.height())) {
        in->copyPixelBufferResult(*destinationPixelBuffer, effectDrawingRect);
        return true;
    }

    radius = flooredIntSize(filter.scaledByFilterScale({ m_effect.radiusX(), m_effect.radiusY() }));
    int radiusX = std::min(effectDrawingRect.width() - 1, radius.width());
    int radiusY = std::min(effectDrawingRect.height() - 1, radius.height());

    if (isDegenerate(radiusX, radiusY)) {
        in->copyPixelBufferResult(*destinationPixelBuffer, effectDrawingRect);
        return true;
    }

    auto sourcePixelBuffer = in->getPixelBufferResult(AlphaPremultiplication::Premultiplied, effectDrawingRect, m_effect.operatingColorSpace());
    if (!sourcePixelBuffer)
        return false;

    auto& sourcePixelArray = sourcePixelBuffer->data();
    auto& destinationPixelArray = destinationPixelBuffer->data();

    PaintingData paintingData;
    paintingData.type = m_effect.morphologyOperator();
    paintingData.srcPixelArray = &sourcePixelArray;
    paintingData.dstPixelArray = &destinationPixelArray;
    paintingData.width = ceilf(effectDrawingRect.width());
    paintingData.height = ceilf(effectDrawingRect.height());
    paintingData.radiusX = ceilf(radiusX);
    paintingData.radiusY = ceilf(radiusY);

    applyPlatform(paintingData);
    return true;
}

bool FEMorphology::platformApplySoftware(const Filter& filter)
{
    return FEMorphologySoftwareApplier(*this).apply(filter, inputEffects());
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
