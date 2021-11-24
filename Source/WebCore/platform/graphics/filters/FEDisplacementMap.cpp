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
#include "FilterEffectApplier.h"
#include "GraphicsContext.h"
#include "PixelBuffer.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEDisplacementMap> FEDisplacementMap::create(ChannelSelectorType xChannelSelector, ChannelSelectorType yChannelSelector, float scale)
{
    return adoptRef(*new FEDisplacementMap(xChannelSelector, yChannelSelector, scale));
}

FEDisplacementMap::FEDisplacementMap(ChannelSelectorType xChannelSelector, ChannelSelectorType yChannelSelector, float scale)
    : FilterEffect(FilterEffect::Type::FEDisplacementMap)
    , m_xChannelSelector(xChannelSelector)
    , m_yChannelSelector(yChannelSelector)
    , m_scale(scale)
{
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

const DestinationColorSpace& FEDisplacementMap::resultColorSpace() const
{
    // Spec: The 'color-interpolation-filters' property only applies to the 'in2' source image
    // and does not apply to the 'in' source image. The 'in' source image must remain in its
    // current color space.
    // The result is in that same color space because it is a displacement of the 'in' image.
    return inputEffect(0)->resultColorSpace();
}

void FEDisplacementMap::transformResultColorSpace(FilterEffect* in, const int index)
{
    // Do not transform the first primitive input, as per the spec.
    if (index)
        in->transformResultColorSpace(operatingColorSpace());
}

// FIXME: Move the class FECompositeSoftwareApplier to separate source and header files.
class FEDisplacementMapSoftwareApplier : public FilterEffectConcreteApplier<FEDisplacementMap> {
    using Base = FilterEffectConcreteApplier<FEDisplacementMap>;

public:
    FEDisplacementMapSoftwareApplier(FEDisplacementMap&);

    bool apply(const Filter&, const FilterEffectVector& inputEffects) override;

private:
    static inline unsigned byteOffsetOfPixel(unsigned x, unsigned y, unsigned rowBytes)
    {
        const unsigned bytesPerPixel = 4;
        return x * bytesPerPixel + y * rowBytes;
    }

    int xChannelIndex() const { return m_effect.xChannelSelector() - 1; }
    int yChannelIndex() const { return m_effect.yChannelSelector() - 1; }
};

FEDisplacementMapSoftwareApplier::FEDisplacementMapSoftwareApplier(FEDisplacementMap& effect)
    : Base(effect)
{
    ASSERT(m_effect.xChannelSelector() != CHANNEL_UNKNOWN);
    ASSERT(m_effect.yChannelSelector() != CHANNEL_UNKNOWN);
}

bool FEDisplacementMapSoftwareApplier::apply(const Filter& filter, const FilterEffectVector& inputEffects)
{
    FilterEffect* in = inputEffects[0].get();
    FilterEffect* in2 = inputEffects[1].get();

    auto destinationPixelBuffer = m_effect.pixelBufferResult(AlphaPremultiplication::Premultiplied);
    if (!destinationPixelBuffer)
        return false;

    auto& destinationPixelArray = destinationPixelBuffer->data();

    auto effectADrawingRect = m_effect.requestedRegionOfInputPixelBuffer(in->absolutePaintRect());
    auto inputPixelBuffer = in->getPixelBufferResult(AlphaPremultiplication::Premultiplied, effectADrawingRect);

    auto effectBDrawingRect = m_effect.requestedRegionOfInputPixelBuffer(in2->absolutePaintRect());
    // The calculations using the pixel values from ‘in2’ are performed using non-premultiplied color values.
    auto displacementPixelBuffer = in2->getPixelBufferResult(AlphaPremultiplication::Unpremultiplied, effectBDrawingRect);
    
    if (!inputPixelBuffer || !displacementPixelBuffer)
        return false;

    auto& inputImage = inputPixelBuffer->data();
    auto& displacementImage = displacementPixelBuffer->data();
    ASSERT(inputImage.length() == displacementImage.length());

    IntSize paintSize = m_effect.absolutePaintRect().size();

    FloatSize scale = filter.scaledByFilterScale({ m_effect.scale(), m_effect.scale() });
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
            int destinationIndex = lineStartOffset + x * 4;
            
            int srcX = x + static_cast<int>(scaleForColorX * displacementImage.item(destinationIndex + displacementChannelX) + scaledOffsetX);
            int srcY = y + static_cast<int>(scaleForColorY * displacementImage.item(destinationIndex + displacementChannelY) + scaledOffsetY);

            unsigned* destinationPixelPtr = reinterpret_cast<unsigned*>(destinationPixelArray.data() + destinationIndex);
            if (srcX < 0 || srcX >= paintSize.width() || srcY < 0 || srcY >= paintSize.height()) {
                *destinationPixelPtr = 0;
                continue;
            }

            *destinationPixelPtr = *reinterpret_cast<unsigned*>(inputImage.data() + byteOffsetOfPixel(srcX, srcY, rowBytes));
        }
    }

    return true;
}

bool FEDisplacementMap::platformApplySoftware(const Filter& filter)
{
    return FEDisplacementMapSoftwareApplier(*this).apply(filter, inputEffects());
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
