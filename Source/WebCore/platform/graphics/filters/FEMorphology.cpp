/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "Filter.h"
#include <wtf/text/TextStream.h>

#include <runtime/Uint8ClampedArray.h>
#include <wtf/ParallelJobs.h>
#include <wtf/Vector.h>

namespace WebCore {

FEMorphology::FEMorphology(Filter& filter, MorphologyOperatorType type, float radiusX, float radiusY)
    : FilterEffect(filter)
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
    paintRect.inflateX(filter.applyHorizontalScale(m_radiusX));
    paintRect.inflateY(filter.applyVerticalScale(m_radiusY));
    if (clipsToBounds())
        paintRect.intersect(maxEffectRect());
    else
        paintRect.unite(maxEffectRect());
    setAbsolutePaintRect(enclosingIntRect(paintRect));
}

static inline bool shouldSupersedeExtremum(unsigned char newValue, unsigned char currentValue, MorphologyOperatorType type)
{
    return (type == FEMORPHOLOGY_OPERATOR_ERODE && newValue < currentValue)
        || (type == FEMORPHOLOGY_OPERATOR_DILATE && newValue > currentValue);
}

static inline int pixelArrayIndex(int x, int y, int width, int colorChannel)
{
    return (y * width + x) * 4 + colorChannel;
}

static inline unsigned char columnExtremum(const Uint8ClampedArray* srcPixelArray, int x, int yStart, int yEnd, int width, unsigned colorChannel, MorphologyOperatorType type)
{
    unsigned char extremum = srcPixelArray->item(pixelArrayIndex(x, yStart, width, colorChannel));
    for (int y = yStart + 1; y < yEnd; ++y) {
        unsigned char pixel = srcPixelArray->item(pixelArrayIndex(x, y, width, colorChannel));
        if (shouldSupersedeExtremum(pixel, extremum, type))
            extremum = pixel;
    }
    return extremum;
}

static inline unsigned char kernelExtremum(const Vector<unsigned char>& kernel, MorphologyOperatorType type)
{
    Vector<unsigned char>::const_iterator iter = kernel.begin();
    unsigned char extremum = *iter;
    for (Vector<unsigned char>::const_iterator end = kernel.end(); ++iter != end; ) {
        if (shouldSupersedeExtremum(*iter, extremum, type))
            extremum = *iter;
    }
    return extremum;
}

void FEMorphology::platformApplyGeneric(PaintingData* paintingData, int yStart, int yEnd)
{
    Uint8ClampedArray* srcPixelArray = paintingData->srcPixelArray;
    Uint8ClampedArray* dstPixelArray = paintingData->dstPixelArray;
    const int radiusX = paintingData->radiusX;
    const int radiusY = paintingData->radiusY;
    const int width = paintingData->width;
    const int height = paintingData->height;

    ASSERT(radiusX <= width || radiusY <= height);
    ASSERT(yStart >= 0 && yEnd <= height && yStart < yEnd);

    Vector<unsigned char> extrema;
    for (int y = yStart; y < yEnd; ++y) {
        int yStartExtrema = std::max(0, y - radiusY);
        int yEndExtrema = std::min(height - 1, y + radiusY);

        for (unsigned colorChannel = 0; colorChannel < 4; ++colorChannel) {
            extrema.clear();
            // Compute extremas for each columns
            for (int x = 0; x < radiusX; ++x)
                extrema.append(columnExtremum(srcPixelArray, x, yStartExtrema, yEndExtrema, width, colorChannel, m_type));

            // Kernel is filled, get extrema of next column
            for (int x = 0; x < width; ++x) {
                if (x < width - radiusX) {
                    int xEnd = std::min(x + radiusX, width - 1);
                    extrema.append(columnExtremum(srcPixelArray, xEnd, yStartExtrema, yEndExtrema + 1, width, colorChannel, m_type));
                }

                if (x > radiusX)
                    extrema.remove(0);

                // The extrema original size = radiusX.
                // Number of new addition = width - radiusX.
                // Number of removals = width - radiusX - 1.
                ASSERT(extrema.size() >= static_cast<size_t>(radiusX + 1));
                dstPixelArray->set(pixelArrayIndex(x, y, width, colorChannel), kernelExtremum(extrema, m_type));
            }
        }
    }
}

