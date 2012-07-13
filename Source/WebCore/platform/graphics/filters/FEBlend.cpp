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
#include "FEBlendNEON.h"

#include "Filter.h"
#include "FloatPoint.h"
#include "GraphicsContext.h"
#include "RenderTreeAsText.h"
#include "TextStream.h"

#include <wtf/Uint8ClampedArray.h>

typedef unsigned char (*BlendType)(unsigned char colorA, unsigned char colorB, unsigned char alphaA, unsigned char alphaB);

namespace WebCore {

FEBlend::FEBlend(Filter* filter, BlendModeType mode)
    : FilterEffect(filter)
    , m_mode(mode)
{
}

PassRefPtr<FEBlend> FEBlend::create(Filter* filter, BlendModeType mode)
{
    return adoptRef(new FEBlend(filter, mode));
}

BlendModeType FEBlend::blendMode() const
{
    return m_mode;
}

bool FEBlend::setBlendMode(BlendModeType mode)
{
    if (m_mode == mode)
        return false;
    m_mode = mode;
    return true;
}

static inline unsigned char normal(unsigned char colorA, unsigned char colorB, unsigned char alphaA, unsigned char)
{
    return (((255 - alphaA) * colorB + colorA * 255) / 255);
}

static inline unsigned char multiply(unsigned char colorA, unsigned char colorB, unsigned char alphaA, unsigned char alphaB)
{
    return (((255 - alphaA) * colorB + (255 - alphaB + colorB) * colorA) / 255);
}

static inline unsigned char screen(unsigned char colorA, unsigned char colorB, unsigned char, unsigned char)
{
    return (((colorB + colorA) * 255 - colorA * colorB) / 255);
}

static inline unsigned char darken(unsigned char colorA, unsigned char colorB, unsigned char alphaA, unsigned char alphaB)
{
    return ((std::min((255 - alphaA) * colorB + colorA * 255, (255 - alphaB) * colorA + colorB * 255)) / 255);
}

static inline unsigned char lighten(unsigned char colorA, unsigned char colorB, unsigned char alphaA, unsigned char alphaB)
{
    return ((std::max((255 - alphaA) * colorB + colorA * 255, (255 - alphaB) * colorA + colorB * 255)) / 255);
}

void FEBlend::platformApplyGeneric(PassRefPtr<Uint8ClampedArray> pixelArrayA, PassRefPtr<Uint8ClampedArray> pixelArrayB,
                                   Uint8ClampedArray* dstPixelArray, unsigned pixelArrayLength)
{
    RefPtr<Uint8ClampedArray> srcPixelArrayA = pixelArrayA;
    RefPtr<Uint8ClampedArray> srcPixelArrayB = pixelArrayB;

    for (unsigned pixelOffset = 0; pixelOffset < pixelArrayLength; pixelOffset += 4) {
        unsigned char alphaA = srcPixelArrayA->item(pixelOffset + 3);
        unsigned char alphaB = srcPixelArrayB->item(pixelOffset + 3);
        for (unsigned channel = 0; channel < 3; ++channel) {
            unsigned char colorA = srcPixelArrayA->item(pixelOffset + channel);
            unsigned char colorB = srcPixelArrayB->item(pixelOffset + channel);
            unsigned char result;

            switch (m_mode) {
            case FEBLEND_MODE_NORMAL:
                result = normal(colorA, colorB, alphaA, alphaB);
                break;
            case FEBLEND_MODE_MULTIPLY:
                result = multiply(colorA, colorB, alphaA, alphaB);
                break;
            case FEBLEND_MODE_SCREEN:
                result = screen(colorA, colorB, alphaA, alphaB);
                break;
            case FEBLEND_MODE_DARKEN:
                result = darken(colorA, colorB, alphaA, alphaB);
                break;
            case FEBLEND_MODE_LIGHTEN:
                result = lighten(colorA, colorB, alphaA, alphaB);
                break;
            case FEBLEND_MODE_UNKNOWN:
            default:
                result = 0;
                break;
            }

            dstPixelArray->set(pixelOffset + channel, result);
        }
        unsigned char alphaR = 255 - ((255 - alphaA) * (255 - alphaB)) / 255;
        dstPixelArray->set(pixelOffset + 3, alphaR);
    }
}

void FEBlend::platformApplySoftware()
{
    FilterEffect* in = inputEffect(0);
    FilterEffect* in2 = inputEffect(1);

    ASSERT(m_mode > FEBLEND_MODE_UNKNOWN);
    ASSERT(m_mode <= FEBLEND_MODE_LIGHTEN);

    Uint8ClampedArray* dstPixelArray = createPremultipliedImageResult();
    if (!dstPixelArray)
        return;

    IntRect effectADrawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    RefPtr<Uint8ClampedArray> srcPixelArrayA = in->asPremultipliedImage(effectADrawingRect);

    IntRect effectBDrawingRect = requestedRegionOfInputImageData(in2->absolutePaintRect());
    RefPtr<Uint8ClampedArray> srcPixelArrayB = in2->asPremultipliedImage(effectBDrawingRect);

    unsigned pixelArrayLength = srcPixelArrayA->length();
    ASSERT(pixelArrayLength == srcPixelArrayB->length());

#if HAVE(ARM_NEON_INTRINSICS)
    if (pixelArrayLength >= 8)
        platformApplyNEON(srcPixelArrayA->data(), srcPixelArrayB->data(), dstPixelArray->data(), pixelArrayLength);
    else { // If there is just one pixel we expand it to two.
        ASSERT(pixelArrayLength > 0);
        uint32_t sourceA[2] = {0, 0};
        uint32_t sourceBAndDest[2] = {0, 0};

        sourceA[0] = reinterpret_cast<uint32_t*>(srcPixelArrayA->data())[0];
        sourceBAndDest[0] = reinterpret_cast<uint32_t*>(srcPixelArrayB->data())[0];
        platformApplyNEON(reinterpret_cast<uint8_t*>(sourceA), reinterpret_cast<uint8_t*>(sourceBAndDest), reinterpret_cast<uint8_t*>(sourceBAndDest), 8);
        reinterpret_cast<uint32_t*>(dstPixelArray->data())[0] = sourceBAndDest[0];
    }
#else
    platformApplyGeneric(srcPixelArrayA, srcPixelArrayB, dstPixelArray, pixelArrayLength);
#endif
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
