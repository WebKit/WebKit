/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include "CSSBasicShapes.h"

#include <wtf/text/StringBuilder.h>

using namespace WTF;

namespace WebCore {

static String buildRectangleString(const String& x, const String& y, const String& width, const String& height, const String& radiusX, const String& radiusY)
{
    StringBuilder result;
    result.appendLiteral("rectangle(");
    result.append(x);
    result.appendLiteral(", ");
    result.append(y);
    result.appendLiteral(", ");
    result.append(width);
    result.appendLiteral(", ");
    result.append(height);
    if (!radiusX.isNull()) {
        result.appendLiteral(", ");
        result.append(radiusX);
        if (!radiusY.isNull()) {
            result.appendLiteral(", ");
            result.append(radiusY);
        }
    }
    result.append(')');
    return result.toString();
}

String CSSBasicShapeRectangle::cssText() const
{
    return buildRectangleString(m_x->cssText(),
                                m_y->cssText(),
                                m_width->cssText(),
                                m_height->cssText(),
                                m_radiusX.get() ? m_radiusX->cssText() : String(),
                                m_radiusY.get() ? m_radiusY->cssText() : String());
}

#if ENABLE(CSS_VARIABLES)
String CSSBasicShapeRectangle::serializeResolvingVariables(const HashMap<AtomicString, String>& variables) const
{
    return buildRectangleString(m_x->serializeResolvingVariables(variables),
                                m_y->serializeResolvingVariables(variables),
                                m_width->serializeResolvingVariables(variables),
                                m_height->serializeResolvingVariables(variables),
                                m_radiusX.get() ? m_radiusX->serializeResolvingVariables(variables) : String(),
                                m_radiusY.get() ? m_radiusY->serializeResolvingVariables(variables) : String());
}

bool CSSBasicShapeRectangle::hasVariableReference() const
{
    return m_x->hasVariableReference()
        || m_y->hasVariableReference()
        || m_width->hasVariableReference()
        || m_height->hasVariableReference()
        || (m_radiusX.get() && m_radiusX->hasVariableReference())
        || (m_radiusY.get() && m_radiusY->hasVariableReference());
}
#endif

static String buildCircleString(const String& x, const String& y, const String& radius)
{
    return "circle(" + x + ", " + y + ", " + radius + ')';
}

String CSSBasicShapeCircle::cssText() const
{
    return buildCircleString(m_centerX->cssText(), m_centerY->cssText(), m_radius->cssText());
}

#if ENABLE(CSS_VARIABLES)
String CSSBasicShapeCircle::serializeResolvingVariables(const HashMap<AtomicString, String>& variables) const
{
    return buildCircleString(m_centerX->serializeResolvingVariables(variables),
                             m_centerY->serializeResolvingVariables(variables),
                             m_radius->serializeResolvingVariables(variables));
}

bool CSSBasicShapeCircle::hasVariableReference() const
{
    return m_centerX->hasVariableReference()
        || m_centerY->hasVariableReference()
        || m_radius->hasVariableReference();
}
#endif

static String buildEllipseString(const String& x, const String& y, const String& radiusX, const String& radiusY)
{
    return "ellipse(" + x + ", " + y + ", " + radiusX + ", " + radiusY + ')';
}

String CSSBasicShapeEllipse::cssText() const
{
    return buildEllipseString(m_centerX->cssText(), m_centerY->cssText(), m_radiusX->cssText(), m_radiusY->cssText());
}

#if ENABLE(CSS_VARIABLES)
String CSSBasicShapeEllipse::serializeResolvingVariables(const HashMap<AtomicString, String>& variables) const
{
    return buildEllipseString(m_centerX->serializeResolvingVariables(variables),
                              m_centerY->serializeResolvingVariables(variables),
                              m_radiusX->serializeResolvingVariables(variables),
                              m_radiusY->serializeResolvingVariables(variables));
}

bool CSSBasicShapeEllipse::hasVariableReference() const
{
    return m_centerX->hasVariableReference()
        || m_centerY->hasVariableReference()
        || m_radiusX->hasVariableReference()
        || m_radiusY->hasVariableReference();
}
#endif

static String buildPolygonString(const WindRule& windRule, const Vector<String>& points)
{
    StringBuilder result;

    if (windRule == RULE_EVENODD)
        result.appendLiteral("polygon(evenodd, ");
    else
        result.appendLiteral("polygon(nonzero, ");

    ASSERT(!(points.size() % 2));

    for (size_t i = 0; i < points.size(); i += 2) {
        if (i)
            result.appendLiteral(", ");
        result.append(points[i]);
        result.append(' ');
        result.append(points[i + 1]);
    }

    result.append(')');

    return result.toString();
}

String CSSBasicShapePolygon::cssText() const
{
    Vector<String> points;
    points.reserveInitialCapacity(m_values.size());

    for (size_t i = 0; i < m_values.size(); ++i)
        points.append(m_values.at(i)->cssText());

    return buildPolygonString(m_windRule, points);
}

#if ENABLE(CSS_VARIABLES)
String CSSBasicShapePolygon::serializeResolvingVariables(const HashMap<AtomicString, String>& variables) const
{
    Vector<String> points;
    points.reserveInitialCapacity(m_values.size());

    for (size_t i = 0; i < m_values.size(); ++i)
        points.append(m_values.at(i)->serializeResolvingVariables(variables));

    return buildPolygonString(m_windRule, points);
}

bool CSSBasicShapePolygon::hasVariableReference() const
{
    for (size_t i = 0; i < m_values.size(); ++i) {
        if (m_values.at(i)->hasVariableReference())
            return true;
    }
    return false;
}
#endif

} // namespace WebCore