void FEMorphology::platformApplyWorker(PlatformApplyParameters* param)
{
    param->filter->platformApplyGeneric(param->paintingData, param->startY, param->endY);
}

void FEMorphology::platformApply(PaintingData* paintingData)
{
    int optimalThreadNumber = (paintingData->width * paintingData->height) / s_minimalArea;
    if (optimalThreadNumber > 1) {
        ParallelJobs<PlatformApplyParameters> parallelJobs(&WebCore::FEMorphology::platformApplyWorker, optimalThreadNumber);
        int numOfThreads = parallelJobs.numberOfJobs();
        if (numOfThreads > 1) {
            // Split the job into "jobSize"-sized jobs but there a few jobs that need to be slightly larger since
            // jobSize * jobs < total size. These extras are handled by the remainder "jobsWithExtra".
            const int jobSize = paintingData->height / numOfThreads;
            const int jobsWithExtra = paintingData->height % numOfThreads;
            int currentY = 0;
            for (int job = numOfThreads - 1; job >= 0; --job) {
                PlatformApplyParameters& param = parallelJobs.parameter(job);
                param.filter = this;
                param.startY = currentY;
                currentY += job < jobsWithExtra ? jobSize + 1 : jobSize;
                param.endY = currentY;
                param.paintingData = paintingData;
            }
            parallelJobs.execute();
            return;
        }
        // Fallback to single thread model
    }

    platformApplyGeneric(paintingData, 0, paintingData->height);
}

bool FEMorphology::platformApplyDegenerate(Uint8ClampedArray* dstPixelArray, const IntRect& imageRect, int radiusX, int radiusY)
{
    // Input radius is less than zero or an overflow happens when scaling it.
    if (radiusX < 0 || radiusY < 0) {
        dstPixelArray->zeroFill();
        return true;
    }

    // Input radius is equal to zero or the scaled radius is less than one.
    if (!m_radiusX || !m_radiusY) {
        FilterEffect* in = inputEffect(0);
        in->copyPremultipliedImage(dstPixelArray, imageRect);
        return true;
    }

    return false;
}

void FEMorphology::platformApplySoftware()
{
    FilterEffect* in = inputEffect(0);

    Uint8ClampedArray* dstPixelArray = createPremultipliedImageResult();
    if (!dstPixelArray)
        return;

    setIsAlphaImage(in->isAlphaImage());

    IntRect effectDrawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    if (platformApplyDegenerate(dstPixelArray, effectDrawingRect, m_radiusX, m_radiusY))
        return;

    Filter& filter = this->filter();
    RefPtr<Uint8ClampedArray> srcPixelArray = in->asPremultipliedImage(effectDrawingRect);
    int radiusX = static_cast<int>(floorf(filter.applyHorizontalScale(m_radiusX)));
    int radiusY = static_cast<int>(floorf(filter.applyVerticalScale(m_radiusY)));
    radiusX = std::min(effectDrawingRect.width() - 1, radiusX);
    radiusY = std::min(effectDrawingRect.height() - 1, radiusY);

    if (platformApplyDegenerate(dstPixelArray, effectDrawingRect, radiusX, radiusY))
        return;

    PaintingData paintingData;
    paintingData.srcPixelArray = srcPixelArray.get();
    paintingData.dstPixelArray = dstPixelArray;
    paintingData.width = ceilf(effectDrawingRect.width() * filter.filterScale());
    paintingData.height = ceilf(effectDrawingRect.height() * filter.filterScale());
    paintingData.radiusX = ceilf(radiusX * filter.filterScale());
    paintingData.radiusY = ceilf(radiusY * filter.filterScale());

    platformApply(&paintingData);
}

void FEMorphology::dump()
{
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

TextStream& FEMorphology::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feMorphology";
    FilterEffect::externalRepresentation(ts);
    ts << " operator=\"" << morphologyOperator() << "\" "
       << "radius=\"" << radiusX() << ", " << radiusY() << "\"]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace WebCore
