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

#if ENABLE(FILTERS)
#include "FEDisplacementMap.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageData.h"

namespace WebCore {

FEDisplacementMap::FEDisplacementMap(Filter* filter, ChannelSelectorType xChannelSelector, ChannelSelectorType yChannelSelector, float scale)
    : FilterEffect(filter)
    , m_xChannelSelector(xChannelSelector)
    , m_yChannelSelector(yChannelSelector)
    , m_scale(scale)
{
}

PassRefPtr<FEDisplacementMap> FEDisplacementMap::create(Filter* filter, ChannelSelectorType xChannelSelector,
    ChannelSelectorType yChannelSelector, float scale)
{
    return adoptRef(new FEDisplacementMap(filter, xChannelSelector, yChannelSelector, scale));
}

ChannelSelectorType FEDisplacementMap::xChannelSelector() const
{
    return m_xChannelSelector;
}

void FEDisplacementMap::setXChannelSelector(const ChannelSelectorType xChannelSelector)
{
    m_xChannelSelector = xChannelSelector;
}

ChannelSelectorType FEDisplacementMap::yChannelSelector() const
{
    return m_yChannelSelector;
}

void FEDisplacementMap::setYChannelSelector(const ChannelSelectorType yChannelSelector)
{
    m_yChannelSelector = yChannelSelector;
}

float FEDisplacementMap::scale() const
{
    return m_scale;
}

void FEDisplacementMap::setScale(float scale)
{
    m_scale = scale;
}

void FEDisplacementMap::apply()
{
    FilterEffect* in = inputEffect(0);
    FilterEffect* in2 = inputEffect(1);
    in->apply();
    in2->apply();
    if (!in->resultImage() || !in2->resultImage())
        return;

    if (m_xChannelSelector == CHANNEL_UNKNOWN || m_yChannelSelector == CHANNEL_UNKNOWN)
        return;

    if (!effectContext())
        return;

    IntRect effectADrawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    RefPtr<ImageData> srcImageDataA = in->resultImage()->getPremultipliedImageData(effectADrawingRect);
    ByteArray* srcPixelArrayA = srcImageDataA->data()->data() ;

    IntRect effectBDrawingRect = requestedRegionOfInputImageData(in2->absolutePaintRect());
    RefPtr<ImageData> srcImageDataB = in2->resultImage()->getUnmultipliedImageData(effectBDrawingRect);
    ByteArray* srcPixelArrayB = srcImageDataB->data()->data();

    IntRect imageRect(IntPoint(), resultImage()->size());
    RefPtr<ImageData> imageData = ImageData::create(imageRect.width(), imageRect.height());
    ByteArray* dstPixelArray = imageData->data()->data();

    ASSERT(srcPixelArrayA->length() == srcPixelArrayB->length());

    Filter* filter = this->filter();
    float scaleX = filter->applyHorizontalScale(m_scale / 255);
    float scaleY = filter->applyVerticalScale(m_scale / 255);
    float scaleAdjustmentX = filter->applyHorizontalScale(0.5f - 0.5f * m_scale);
    float scaleAdjustmentY = filter->applyVerticalScale(0.5f - 0.5f * m_scale);
    int stride = imageRect.width() * 4;
    for (int y = 0; y < imageRect.height(); ++y) {
        int line = y * stride;
        for (int x = 0; x < imageRect.width(); ++x) {
            int dstIndex = line + x * 4;
            int srcX = x + static_cast<int>(scaleX * srcPixelArrayB->get(dstIndex + m_xChannelSelector - 1) + scaleAdjustmentX);
            int srcY = y + static_cast<int>(scaleY * srcPixelArrayB->get(dstIndex + m_yChannelSelector - 1) + scaleAdjustmentY);
            for (unsigned channel = 0; channel < 4; ++channel) {
                if (srcX < 0 || srcX >= imageRect.width() || srcY < 0 || srcY >= imageRect.height())
                    dstPixelArray->set(dstIndex + channel, static_cast<unsigned char>(0));
                else {
                    unsigned char pixelValue = srcPixelArrayA->get(srcY * stride + srcX * 4 + channel);
                    dstPixelArray->set(dstIndex + channel, pixelValue);
                }
            }

        }
    }
    resultImage()->putPremultipliedImageData(imageData.get(), imageRect, IntPoint());
}

void FEDisplacementMap::dump()
{
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

TextStream& FEDisplacementMap::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feDisplacementMap";
    FilterEffect::externalRepresentation(ts);
    ts << " scale=\"" << m_scale << "\" "
       << "xChannelSelector=\"" << m_xChannelSelector << "\" "
       << "yChannelSelector=\"" << m_yChannelSelector << "\"]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    inputEffect(1)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
