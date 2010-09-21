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
#include "FEComposite.h"

#include "CanvasPixelArray.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageData.h"

namespace WebCore {

FEComposite::FEComposite(const CompositeOperationType& type, float k1, float k2, float k3, float k4)
    : FilterEffect()
    , m_type(type)
    , m_k1(k1)
    , m_k2(k2)
    , m_k3(k3)
    , m_k4(k4)
{
}

PassRefPtr<FEComposite> FEComposite::create(const CompositeOperationType& type, float k1, float k2, float k3, float k4)
{
    return adoptRef(new FEComposite(type, k1, k2, k3, k4));
}

CompositeOperationType FEComposite::operation() const
{
    return m_type;
}

void FEComposite::setOperation(CompositeOperationType type)
{
    m_type = type;
}

float FEComposite::k1() const
{
    return m_k1;
}

void FEComposite::setK1(float k1)
{
    m_k1 = k1;
}

float FEComposite::k2() const
{
    return m_k2;
}

void FEComposite::setK2(float k2)
{
    m_k2 = k2;
}

float FEComposite::k3() const
{
    return m_k3;
}

void FEComposite::setK3(float k3)
{
    m_k3 = k3;
}

float FEComposite::k4() const
{
    return m_k4;
}

void FEComposite::setK4(float k4)
{
    m_k4 = k4;
}

inline void arithmetic(const RefPtr<CanvasPixelArray>& srcPixelArrayA, CanvasPixelArray*& srcPixelArrayB,
                       float k1, float k2, float k3, float k4)
{
    float scaledK1 = k1 / 255.f;
    float scaledK4 = k4 * 255.f;
    for (unsigned pixelOffset = 0; pixelOffset < srcPixelArrayA->length(); pixelOffset += 4) {
        for (unsigned channel = 0; channel < 4; ++channel) {
            unsigned char i1 = srcPixelArrayA->get(pixelOffset + channel);
            unsigned char i2 = srcPixelArrayB->get(pixelOffset + channel);

            double result = scaledK1 * i1 * i2 + k2 * i1 + k3 * i2 + scaledK4;
            srcPixelArrayB->set(pixelOffset + channel, result);
        }
    }
}

void FEComposite::apply(Filter* filter)
{
    FilterEffect* in = inputEffect(0);
    FilterEffect* in2 = inputEffect(1);
    in->apply(filter);
    in2->apply(filter);
    if (!in->resultImage() || !in2->resultImage())
        return;

    GraphicsContext* filterContext = effectContext();
    if (!filterContext)
        return;

    FloatRect srcRect = FloatRect(0, 0, -1, -1);
    switch (m_type) {
    case FECOMPOSITE_OPERATOR_OVER:
        filterContext->drawImageBuffer(in2->resultImage(), DeviceColorSpace, drawingRegionOfInputImage(in2->repaintRectInLocalCoordinates()));
        filterContext->drawImageBuffer(in->resultImage(), DeviceColorSpace, drawingRegionOfInputImage(in->repaintRectInLocalCoordinates()));
        break;
    case FECOMPOSITE_OPERATOR_IN:
        filterContext->save();
        filterContext->clipToImageBuffer(in2->resultImage(), drawingRegionOfInputImage(in2->repaintRectInLocalCoordinates()));
        filterContext->drawImageBuffer(in->resultImage(), DeviceColorSpace, drawingRegionOfInputImage(in->repaintRectInLocalCoordinates()));
        filterContext->restore();
        break;
    case FECOMPOSITE_OPERATOR_OUT:
        filterContext->drawImageBuffer(in->resultImage(), DeviceColorSpace, drawingRegionOfInputImage(in->repaintRectInLocalCoordinates()));
        filterContext->drawImageBuffer(in2->resultImage(), DeviceColorSpace, drawingRegionOfInputImage(in2->repaintRectInLocalCoordinates()), srcRect, CompositeDestinationOut);
        break;
    case FECOMPOSITE_OPERATOR_ATOP:
        filterContext->drawImageBuffer(in2->resultImage(), DeviceColorSpace, drawingRegionOfInputImage(in2->repaintRectInLocalCoordinates()));
        filterContext->drawImageBuffer(in->resultImage(), DeviceColorSpace, drawingRegionOfInputImage(in->repaintRectInLocalCoordinates()), srcRect, CompositeSourceAtop);
        break;
    case FECOMPOSITE_OPERATOR_XOR:
        filterContext->drawImageBuffer(in2->resultImage(), DeviceColorSpace, drawingRegionOfInputImage(in2->repaintRectInLocalCoordinates()));
        filterContext->drawImageBuffer(in->resultImage(), DeviceColorSpace, drawingRegionOfInputImage(in->repaintRectInLocalCoordinates()), srcRect, CompositeXOR);
        break;
    case FECOMPOSITE_OPERATOR_ARITHMETIC: {
        IntRect effectADrawingRect = requestedRegionOfInputImageData(in->repaintRectInLocalCoordinates());
        RefPtr<CanvasPixelArray> srcPixelArrayA(in->resultImage()->getPremultipliedImageData(effectADrawingRect)->data());

        IntRect effectBDrawingRect = requestedRegionOfInputImageData(in2->repaintRectInLocalCoordinates());
        RefPtr<ImageData> imageData(in2->resultImage()->getPremultipliedImageData(effectBDrawingRect));
        CanvasPixelArray* srcPixelArrayB(imageData->data());

        arithmetic(srcPixelArrayA, srcPixelArrayB, m_k1, m_k2, m_k3, m_k4);
        resultImage()->putPremultipliedImageData(imageData.get(), IntRect(IntPoint(), resultImage()->size()), IntPoint());
        }
        break;
    default:
        break;
    }
}

void FEComposite::dump()
{
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
    }
    return ts;
}

TextStream& FEComposite::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feComposite";
    FilterEffect::externalRepresentation(ts);
    ts << " operation=\"" << m_type << "\"";
    if (m_type == FECOMPOSITE_OPERATOR_ARITHMETIC)
        ts << " k1=\"" << m_k1 << "\" k2=\"" << m_k2 << "\" k3=\"" << m_k3 << "\" k4=\"" << m_k4 << "\"";
    ts << "]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    inputEffect(1)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
