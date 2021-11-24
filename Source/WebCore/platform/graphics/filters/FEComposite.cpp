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
#include "FEComposite.h"

#include "FECompositeArithmeticNEON.h"
#include "FilterEffectApplier.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEComposite> FEComposite::create(const CompositeOperationType& type, float k1, float k2, float k3, float k4)
{
    return adoptRef(*new FEComposite(type, k1, k2, k3, k4));
}

FEComposite::FEComposite(const CompositeOperationType& type, float k1, float k2, float k3, float k4)
    : FilterEffect(FilterEffect::Type::FEComposite)
    , m_type(type)
    , m_k1(k1)
    , m_k2(k2)
    , m_k3(k3)
    , m_k4(k4)
{
}

bool FEComposite::setOperation(CompositeOperationType type)
{
    if (m_type == type)
        return false;
    m_type = type;
    return true;
}

bool FEComposite::setK1(float k1)
{
    if (m_k1 == k1)
        return false;
    m_k1 = k1;
    return true;
}

bool FEComposite::setK2(float k2)
{
    if (m_k2 == k2)
        return false;
    m_k2 = k2;
    return true;
}

bool FEComposite::setK3(float k3)
{
    if (m_k3 == k3)
        return false;
    m_k3 = k3;
    return true;
}

bool FEComposite::setK4(float k4)
{
    if (m_k4 == k4)
        return false;
    m_k4 = k4;
    return true;
}

void FEComposite::determineAbsolutePaintRect(const Filter& filter)
{
    switch (m_type) {
    case FECOMPOSITE_OPERATOR_IN:
    case FECOMPOSITE_OPERATOR_ATOP:
        // For In and Atop the first effect just influences the result of
        // the second effect. So just use the absolute paint rect of the second effect here.
        setAbsolutePaintRect(inputEffect(1)->absolutePaintRect());
        clipAbsolutePaintRect();
        return;
    case FECOMPOSITE_OPERATOR_ARITHMETIC:
        // Arithmetic may influnce the compele filter primitive region. So we can't
        // optimize the paint region here.
        setAbsolutePaintRect(enclosingIntRect(maxEffectRect()));
        return;
    default:
        // Take the union of both input effects.
        FilterEffect::determineAbsolutePaintRect(filter);
        return;
    }
}

// FIXME: Move the class FECompositeSoftwareApplier to separate source and header files.
class FECompositeSoftwareApplier : public FilterEffectConcreteApplier<FEComposite> {
    using Base = FilterEffectConcreteApplier<FEComposite>;

public:
    using Base::Base;

    bool apply(const Filter&, const FilterEffectVector& inputEffects) override;

private:
    static uint8_t clampByte(int);

    template <int b1, int b4>
    static inline void computeArithmeticPixels(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4);

    template <int b1, int b4>
    static inline void computeArithmeticPixelsUnclamped(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4);

    static inline void applyPlatformArithmetic(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4);

    bool applyArithmetic(FilterEffect* in, FilterEffect* in2);
    bool applyNonArithmetic(FilterEffect* in, FilterEffect* in2);
};

uint8_t FECompositeSoftwareApplier::clampByte(int c)
{
    uint8_t buff[] = { static_cast<uint8_t>(c), 255, 0 };
    unsigned uc = static_cast<unsigned>(c);
    return buff[!!(uc & ~0xff) + !!(uc & ~(~0u >> 1))];
}

template <int b1, int b4>
inline void FECompositeSoftwareApplier::computeArithmeticPixels(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4)
{
    float scaledK1;
    float scaledK4;
    if (b1)
        scaledK1 = k1 / 255.0f;
    if (b4)
        scaledK4 = k4 * 255.0f;

    while (--pixelArrayLength >= 0) {
        unsigned char i1 = *source;
        unsigned char i2 = *destination;
        float result = k2 * i1 + k3 * i2;
        if (b1)
            result += scaledK1 * i1 * i2;
        if (b4)
            result += scaledK4;

        *destination = clampByte(result);
        ++source;
        ++destination;
    }
}

