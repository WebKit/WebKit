/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#if ENABLE(FILTERS)
#include "FEMorphology.h"

#include "CanvasPixelArray.h"
#include "Filter.h"
#include "ImageData.h"

#include <wtf/Vector.h>

using std::min;
using std::max;

namespace WebCore {

FEMorphology::FEMorphology(MorphologyOperatorType type, float radiusX, float radiusY)
    : FilterEffect()
    , m_type(type)
    , m_radiusX(radiusX)
    , m_radiusY(radiusY)
{
}

PassRefPtr<FEMorphology> FEMorphology::create(MorphologyOperatorType type, float radiusX, float radiusY)
{
    return adoptRef(new FEMorphology(type, radiusX, radiusY));
}

MorphologyOperatorType FEMorphology::morphologyOperator() const
{
    return m_type;
}

void FEMorphology::setMorphologyOperator(MorphologyOperatorType type)
{
    m_type = type;
}

float FEMorphology::radiusX() const
{
    return m_radiusX;
}

void FEMorphology::setRadiusX(float radiusX)
{
    m_radiusX = radiusX;
}

float FEMorphology::radiusY() const
{
    return m_radiusY;
}

void FEMorphology::setRadiusY(float radiusY)
{
    m_radiusY = radiusY;
}

void FEMorphology::apply(Filter* filter)
{
    FilterEffect* in = inputEffect(0);
    in->apply(filter);
    if (!in->resultImage())
        return;

    if (!effectContext())
        return;

    setIsAlphaImage(in->isAlphaImage());

    int radiusX = static_cast<int>(m_radiusX * filter->filterResolution().width());
    int radiusY = static_cast<int>(m_radiusY * filter->filterResolution().height());
    if (radiusX <= 0 || radiusY <= 0)
        return;

    IntRect imageRect(IntPoint(), resultImage()->size());
    IntRect effectDrawingRect = requestedRegionOfInputImageData(in->repaintRectInLocalCoordinates());
    RefPtr<CanvasPixelArray> srcPixelArray(in->resultImage()->getPremultipliedImageData(effectDrawingRect)->data());
    RefPtr<ImageData> imageData = ImageData::create(imageRect.width(), imageRect.height());

    int effectWidth = effectDrawingRect.width() * 4;
    
    // Limit the radius size to effect dimensions
    radiusX = min(effectDrawingRect.width() - 1, radiusX);
    radiusY = min(effectDrawingRect.height() - 1, radiusY);
    
    Vector<unsigned char> extrema;
    for (int y = 0; y < effectDrawingRect.height(); ++y) {
        int startY = max(0, y - radiusY);
        int endY = min(effectDrawingRect.height() - 1, y + radiusY);
        for (unsigned channel = 0; channel < 4; ++channel) {
            // Fill the kernel
            extrema.clear();
            for (int j = 0; j <= radiusX; ++j) {
                unsigned char columnExtrema = srcPixelArray->get(startY * effectWidth + 4 * j + channel);
                for (int i = startY; i <= endY; ++i) {
                    unsigned char pixel = srcPixelArray->get(i * effectWidth + 4 * j + channel);
                    if ((m_type == FEMORPHOLOGY_OPERATOR_ERODE && pixel <= columnExtrema) ||
                        (m_type == FEMORPHOLOGY_OPERATOR_DILATE && pixel >= columnExtrema))
                        columnExtrema = pixel;
                }
                extrema.append(columnExtrema);
            }
            
            // Kernel is filled, get extrema of next column 
            for (int x = 0; x < effectDrawingRect.width(); ++x) {
                unsigned endX = min(x + radiusX, effectDrawingRect.width() - 1);
                unsigned char columnExtrema = srcPixelArray->get(startY * effectWidth + endX * 4 + channel);
                for (int i = startY; i <= endY; ++i) {
                    unsigned char pixel = srcPixelArray->get(i * effectWidth + endX * 4 + channel);
                    if ((m_type == FEMORPHOLOGY_OPERATOR_ERODE && pixel <= columnExtrema) ||
                        (m_type == FEMORPHOLOGY_OPERATOR_DILATE && pixel >= columnExtrema))
                        columnExtrema = pixel;
                }
                if (x - radiusX >= 0)
                    extrema.remove(0);
                if (x + radiusX <= effectDrawingRect.width())
                    extrema.append(columnExtrema);
                unsigned char entireExtrema = extrema[0];
                for (unsigned kernelIndex = 0; kernelIndex < extrema.size(); ++kernelIndex) {
                    if ((m_type == FEMORPHOLOGY_OPERATOR_ERODE && extrema[kernelIndex] <= entireExtrema) ||
                        (m_type == FEMORPHOLOGY_OPERATOR_DILATE && extrema[kernelIndex] >= entireExtrema))
                        entireExtrema = extrema[kernelIndex];
                }
                imageData->data()->set(y * effectWidth + 4 * x + channel, entireExtrema);
            }
        }
    }
    resultImage()->putPremultipliedImageData(imageData.get(), imageRect, IntPoint());
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

#endif // ENABLE(FILTERS)
