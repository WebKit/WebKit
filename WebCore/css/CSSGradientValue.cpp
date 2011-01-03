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

#include "CSSValueKeywords.h"
#include "CSSStyleSelector.h"
#include "GeneratedImage.h"
#include "Gradient.h"
#include "Image.h"
#include "IntSize.h"
#include "IntSizeHash.h"
#include "NodeRenderStyle.h"
#include "PlatformString.h"
#include "RenderObject.h"

using namespace std;

namespace WebCore {

Image* CSSGradientValue::image(RenderObject* renderer, const IntSize& size)
{
    ASSERT(m_clients.contains(renderer));

    // Need to look up our size.  Create a string of width*height to use as a hash key.
    // FIXME: hashing based only on size is not sufficient. Color stops may use context-sensitive units (like em)
    // that should force the color stop positions to be recomputed.
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

// Should only ever be called for deprecated gradients.
static inline bool compareStops(const CSSGradientColorStop& a, const CSSGradientColorStop& b)
{
    double aVal = a.m_position->getDoubleValue(CSSPrimitiveValue::CSS_NUMBER);
    double bVal = b.m_position->getDoubleValue(CSSPrimitiveValue::CSS_NUMBER);
    
    return aVal < bVal;
}

void CSSGradientValue::sortStopsIfNeeded()
{
    ASSERT(m_deprecatedType);
    if (!m_stopsSorted) {
        if (m_stops.size())
            std::stable_sort(m_stops.begin(), m_stops.end(), compareStops);
        m_stopsSorted = true;
    }
}

void CSSGradientValue::addStops(Gradient* gradient, RenderObject* renderer, RenderStyle* rootStyle)
{
    RenderStyle* style = renderer->style();
    
    if (m_deprecatedType) {
        sortStopsIfNeeded();

        // We have to resolve colors.
        for (unsigned i = 0; i < m_stops.size(); i++) {
            const CSSGradientColorStop& stop = m_stops[i];
            Color color = renderer->document()->styleSelector()->getColorFromPrimitiveValue(stop.m_color.get());

            float offset;
            if (stop.m_position->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
                offset = stop.m_position->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE) / 100;
            else
                offset = stop.m_position->getFloatValue(CSSPrimitiveValue::CSS_NUMBER);
            
            gradient->addColorStop(offset, color);
        }

        // The back end already sorted the stops.
        gradient->setStopsSorted(true);
        return;
    }

    size_t numStops = m_stops.size();
    // Pair of <offset, specified>.
    Vector<pair<float, bool> > positions(numStops);
    
    float gradientLength = 0;
    bool computedGradientLength = false;
    
    for (size_t i = 0; i < numStops; ++i) {
        const CSSGradientColorStop& stop = m_stops[i];

        positions[i].first = 0;
        positions[i].second = false;

        if (stop.m_position) {
            int type = stop.m_position->primitiveType();
            if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                positions[i].first = stop.m_position->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE) / 100;
            else if (CSSPrimitiveValue::isUnitTypeLength(type)) {
                float length = stop.m_position->computeLengthFloat(style, rootStyle, style->effectiveZoom());
                if (!computedGradientLength) {
                    float xDelta = gradient->p0().x() - gradient->p1().x();
                    float yDelta = gradient->p0().y() - gradient->p1().y();
                    gradientLength = sqrtf(xDelta * xDelta + yDelta * yDelta);
                }
                positions[i].first = (gradientLength > 0) ? length / gradientLength : 0;
            } else {
                ASSERT_NOT_REACHED();
                positions[i].first = 0;
            }
            positions[i].second = true;
        } else {
            // If the first color-stop does not have a position, its position defaults to 0%.
            // If the last color-stop does not have a position, its position defaults to 100%.
            if (!i) {
                positions[i].first = 0;
                positions[i].second = true;
            } else if (numStops > 1 && i == numStops - 1) {
                positions[i].first = 1;
                positions[i].second = true;
            }
        }

        // If a color-stop has a position that is less than the specified position of any
        // color-stop before it in the list, its position is changed to be equal to the
        // largest specified position of any color-stop before it.
        if (positions[i].second && i > 0) {
            size_t prevSpecifiedIndex;
            for (prevSpecifiedIndex = i - 1; prevSpecifiedIndex; --prevSpecifiedIndex) {
                if (positions[prevSpecifiedIndex].second)
                    break;
            }
            
            if (positions[i].first < positions[prevSpecifiedIndex].first)
                positions[i].first = positions[prevSpecifiedIndex].first;
        }
    }

    ASSERT(positions[0].second && positions[numStops - 1].second);
    
    // If any color-stop still does not have a position, then, for each run of adjacent
    // color-stops without positions, set their positions so that they are evenly spaced
    // between the preceding and following color-stops with positions.
    if (numStops > 2) {
        size_t unspecifiedRunStart = 0;
        bool inUnspecifiedRun = false;

        for (size_t i = 0; i < numStops; ++i) {
            if (!positions[i].second && !inUnspecifiedRun) {
                unspecifiedRunStart = i;
                inUnspecifiedRun = true;
            } else if (positions[i].second && inUnspecifiedRun) {
                size_t unspecifiedRunEnd = i;

                if (unspecifiedRunStart < unspecifiedRunEnd) {
                    float lastSpecifiedOffset = positions[unspecifiedRunStart - 1].first;
                    float nextSpecifiedOffset = positions[unspecifiedRunEnd].first;
                    float delta = (nextSpecifiedOffset - lastSpecifiedOffset) / (unspecifiedRunEnd - unspecifiedRunStart + 1);
                    
                    for (size_t j = unspecifiedRunStart; j < unspecifiedRunEnd; ++j)
                        positions[j].first = lastSpecifiedOffset + (j - unspecifiedRunStart + 1) * delta;
                }

                inUnspecifiedRun = false;
            }
        }
    }
    
    // If the gradient goes outside the 0-1 range, normalize it by moving the endpoints, and ajusting the stops.
    if (numStops > 1 && (positions[0].first < 0 || positions[numStops - 1].first > 1)) {
        float firstOffset = positions[0].first;
        float lastOffset = positions[numStops - 1].first;
        float scale = lastOffset - firstOffset;
        
        for (size_t i = 0; i < numStops; ++i)
            positions[i].first = (positions[i].first - firstOffset) / scale;

        FloatPoint p0 = gradient->p0();
        FloatPoint p1 = gradient->p1();
        gradient->setP0(FloatPoint(p0.x() + firstOffset * (p1.x() - p0.x()), p0.y() + firstOffset * (p1.y() - p0.y())));
        gradient->setP1(FloatPoint(p1.x() + (lastOffset - 1) * (p1.x() - p0.x()), p1.y() + (lastOffset - 1) * (p1.y() - p0.y())));
    }
    
    for (unsigned i = 0; i < numStops; i++) {
        const CSSGradientColorStop& stop = m_stops[i];
        Color color = renderer->document()->styleSelector()->getColorFromPrimitiveValue(stop.m_color.get());
        gradient->addColorStop(positions[i].first, color);
    }

    gradient->setStopsSorted(true);
}

static float positionFromValue(CSSPrimitiveValue* value, RenderStyle* style, RenderStyle* rootStyle, const IntSize& size, bool isHorizontal)
{
    float zoomFactor = style->effectiveZoom();

    switch (value->primitiveType()) {
    case CSSPrimitiveValue::CSS_NUMBER:
        return value->getFloatValue() * zoomFactor;

    case CSSPrimitiveValue::CSS_PERCENTAGE:
        return value->getFloatValue() / 100.f * (isHorizontal ? size.width() : size.height());

    case CSSPrimitiveValue::CSS_IDENT:
        switch (value->getIdent()) {
            case CSSValueTop:
                ASSERT(!isHorizontal);
                return 0;
            case CSSValueLeft:
                ASSERT(isHorizontal);
                return 0;
            case CSSValueBottom:
                ASSERT(!isHorizontal);
                return size.height();
            case CSSValueRight:
                ASSERT(isHorizontal);
                return size.width();
        }

    default:
        return value->computeLengthFloat(style, rootStyle, zoomFactor);
    }
}

FloatPoint CSSGradientValue::computeEndPoint(CSSPrimitiveValue* first, CSSPrimitiveValue* second, RenderStyle* style, RenderStyle* rootStyle, const IntSize& size)
{
    FloatPoint result;

    if (first)
        result.setX(positionFromValue(first, style, rootStyle, size, true));

    if (second)
        result.setY(positionFromValue(second, style, rootStyle, size, false));
        
    return result;
}

String CSSLinearGradientValue::cssText() const
{
    String result;
    if (m_deprecatedType) {
        result = "-webkit-gradient(linear, ";
        result += m_firstX->cssText() + " ";
        result += m_firstY->cssText() + ", ";
        result += m_secondX->cssText() + " ";
        result += m_secondY->cssText();

        for (unsigned i = 0; i < m_stops.size(); i++) {
            const CSSGradientColorStop& stop = m_stops[i];
            result += ", ";
            if (stop.m_position->getDoubleValue(CSSPrimitiveValue::CSS_NUMBER) == 0)
                result += "from(" + stop.m_color->cssText() + ")";
            else if (stop.m_position->getDoubleValue(CSSPrimitiveValue::CSS_NUMBER) == 1)
                result += "to(" + stop.m_color->cssText() + ")";
            else
                result += "color-stop(" + String::number(stop.m_position->getDoubleValue(CSSPrimitiveValue::CSS_NUMBER)) + ", " + stop.m_color->cssText() + ")";
        }
    } else {
        result = "-webkit-linear-gradient(";
        if (m_angle)
            result += m_angle->cssText();
        else {
            if (m_firstX && m_firstY)
                result += m_firstX->cssText() + " " + m_firstY->cssText();
            else if (m_firstX || m_firstY) {
                if (m_firstX)
                    result += m_firstX->cssText();

                if (m_firstY)
                    result += m_firstY->cssText();
            }
        }

        for (unsigned i = 0; i < m_stops.size(); i++) {
            const CSSGradientColorStop& stop = m_stops[i];
            result += ", ";
            result += stop.m_color->cssText();
            if (stop.m_position)
                result += " " + stop.m_position->cssText();
        }
    }

    result += ")";
    return result;
}

// Compute the endpoints so that a gradient of the given angle covers a box of the given size.
static void endPointsFromAngle(float angleDeg, const IntSize& size, FloatPoint& firstPoint, FloatPoint& secondPoint)
{
    angleDeg = fmodf(angleDeg, 360);
    if (angleDeg < 0)
        angleDeg += 360;
    
    if (!angleDeg) {
        firstPoint.set(0, 0);
        secondPoint.set(size.width(), 0);
        return;
    }
    
    if (angleDeg == 90) {
        firstPoint.set(0, size.height());
        secondPoint.set(0, 0);
        return;
    }

    if (angleDeg == 180) {
        firstPoint.set(size.width(), 0);
        secondPoint.set(0, 0);
        return;
    }

    float slope = tan(deg2rad(angleDeg));

    // We find the endpoint by computing the intersection of the line formed by the slope,
    // and a line perpendicular to it that intersects the corner.
    float perpendicularSlope = -1 / slope;
    
    // Compute start corner relative to center.
    float halfHeight = size.height() / 2;
    float halfWidth = size.width() / 2;
    FloatPoint endCorner;
    if (angleDeg < 90)
        endCorner.set(halfWidth, halfHeight);
    else if (angleDeg < 180)
        endCorner.set(-halfWidth, halfHeight);
    else if (angleDeg < 270)
        endCorner.set(-halfWidth, -halfHeight);
    else
        endCorner.set(halfWidth, -halfHeight);

    // Compute c (of y = mx + c) using the corner point.
    float c = endCorner.y() - perpendicularSlope * endCorner.x();
    float endX = c / (slope - perpendicularSlope);
    float endY = perpendicularSlope * endX + c;
    
    // We computed the end point, so set the second point, flipping the Y to account for angles going anticlockwise.
    secondPoint.set(halfWidth + endX, size.height() - (halfHeight + endY));
    // Reflect around the center for the start point.
    firstPoint.set(size.width() - secondPoint.x(), size.height() - secondPoint.y());
}

PassRefPtr<Gradient> CSSLinearGradientValue::createGradient(RenderObject* renderer, const IntSize& size)
{
    ASSERT(!size.isEmpty());
    
    RenderStyle* rootStyle = renderer->document()->documentElement()->renderStyle();

    FloatPoint firstPoint;
    FloatPoint secondPoint;
    if (m_angle) {
        float angle = m_angle->getDoubleValue(CSSPrimitiveValue::CSS_DEG);
        endPointsFromAngle(angle, size, firstPoint, secondPoint);
    } else {
        firstPoint = computeEndPoint(m_firstX.get(), m_firstY.get(), renderer->style(), rootStyle, size);
        
        if (m_secondX || m_secondY)
            secondPoint = computeEndPoint(m_secondX.get(), m_secondY.get(), renderer->style(), rootStyle, size);
        else {
            if (m_firstX)
                secondPoint.setX(size.width() - firstPoint.x());
            if (m_firstY)
                secondPoint.setY(size.height() - firstPoint.y());
        }
    }

    RefPtr<Gradient> gradient = Gradient::create(firstPoint, secondPoint);

    // Now add the stops.
    addStops(gradient.get(), renderer, rootStyle);

    return gradient.release();
}

String CSSRadialGradientValue::cssText() const
{
    String result;

    if (m_deprecatedType)
        result = "-webkit-gradient(radial, ";
    else
        result = "-webkit-radial-gradient(";

    result += m_firstX->cssText() + " ";
    result += m_firstY->cssText() + ", ";
    result += m_firstRadius->cssText() + ", ";
    result += m_secondX->cssText() + " ";
    result += m_secondY->cssText();
    result += ", ";
    result += m_secondRadius->cssText();

    for (unsigned i = 0; i < m_stops.size(); i++) {
        result += ", ";
        if (m_stops[i].m_position->getDoubleValue(CSSPrimitiveValue::CSS_NUMBER) == 0)
            result += "from(" + m_stops[i].m_color->cssText() + ")";
        else if (m_stops[i].m_position->getDoubleValue(CSSPrimitiveValue::CSS_NUMBER) == 1)
            result += "to(" + m_stops[i].m_color->cssText() + ")";
        else
            result += "color-stop(" + String::number(m_stops[i].m_position->getDoubleValue(CSSPrimitiveValue::CSS_NUMBER)) + ", " + m_stops[i].m_color->cssText() + ")";
    }
    result += ")";
    return result;
}

float CSSRadialGradientValue::resolveRadius(CSSPrimitiveValue* radius, RenderStyle* style, RenderStyle* rootStyle)
{
    float zoomFactor = style->effectiveZoom();

    float result = 0.f;
    if (radius->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)  // Can the radius be a percentage?
        result = radius->getFloatValue() * zoomFactor;
    else
        result = radius->computeLengthFloat(style, rootStyle, zoomFactor);
 
    return result;
}

// FIXME: share code with the linear version
PassRefPtr<Gradient> CSSRadialGradientValue::createGradient(RenderObject* renderer, const IntSize& size)
{
    ASSERT(!size.isEmpty());
    
    RenderStyle* rootStyle = renderer->document()->documentElement()->renderStyle();

    FloatPoint firstPoint = computeEndPoint(m_firstX.get(), m_firstY.get(), renderer->style(), rootStyle, size);
    FloatPoint secondPoint = computeEndPoint(m_secondX.get(), m_secondY.get(), renderer->style(), rootStyle, size);
    
    float firstRadius = resolveRadius(m_firstRadius.get(), renderer->style(), rootStyle);
    float secondRadius = resolveRadius(m_secondRadius.get(), renderer->style(), rootStyle);
    RefPtr<Gradient> gradient = Gradient::create(firstPoint, firstRadius, secondPoint, secondRadius);

    // Now add the stops.
    sortStopsIfNeeded();

    // We have to resolve colors.
    for (unsigned i = 0; i < m_stops.size(); i++) {
        Color color = renderer->document()->styleSelector()->getColorFromPrimitiveValue(m_stops[i].m_color.get());
        gradient->addColorStop(m_stops[i].m_position->getDoubleValue(CSSPrimitiveValue::CSS_NUMBER), color);
    }

    // The back end already sorted the stops.
    gradient->setStopsSorted(true);

    return gradient.release();
}

} // namespace WebCore