// computeArithmeticPixelsUnclamped is a faster version of computeArithmeticPixels for the common case where clamping
// is not necessary. This enables aggresive compiler optimizations such as auto-vectorization.
template <int b1, int b4>
inline void FECompositeSoftwareApplier::computeArithmeticPixelsUnclamped(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4)
{
    float scaledK1;
    float scaledK4;
    if (b1)
        scaledK1 = k1 / 255.0f;
    if (b4)
        scaledK4 = k4 * 255.0f;

    while (--pixelArrayLength >= 0) {
        unsigned char i1 = *source;
        unsigned char i2 = *destination;
        float result = k2 * i1 + k3 * i2;
        if (b1)
            result += scaledK1 * i1 * i2;
        if (b4)
            result += scaledK4;

        *destination = result;
        ++source;
        ++destination;
    }
}

#if !HAVE(ARM_NEON_INTRINSICS)
inline void FECompositeSoftwareApplier::applyPlatformArithmetic(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4)
{
    float upperLimit = std::max(0.0f, k1) + std::max(0.0f, k2) + std::max(0.0f, k3) + k4;
    float lowerLimit = std::min(0.0f, k1) + std::min(0.0f, k2) + std::min(0.0f, k3) + k4;
    if ((k4 >= 0.0f && k4 <= 1.0f) && (upperLimit >= 0.0f && upperLimit <= 1.0f) && (lowerLimit >= 0.0f && lowerLimit <= 1.0f)) {
        if (k4) {
            if (k1)
                computeArithmeticPixelsUnclamped<1, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
            else
                computeArithmeticPixelsUnclamped<0, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        } else {
            if (k1)
                computeArithmeticPixelsUnclamped<1, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
            else
                computeArithmeticPixelsUnclamped<0, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        }
        return;
    }

    if (k4) {
        if (k1)
            computeArithmeticPixels<1, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        else
            computeArithmeticPixels<0, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
    } else {
        if (k1)
            computeArithmeticPixels<1, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        else
            computeArithmeticPixels<0, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
    }
}
#endif

bool FECompositeSoftwareApplier::applyArithmetic(FilterEffect* in, FilterEffect* in2)
{
    auto destinationPixelBuffer = m_effect.pixelBufferResult(AlphaPremultiplication::Premultiplied);
    if (!destinationPixelBuffer)
        return false;

    IntRect effectADrawingRect = m_effect.requestedRegionOfInputPixelBuffer(in->absolutePaintRect());
    auto sourcePixelBuffer = in->getPixelBufferResult(AlphaPremultiplication::Premultiplied, effectADrawingRect, m_effect.operatingColorSpace());
    if (!sourcePixelBuffer)
        return false;

    IntRect effectBDrawingRect = m_effect.requestedRegionOfInputPixelBuffer(in2->absolutePaintRect());
    in2->copyPixelBufferResult(*destinationPixelBuffer, effectBDrawingRect);

    auto& sourcePixelArray = sourcePixelBuffer->data();
    auto& destinationPixelArray = destinationPixelBuffer->data();

    int length = sourcePixelArray.length();
    ASSERT(length == static_cast<int>(destinationPixelArray.length()));
    applyPlatformArithmetic(sourcePixelArray.data(), destinationPixelArray.data(), length, m_effect.k1(), m_effect.k2(), m_effect.k3(), m_effect.k4());
    return true;
}

