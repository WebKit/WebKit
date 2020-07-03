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
#include "FEDisplacementMap.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

FEDisplacementMap::FEDisplacementMap(Filter& filter, ChannelSelectorType xChannelSelector, ChannelSelectorType yChannelSelector, float scale)
    : FilterEffect(filter)
    , m_xChannelSelector(xChannelSelector)
    , m_yChannelSelector(yChannelSelector)
    , m_scale(scale)
{
}

Ref<FEDisplacementMap> FEDisplacementMap::create(Filter& filter, ChannelSelectorType xChannelSelector,
    ChannelSelectorType yChannelSelector, float scale)
{
    return adoptRef(*new FEDisplacementMap(filter, xChannelSelector, yChannelSelector, scale));
}

bool FEDisplacementMap::setXChannelSelector(const ChannelSelectorType xChannelSelector)
{
    if (m_xChannelSelector == xChannelSelector)
        return false;
    m_xChannelSelector = xChannelSelector;
    return true;
}

bool FEDisplacementMap::setYChannelSelector(const ChannelSelectorType yChannelSelector)
{
    if (m_yChannelSelector == yChannelSelector)
        return false;
    m_yChannelSelector = yChannelSelector;
    return true;
}

bool FEDisplacementMap::setScale(float scale)
{
    if (m_scale == scale)
        return false;
    m_scale = scale;
    return true;
}

void FEDisplacementMap::setResultColorSpace(ColorSpace)
{
    // Spec: The 'color-interpolation-filters' property only applies to the 'in2' source image
    // and does not apply to the 'in' source image. The 'in' source image must remain in its
    // current color space.
    // The result is in that smae color space because it is a displacement of the 'in' image.
    FilterEffect::setResultColorSpace(inputEffect(0)->resultColorSpace());
}

void FEDisplacementMap::transformResultColorSpace(FilterEffect* in, const int index)
{
    // Do not transform the first primitive input, as per the spec.
    if (index)
        in->transformResultColorSpace(operatingColorSpace());
}

static inline unsigned byteOffsetOfPixel(unsigned x, unsigned y, unsigned rowBytes)
{
    const unsigned bytesPerPixel = 4;
    return x * bytesPerPixel + y * rowBytes;
}

void FEDisplacementMap::platformApplySoftware()
{
    FilterEffect* in = inputEffect(0);
    FilterEffect* in2 = inputEffect(1);

    ASSERT(m_xChannelSelector != CHANNEL_UNKNOWN);
    ASSERT(m_yChannelSelector != CHANNEL_UNKNOWN);

    auto* resultImage = createPremultipliedImageResult();
    auto* dstPixelArray = resultImage ? resultImage->data() : nullptr;
    if (!dstPixelArray)
        return;

    IntRect effectADrawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    auto inputImage = in->premultipliedResult(effectADrawingRect);

    IntRect effectBDrawingRect = requestedRegionOfInputImageData(in2->absolutePaintRect());
    // The calculations using the pixel values from ‘in2’ are performed using non-premultiplied color values.
    auto displacementImage = in2->unmultipliedResult(effectBDrawingRect);
    
    if (!inputImage || !displacementImage)
        return;

    ASSERT(inputImage->length() == displacementImage->length());

    Filter& filter = this->filter();
    IntSize paintSize = absolutePaintRect().size();
    paintSize.scale(filter.filterScale());

    FloatSize scale = filter.scaledByFilterResolution({ m_scale, m_scale });
    float scaleForColorX = scale.width() / 255.0;
    float scaleForColorY = scale.height() / 255.0;
    float scaledOffsetX = 0.5 - scale.width() * 0.5;
    float scaledOffsetY = 0.5 - scale.height() * 0.5;
    
    int displacementChannelX = xChannelIndex();
    int displacementChannelY = yChannelIndex();

    int rowBytes = paintSize.width() * 4;

    for (int y = 0; y < paintSize.height(); ++y) {
        int lineStartOffset = y * rowBytes;

        for (int x = 0; x < paintSize.width(); ++x) {
            int dstIndex = lineStartOffset + x * 4;
            
            int srcX = x + static_cast<int>(scaleForColorX * displacementImage->item(dstIndex + displacementChannelX) + scaledOffsetX);
            int srcY = y + static_cast<int>(scaleForColorY * displacementImage->item(dstIndex + displacementChannelY) + scaledOffsetY);

            unsigned* dstPixelPtr = reinterpret_cast<unsigned*>(dstPixelArray->data() + dstIndex);
            if (srcX < 0 || srcX >= paintSize.width() || srcY < 0 || srcY >= paintSize.height()) {
                *dstPixelPtr = 0;
                continue;
            }

            *dstPixelPtr = *reinterpret_cast<unsigned*>(inputImage->data() + byteOffsetOfPixel(srcX, srcY, rowBytes));
        }
    }
}

static TextStream& operator<<(TextStream& ts, const ChannelSelectorType& type)
{
    switch (type) {
    case CHANNEL_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case CHANNEL_R:
        ts << "RED";
        break;
    case CHANNEL_G:
        ts << "GREEN";
        break;
    case CHANNEL_B:
        ts << "BLUE";
        break;
    case CHANNEL_A:
        ts << "ALPHA";
        break;
    }
    return ts;
}

TextStream& FEDisplacementMap::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feDisplacementMap";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " scale=\"" << m_scale << "\" "
       << "xChannelSelector=\"" << m_xChannelSelector << "\" "
       << "yChannelSelector=\"" << m_yChannelSelector << "\"]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    inputEffect(1)->externalRepresentation(ts, representation);
    return ts;
}

} // namespace WebCore
