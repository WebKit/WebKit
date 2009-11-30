/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CSSGradientValue.h"

#include "CSSStyleSelector.h"
#include "GeneratedImage.h"
#include "Gradient.h"
#include "Image.h"
#include "IntSize.h"
#include "IntSizeHash.h"
#include "PlatformString.h"
#include "RenderObject.h"

using namespace std;

namespace WebCore {

String CSSGradientValue::cssText() const
{
    String result = "-webkit-gradient(";
    if (m_type == CSSLinearGradient)
        result += "linear, ";
    else
        result += "radial, ";
    result += m_firstX->cssText() + " ";
    result += m_firstY->cssText() + ", ";
    if (m_type == CSSRadialGradient)
        result += m_firstRadius->cssText() + ", ";
    result += m_secondX->cssText() + " ";
    result += m_secondY->cssText();
    if (m_type == CSSRadialGradient) {
        result += ", ";
        result += m_secondRadius->cssText();
    }
    for (unsigned i = 0; i < m_stops.size(); i++) {
        result += ", ";
        if (m_stops[i].m_stop == 0)
            result += "from(" + m_stops[i].m_color->cssText() + ")";
        else if (m_stops[i].m_stop == 1)
            result += "to(" + m_stops[i].m_color->cssText() + ")";
        else
            result += "color-stop(" + String::number(m_stops[i].m_stop) + ", " + m_stops[i].m_color->cssText() + ")";
    }
    result += ")";
    return result;
}

PassRefPtr<Gradient> CSSGradientValue::createGradient(RenderObject* renderer, const IntSize& size)
{
    ASSERT(!size.isEmpty());
    
    float zoomFactor = renderer->style()->effectiveZoom();
    
    FloatPoint firstPoint = resolvePoint(m_firstX.get(), m_firstY.get(), size, zoomFactor);
    FloatPoint secondPoint = resolvePoint(m_secondX.get(), m_secondY.get(), size, zoomFactor);
    
    RefPtr<Gradient> gradient;
    if (m_type == CSSLinearGradient)
        gradient = Gradient::create(firstPoint, secondPoint);
    else {
        float firstRadius = resolveRadius(m_firstRadius.get(), zoomFactor);
        float secondRadius = resolveRadius(m_secondRadius.get(), zoomFactor);
        gradient = Gradient::create(firstPoint, firstRadius, secondPoint, secondRadius);
    }

    // Now add the stops.
    sortStopsIfNeeded();

    // We have to resolve colors.
    for (unsigned i = 0; i < m_stops.size(); i++) {
        Color color = renderer->document()->styleSelector()->getColorFromPrimitiveValue(m_stops[i].m_color.get());
        gradient->addColorStop(m_stops[i].m_stop, color);
    }

    // The back end already sorted the stops.
    gradient->setStopsSorted(true);

    return gradient.release();
}

Image* CSSGradientValue::image(RenderObject* renderer, const IntSize& size)
{
    ASSERT(m_clients.contains(renderer));

    // Need to look up our size.  Create a string of width*height to use as a hash key.
    Image* result = getImage(renderer, size);
    if (result)
        return result;

    if (size.isEmpty())
        return 0;

    // We need to create an image.
    RefPtr<Image> newImage = GeneratedImage::create(createGradient(renderer, size), size);
    result = newImage.get();
    putImage(size, newImage.release());

    return result;
}

static inline bool compareStops(const CSSGradientColorStop& a, const CSSGradientColorStop& b)
{
    return a.m_stop < b.m_stop;
}

void CSSGradientValue::sortStopsIfNeeded()
{
    if (!m_stopsSorted) {
        if (m_stops.size())
            std::stable_sort(m_stops.begin(), m_stops.end(), compareStops);
        m_stopsSorted = true;
    }
}

FloatPoint CSSGradientValue::resolvePoint(CSSPrimitiveValue* first, CSSPrimitiveValue* second, const IntSize& size, float zoomFactor)
{
    FloatPoint result;
    if (first->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)
        result.setX(first->getFloatValue() * zoomFactor);
    else if (first->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
        result.setX(first->getFloatValue() / 100.f * size.width());
    if (second->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)
        result.setY(second->getFloatValue() * zoomFactor);
    else if (second->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
        result.setY(second->getFloatValue() / 100.f * size.height());

    return result;
}

float CSSGradientValue::resolveRadius(CSSPrimitiveValue* radius, float zoomFactor)
{
    float result = 0.f;
    if (radius->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)
        result = radius->getFloatValue() * zoomFactor;
    return result;
}

} // namespace WebCore
