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
#include "FEBlend.h"

#include "CanvasPixelArray.h"
#include "Filter.h"
#include "FloatPoint.h"
#include "GraphicsContext.h"
#include "ImageData.h"

typedef unsigned char (*BlendType)(unsigned char colorA, unsigned char colorB, unsigned char alphaA, unsigned char alphaB);

namespace WebCore {

FEBlend::FEBlend(BlendModeType mode)
    : FilterEffect()
    , m_mode(mode)
{
}

PassRefPtr<FEBlend> FEBlend::create(BlendModeType mode)
{
    return adoptRef(new FEBlend(mode));
}

BlendModeType FEBlend::blendMode() const
{
    return m_mode;
}

void FEBlend::setBlendMode(BlendModeType mode)
{
    m_mode = mode;
}

static unsigned char unknown(unsigned char, unsigned char, unsigned char, unsigned char)
{
    return 0;
}

static unsigned char normal(unsigned char colorA, unsigned char colorB, unsigned char alphaA, unsigned char)
{
    return (((255 - alphaA) * colorB + colorA * 255) / 255);
}

static unsigned char multiply(unsigned char colorA, unsigned char colorB, unsigned char alphaA, unsigned char alphaB)
{
    return (((255 - alphaA) * colorB + (255 - alphaB + colorB) * colorA) / 255);
}

static unsigned char screen(unsigned char colorA, unsigned char colorB, unsigned char, unsigned char)
{
    return (((colorB + colorA) * 255 - colorA * colorB) / 255);
}

static unsigned char darken(unsigned char colorA, unsigned char colorB, unsigned char alphaA, unsigned char alphaB)
{
    return ((std::min((255 - alphaA) * colorB + colorA * 255, (255 - alphaB) * colorA + colorB * 255)) / 255);
}

static unsigned char lighten(unsigned char colorA, unsigned char colorB, unsigned char alphaA, unsigned char alphaB)
{
    return ((std::max((255 - alphaA) * colorB + colorA * 255, (255 - alphaB) * colorA + colorB * 255)) / 255);
}

void FEBlend::apply(Filter* filter)
{
    FilterEffect* in = inputEffect(0);
    FilterEffect* in2 = inputEffect(1);
    in->apply(filter);
    in2->apply(filter);
    if (!in->resultImage() || !in2->resultImage())
        return;

    if (m_mode == FEBLEND_MODE_UNKNOWN)
        return;

    if (!effectContext())
        return;

    IntRect effectADrawingRect = requestedRegionOfInputImageData(in->repaintRectInLocalCoordinates());
    RefPtr<CanvasPixelArray> srcPixelArrayA(in->resultImage()->getPremultipliedImageData(effectADrawingRect)->data());

    IntRect effectBDrawingRect = requestedRegionOfInputImageData(in2->repaintRectInLocalCoordinates());
    RefPtr<CanvasPixelArray> srcPixelArrayB(in2->resultImage()->getPremultipliedImageData(effectBDrawingRect)->data());

    IntRect imageRect(IntPoint(), resultImage()->size());
    RefPtr<ImageData> imageData = ImageData::create(imageRect.width(), imageRect.height());

    // Keep synchronized with BlendModeType
    static const BlendType callEffect[] = {unknown, normal, multiply, screen, darken, lighten};

    ASSERT(srcPixelArrayA->length() == srcPixelArrayB->length());
    for (unsigned pixelOffset = 0; pixelOffset < srcPixelArrayA->length(); pixelOffset += 4) {
        unsigned char alphaA = srcPixelArrayA->get(pixelOffset + 3);
        unsigned char alphaB = srcPixelArrayB->get(pixelOffset + 3);
        for (unsigned channel = 0; channel < 3; ++channel) {
            unsigned char colorA = srcPixelArrayA->get(pixelOffset + channel);
            unsigned char colorB = srcPixelArrayB->get(pixelOffset + channel);

            unsigned char result = (*callEffect[m_mode])(colorA, colorB, alphaA, alphaB);
            imageData->data()->set(pixelOffset + channel, result);
        }
        unsigned char alphaR = 255 - ((255 - alphaA) * (255 - alphaB)) / 255;
        imageData->data()->set(pixelOffset + 3, alphaR);
    }

    resultImage()->putPremultipliedImageData(imageData.get(), imageRect, IntPoint());
}

void FEBlend::dump()
{
}

static TextStream& operator<<(TextStream& ts, const BlendModeType& type)
{
    switch (type) {
    case FEBLEND_MODE_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case FEBLEND_MODE_NORMAL:
        ts << "NORMAL";
        break;
    case FEBLEND_MODE_MULTIPLY:
        ts << "MULTIPLY";
        break;
    case FEBLEND_MODE_SCREEN:
        ts << "SCREEN";
        break;
    case FEBLEND_MODE_DARKEN:
        ts << "DARKEN";
        break;
    case FEBLEND_MODE_LIGHTEN:
        ts << "LIGHTEN";
        break;
    }
    return ts;
}

TextStream& FEBlend::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feBlend";
    FilterEffect::externalRepresentation(ts);
    ts << " mode=\"" << m_mode << "\"]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    inputEffect(1)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