bool FECompositeSoftwareApplier::applyNonArithmetic(FilterEffect* in, FilterEffect* in2)
{
    auto resultImage = m_effect.imageBufferResult();
    if (!resultImage)
        return false;

    auto imageBuffer = in->imageBufferResult();
    auto imageBuffer2 = in2->imageBufferResult();
    if (!imageBuffer || !imageBuffer2)
        return false;

    auto& filterContext = resultImage->context();

    switch (m_effect.operation()) {
    case FECOMPOSITE_OPERATOR_UNKNOWN:
        return false;

    case FECOMPOSITE_OPERATOR_OVER:
        filterContext.drawImageBuffer(*imageBuffer2, m_effect.drawingRegionOfInputImage(in2->absolutePaintRect()));
        filterContext.drawImageBuffer(*imageBuffer, m_effect.drawingRegionOfInputImage(in->absolutePaintRect()));
        break;

    case FECOMPOSITE_OPERATOR_IN: {
        // Applies only to the intersected region.
        IntRect destinationRect = in->absolutePaintRect();
        destinationRect.intersect(in2->absolutePaintRect());
        destinationRect.intersect(m_effect.absolutePaintRect());
        if (destinationRect.isEmpty())
            break;
        IntRect adjustedDestinationRect = destinationRect - m_effect.absolutePaintRect().location();
        IntRect sourceRect = destinationRect - in->absolutePaintRect().location();
        IntRect source2Rect = destinationRect - in2->absolutePaintRect().location();
        filterContext.drawImageBuffer(*imageBuffer2, FloatRect(adjustedDestinationRect), FloatRect(source2Rect));
        filterContext.drawImageBuffer(*imageBuffer, FloatRect(adjustedDestinationRect), FloatRect(sourceRect), { CompositeOperator::SourceIn });
        break;
    }

    case FECOMPOSITE_OPERATOR_OUT:
        filterContext.drawImageBuffer(*imageBuffer, m_effect.drawingRegionOfInputImage(in->absolutePaintRect()));
        filterContext.drawImageBuffer(*imageBuffer2, m_effect.drawingRegionOfInputImage(in2->absolutePaintRect()), { { }, imageBuffer2->logicalSize() }, CompositeOperator::DestinationOut);
        break;

    case FECOMPOSITE_OPERATOR_ATOP:
        filterContext.drawImageBuffer(*imageBuffer2, m_effect.drawingRegionOfInputImage(in2->absolutePaintRect()));
        filterContext.drawImageBuffer(*imageBuffer, m_effect.drawingRegionOfInputImage(in->absolutePaintRect()), { { }, imageBuffer->logicalSize() }, CompositeOperator::SourceAtop);
        break;

    case FECOMPOSITE_OPERATOR_XOR:
        filterContext.drawImageBuffer(*imageBuffer2, m_effect.drawingRegionOfInputImage(in2->absolutePaintRect()));
        filterContext.drawImageBuffer(*imageBuffer, m_effect.drawingRegionOfInputImage(in->absolutePaintRect()), { { }, imageBuffer->logicalSize() }, CompositeOperator::XOR);
        break;

    case FECOMPOSITE_OPERATOR_ARITHMETIC:
        ASSERT_NOT_REACHED();
        return false;

    case FECOMPOSITE_OPERATOR_LIGHTER:
        filterContext.drawImageBuffer(*imageBuffer2, m_effect.drawingRegionOfInputImage(in2->absolutePaintRect()));
        filterContext.drawImageBuffer(*imageBuffer, m_effect.drawingRegionOfInputImage(in->absolutePaintRect()), { { }, imageBuffer->logicalSize() }, CompositeOperator::PlusLighter);
        break;
    }

    return true;
}

bool FECompositeSoftwareApplier::apply(const Filter&, const FilterEffectVector& inputEffects)
{
    FilterEffect* in = inputEffects[0].get();
    FilterEffect* in2 = inputEffects[1].get();

    if (m_effect.operation() == FECOMPOSITE_OPERATOR_ARITHMETIC)
        return applyArithmetic(in, in2);
    return applyNonArithmetic(in, in2);
}

bool FEComposite::platformApplySoftware(const Filter& filter)
{
    return FECompositeSoftwareApplier(*this).apply(filter, inputEffects());
}

static TextStream& operator<<(TextStream& ts, const CompositeOperationType& type)
{
    switch (type) {
    case FECOMPOSITE_OPERATOR_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case FECOMPOSITE_OPERATOR_OVER:
        ts << "OVER";
        break;
    case FECOMPOSITE_OPERATOR_IN:
        ts << "IN";
        break;
    case FECOMPOSITE_OPERATOR_OUT:
        ts << "OUT";
        break;
    case FECOMPOSITE_OPERATOR_ATOP:
        ts << "ATOP";
        break;
    case FECOMPOSITE_OPERATOR_XOR:
        ts << "XOR";
        break;
    case FECOMPOSITE_OPERATOR_ARITHMETIC:
        ts << "ARITHMETIC";
        break;
    case FECOMPOSITE_OPERATOR_LIGHTER:
        ts << "LIGHTER";
        break;
    }
    return ts;
}

TextStream& FEComposite::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feComposite";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " operation=\"" << m_type << "\"";
    if (m_type == FECOMPOSITE_OPERATOR_ARITHMETIC)
        ts << " k1=\"" << m_k1 << "\" k2=\"" << m_k2 << "\" k3=\"" << m_k3 << "\" k4=\"" << m_k4 << "\"";
    ts << "]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    inputEffect(1)->externalRepresentation(ts, representation);
    return ts;
}

} // namespace WebCore
