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

#include "CSSMarkup.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "SVGPathByteStream.h"
#include "SVGPathUtilities.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSCircleValue::CSSCircleValue(RefPtr<CSSValue>&& radius, RefPtr<CSSValue>&& centerX, RefPtr<CSSValue>&& centerY)
    : CSSValue(ClassType::Circle)
    , m_radius(WTFMove(radius))
    , m_centerX(WTFMove(centerX))
    , m_centerY(WTFMove(centerY))
{
}

Ref<CSSCircleValue> CSSCircleValue::create(RefPtr<CSSValue>&& radius, RefPtr<CSSValue>&& centerX, RefPtr<CSSValue>&& centerY)
{
    return adoptRef(*new CSSCircleValue(WTFMove(radius), WTFMove(centerX), WTFMove(centerY)));
}

String CSSCircleValue::customCSSText() const
{
    String radius;
    if (m_radius && m_radius->valueID() != CSSValueClosestSide)
        radius = m_radius->cssText();

    if (!m_centerX) {
        ASSERT(!m_centerY);
        return makeString("circle("_s, radius, ')');
    }

    return makeString("circle("_s, radius, radius.isNull() ? ""_s : " "_s, "at "_s, m_centerX->cssText(), ' ', m_centerY->cssText(), ')');
}

bool CSSCircleValue::equals(const CSSCircleValue& other) const
{
    return compareCSSValuePtr(m_centerX, other.m_centerX)
        && compareCSSValuePtr(m_centerY, other.m_centerY)
        && compareCSSValuePtr(m_radius, other.m_radius);
}

// MARK: -

CSSEllipseValue::CSSEllipseValue(RefPtr<CSSValue>&& radiusX, RefPtr<CSSValue>&& radiusY, RefPtr<CSSValue>&& centerX, RefPtr<CSSValue>&& centerY)
    : CSSValue(ClassType::Ellipse)
    , m_radiusX(WTFMove(radiusX))
    , m_radiusY(WTFMove(radiusY))
    , m_centerX(WTFMove(centerX))
    , m_centerY(WTFMove(centerY))
{
}

Ref<CSSEllipseValue> CSSEllipseValue::create(RefPtr<CSSValue>&& radiusX, RefPtr<CSSValue>&& radiusY, RefPtr<CSSValue>&& centerX, RefPtr<CSSValue>&& centerY)
{
    return adoptRef(*new CSSEllipseValue(WTFMove(radiusX), WTFMove(radiusY), WTFMove(centerX), WTFMove(centerY)));
}

static String buildEllipseString(const String& radiusX, const String& radiusY, const String& centerX, const String& centerY)
{
    StringBuilder result;
    result.append("ellipse("_s);
    bool needsSeparator = false;
    if (!radiusX.isNull()) {
        result.append(radiusX);
        needsSeparator = true;
    }
    if (!radiusY.isNull()) {
        if (needsSeparator)
            result.append(' ');
        result.append(radiusY);
        needsSeparator = true;
    }
    if (!centerX.isNull() || !centerY.isNull()) {
        if (needsSeparator)
            result.append(' ');
        result.append("at "_s, centerX, ' ', centerY);
    }
    result.append(')');
    return result.toString();
}

String CSSEllipseValue::customCSSText() const
{
    String radiusX;
    String radiusY;
    if (m_radiusX) {
        ASSERT(m_radiusY);
        bool radiusXClosestSide = m_radiusX->valueID() == CSSValueClosestSide;
        bool radiusYClosestSide = m_radiusY->valueID() == CSSValueClosestSide;
        if (!radiusXClosestSide || !radiusYClosestSide) {
            radiusX = m_radiusX->cssText();
            radiusY = m_radiusY->cssText();
        }
    }
    if (!m_centerX) {
        ASSERT(!m_centerY);
        return buildEllipseString(radiusX, radiusY, nullString(), nullString());
    }

    auto x = m_centerX->cssText();
    auto y = m_centerY->cssText();
    return buildEllipseString(radiusX, radiusY, x, y);
}

bool CSSEllipseValue::equals(const CSSEllipseValue& other) const
{
    return compareCSSValuePtr(m_centerX, other.m_centerX)
        && compareCSSValuePtr(m_centerY, other.m_centerY)
        && compareCSSValuePtr(m_radiusX, other.m_radiusX)
        && compareCSSValuePtr(m_radiusY, other.m_radiusY);
}

// MARK: -

CSSXywhValue::CSSXywhValue(Ref<CSSValue>&& insetX, Ref<CSSValue>&& insetY, Ref<CSSValue>&& width, Ref<CSSValue>&& height, RefPtr<CSSValue>&& topLeftRadius, RefPtr<CSSValue>&& topRightRadius, RefPtr<CSSValue>&& bottomRightRadius, RefPtr<CSSValue>&& bottomLeftRadius)
    : CSSValue(ClassType::XywhShape)
    , m_insetX(WTFMove(insetX))
    , m_insetY(WTFMove(insetY))
    , m_width(WTFMove(width))
    , m_height(WTFMove(height))
    , m_topLeftRadius(WTFMove(topLeftRadius))
    , m_topRightRadius(WTFMove(topRightRadius))
    , m_bottomRightRadius(WTFMove(bottomRightRadius))
    , m_bottomLeftRadius(WTFMove(bottomLeftRadius))
{
}

Ref<CSSXywhValue> CSSXywhValue::create(Ref<CSSValue>&& insetX, Ref<CSSValue>&& insetY, Ref<CSSValue>&& width, Ref<CSSValue>&& height, RefPtr<CSSValue>&& topLeftRadius, RefPtr<CSSValue>&& topRightRadius, RefPtr<CSSValue>&& bottomRightRadius, RefPtr<CSSValue>&& bottomLeftRadius)
{
    return adoptRef(*new CSSXywhValue(WTFMove(insetX), WTFMove(insetY), WTFMove(width), WTFMove(height), WTFMove(topLeftRadius), WTFMove(topRightRadius), WTFMove(bottomRightRadius), WTFMove(bottomLeftRadius)));
}

bool CSSXywhValue::equals(const CSSXywhValue& other) const
{
    return compareCSSValue(m_insetX, other.m_insetX)
        && compareCSSValue(m_insetY, other.m_insetY)
        && compareCSSValue(m_width, other.m_width)
        && compareCSSValue(m_height, other.m_height)
        && compareCSSValuePtr(m_topLeftRadius, other.m_topLeftRadius)
        && compareCSSValuePtr(m_topRightRadius, other.m_topRightRadius)
        && compareCSSValuePtr(m_bottomRightRadius, other.m_bottomRightRadius)
        && compareCSSValuePtr(m_bottomLeftRadius, other.m_bottomLeftRadius);
}

static inline void updateCornerRadiusWidthAndHeight(const CSSValue* corner, String& width, String& height)
{
    if (!corner)
        return;
    width = corner->first().cssText();
    height = corner->second().cssText();
}

static bool buildRadii(Vector<String>& radii, const String& topLeftRadius, const String& topRightRadius, const String& bottomRightRadius, const String& bottomLeftRadius)
{
    bool showBottomLeft = topRightRadius != bottomLeftRadius;
    bool showBottomRight = showBottomLeft || (bottomRightRadius != topLeftRadius);
    bool showTopRight = showBottomRight || (topRightRadius != topLeftRadius);

    radii.append(topLeftRadius);
    if (showTopRight)
        radii.append(topRightRadius);
    if (showBottomRight)
        radii.append(bottomRightRadius);
    if (showBottomLeft)
        radii.append(bottomLeftRadius);

    return radii.size() == 1 && radii[0] == "0px"_s;
}

static void buildRadiiString(StringBuilder& result, const String& topLeftRadiusWidth, const String& topLeftRadiusHeight,
    const String& topRightRadiusWidth, const String& topRightRadiusHeight,
    const String& bottomRightRadiusWidth, const String& bottomRightRadiusHeight,
    const String& bottomLeftRadiusWidth, const String& bottomLeftRadiusHeight)
{
    if (!topLeftRadiusWidth.isNull() && !topLeftRadiusHeight.isNull()) {
        Vector<String> horizontalRadii;
        bool areDefaultCornerRadii = buildRadii(horizontalRadii, topLeftRadiusWidth, topRightRadiusWidth, bottomRightRadiusWidth, bottomLeftRadiusWidth);

        Vector<String> verticalRadii;
        areDefaultCornerRadii &= buildRadii(verticalRadii, topLeftRadiusHeight, topRightRadiusHeight, bottomRightRadiusHeight, bottomLeftRadiusHeight);

        if (!areDefaultCornerRadii) {
            result.append(" round"_s);

            for (auto& radius : horizontalRadii)
                result.append(' ', radius);

            if (verticalRadii != horizontalRadii) {
                result.append(" /"_s);
                for (auto& radius : verticalRadii)
                    result.append(' ', radius);
            }
        }
    }
}

static String buildXywhString(const String& insetX, const String& insetY, const String& width, const String& height,
    const String& topLeftRadiusWidth, const String& topLeftRadiusHeight,
    const String& topRightRadiusWidth, const String& topRightRadiusHeight,
    const String& bottomRightRadiusWidth, const String& bottomRightRadiusHeight,
    const String& bottomLeftRadiusWidth, const String& bottomLeftRadiusHeight)
{
    StringBuilder result;
    result.append("xywh("_s, insetX, ' ', insetY, ' ', width, ' ', height);

    buildRadiiString(result, topLeftRadiusWidth, topLeftRadiusHeight, topRightRadiusWidth, topRightRadiusHeight, bottomRightRadiusWidth, bottomRightRadiusHeight, bottomLeftRadiusWidth, bottomLeftRadiusHeight);

    result.append(')');
    return result.toString();
}

String CSSXywhValue::customCSSText() const
{
    String topLeftRadiusWidth;
    String topLeftRadiusHeight;
    String topRightRadiusWidth;
    String topRightRadiusHeight;
    String bottomRightRadiusWidth;
    String bottomRightRadiusHeight;
    String bottomLeftRadiusWidth;
    String bottomLeftRadiusHeight;

    updateCornerRadiusWidthAndHeight(topLeftRadius(), topLeftRadiusWidth, topLeftRadiusHeight);
    updateCornerRadiusWidthAndHeight(topRightRadius(), topRightRadiusWidth, topRightRadiusHeight);
    updateCornerRadiusWidthAndHeight(bottomRightRadius(), bottomRightRadiusWidth, bottomRightRadiusHeight);
    updateCornerRadiusWidthAndHeight(bottomLeftRadius(), bottomLeftRadiusWidth, bottomLeftRadiusHeight);

    return buildXywhString(m_insetX->cssText(), m_insetY->cssText(), m_width->cssText(), m_height->cssText(),
        topLeftRadiusWidth, topLeftRadiusHeight, topRightRadiusWidth, topRightRadiusHeight,
        bottomRightRadiusWidth, bottomRightRadiusHeight, bottomLeftRadiusWidth, bottomLeftRadiusHeight);
}

// MARK: -

CSSRectShapeValue::CSSRectShapeValue(Ref<CSSValue>&& top, Ref<CSSValue>&& right, Ref<CSSValue>&& bottom, Ref<CSSValue>&& left, RefPtr<CSSValue>&& topLeftRadius, RefPtr<CSSValue>&& topRightRadius, RefPtr<CSSValue>&& bottomRightRadius, RefPtr<CSSValue>&& bottomLeftRadius)
    : CSSValue(ClassType::RectShape)
    , m_top(WTFMove(top))
    , m_right(WTFMove(right))
    , m_bottom(WTFMove(bottom))
    , m_left(WTFMove(left))
    , m_topLeftRadius(WTFMove(topLeftRadius))
    , m_topRightRadius(WTFMove(topRightRadius))
    , m_bottomRightRadius(WTFMove(bottomRightRadius))
    , m_bottomLeftRadius(WTFMove(bottomLeftRadius))
{
}

Ref<CSSRectShapeValue> CSSRectShapeValue::create(Ref<CSSValue>&& top, Ref<CSSValue>&& right, Ref<CSSValue>&& bottom, Ref<CSSValue>&& left, RefPtr<CSSValue>&& topLeftRadius, RefPtr<CSSValue>&& topRightRadius, RefPtr<CSSValue>&& bottomRightRadius, RefPtr<CSSValue>&& bottomLeftRadius)
{
    return adoptRef(*new CSSRectShapeValue(WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left), WTFMove(topLeftRadius), WTFMove(topRightRadius), WTFMove(bottomRightRadius), WTFMove(bottomLeftRadius)));
}

bool CSSRectShapeValue::equals(const CSSRectShapeValue& other) const
{
    return compareCSSValue(m_top, other.m_top)
        && compareCSSValue(m_right, other.m_right)
        && compareCSSValue(m_bottom, other.m_bottom)
        && compareCSSValue(m_left, other.m_left)
        && compareCSSValuePtr(m_topLeftRadius, other.m_topLeftRadius)
        && compareCSSValuePtr(m_topRightRadius, other.m_topRightRadius)
        && compareCSSValuePtr(m_bottomRightRadius, other.m_bottomRightRadius)
        && compareCSSValuePtr(m_bottomLeftRadius, other.m_bottomLeftRadius);
}

static String buildRectString(const String& top, const String& right, const String& bottom, const String& left,
    const String& topLeftRadiusWidth, const String& topLeftRadiusHeight,
    const String& topRightRadiusWidth, const String& topRightRadiusHeight,
    const String& bottomRightRadiusWidth, const String& bottomRightRadiusHeight,
    const String& bottomLeftRadiusWidth, const String& bottomLeftRadiusHeight)
{
    StringBuilder result;
    result.append("rect("_s, top, ' ', right, ' ', bottom, ' ', left);

    buildRadiiString(result, topLeftRadiusWidth, topLeftRadiusHeight, topRightRadiusWidth, topRightRadiusHeight, bottomRightRadiusWidth, bottomRightRadiusHeight, bottomLeftRadiusWidth, bottomLeftRadiusHeight);

    result.append(')');
    return result.toString();
}

String CSSRectShapeValue::customCSSText() const
{
    String topLeftRadiusWidth;
    String topLeftRadiusHeight;
    String topRightRadiusWidth;
    String topRightRadiusHeight;
    String bottomRightRadiusWidth;
    String bottomRightRadiusHeight;
    String bottomLeftRadiusWidth;
    String bottomLeftRadiusHeight;

    updateCornerRadiusWidthAndHeight(topLeftRadius(), topLeftRadiusWidth, topLeftRadiusHeight);
    updateCornerRadiusWidthAndHeight(topRightRadius(), topRightRadiusWidth, topRightRadiusHeight);
    updateCornerRadiusWidthAndHeight(bottomRightRadius(), bottomRightRadiusWidth, bottomRightRadiusHeight);
    updateCornerRadiusWidthAndHeight(bottomLeftRadius(), bottomLeftRadiusWidth, bottomLeftRadiusHeight);

    return buildRectString(m_top->cssText(), m_right->cssText(), m_bottom->cssText(), m_left->cssText(),
        topLeftRadiusWidth, topLeftRadiusHeight, topRightRadiusWidth, topRightRadiusHeight,
        bottomRightRadiusWidth, bottomRightRadiusHeight, bottomLeftRadiusWidth, bottomLeftRadiusHeight);
}

// MARK: -

CSSPathValue::CSSPathValue(SVGPathByteStream data, WindRule rule)
    : CSSValue(ClassType::Path)
    , m_pathData(WTFMove(data))
    , m_windRule(rule)
{
}

Ref<CSSPathValue> CSSPathValue::create(SVGPathByteStream data, WindRule rule)
{
    return adoptRef(*new CSSPathValue(WTFMove(data), rule));
}

String CSSPathValue::customCSSText() const
{
    String pathString;
    buildStringFromByteStream(m_pathData, pathString, UnalteredParsing);
    StringBuilder result;
    if (m_windRule == WindRule::EvenOdd)
        result.append("path(evenodd, "_s);
    else
        result.append("path("_s);
    serializeString(pathString, result);
    result.append(')');
    return result.toString();
}

bool CSSPathValue::equals(const CSSPathValue& other) const
{
    return m_pathData == other.m_pathData && m_windRule == other.m_windRule;
}

// MARK: -

CSSPolygonValue::CSSPolygonValue(CSSValueListBuilder&& values, WindRule rule)
    : CSSValueContainingVector(ClassType::Polygon, SpaceSeparator, WTFMove(values))
    , m_windRule(rule)
{
}

Ref<CSSPolygonValue> CSSPolygonValue::create(CSSValueListBuilder&& values, WindRule rule)
{
    return adoptRef(*new CSSPolygonValue(WTFMove(values), rule));
}

String CSSPolygonValue::customCSSText() const
{
    ASSERT(!(length() % 2));
    StringBuilder result;
    if (m_windRule == WindRule::EvenOdd)
        result.append("polygon(evenodd, "_s);
    else
        result.append("polygon("_s);
    for (size_t i = 0; i < length(); i += 2)
        result.append(i ? ", "_s : ""_s, item(i)->cssText(), ' ', item(i + 1)->cssText());
    result.append(')');
    return result.toString();
}

bool CSSPolygonValue::equals(const CSSPolygonValue& other) const
{
    return m_windRule == other.m_windRule && itemsEqual(other);
}

// MARK: -

CSSInsetShapeValue::CSSInsetShapeValue(Ref<CSSValue>&& top, Ref<CSSValue>&& right, Ref<CSSValue>&& bottom, Ref<CSSValue>&& left, RefPtr<CSSValue>&& topLeftRadius, RefPtr<CSSValue>&& topRightRadius, RefPtr<CSSValue>&& bottomRightRadius, RefPtr<CSSValue>&& bottomLeftRadius)
    : CSSValue(ClassType::InsetShape)
    , m_top(WTFMove(top))
    , m_right(WTFMove(right))
    , m_bottom(WTFMove(bottom))
    , m_left(WTFMove(left))
    , m_topLeftRadius(WTFMove(topLeftRadius))
    , m_topRightRadius(WTFMove(topRightRadius))
    , m_bottomRightRadius(WTFMove(bottomRightRadius))
    , m_bottomLeftRadius(WTFMove(bottomLeftRadius))
{
}

Ref<CSSInsetShapeValue> CSSInsetShapeValue::create(Ref<CSSValue>&& top, Ref<CSSValue>&& right, Ref<CSSValue>&& bottom, Ref<CSSValue>&& left,
    RefPtr<CSSValue>&& topLeftRadius, RefPtr<CSSValue>&& topRightRadius, RefPtr<CSSValue>&& bottomRightRadius, RefPtr<CSSValue>&& bottomLeftRadius)
{
    return adoptRef(*new CSSInsetShapeValue(WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left),
        WTFMove(topLeftRadius), WTFMove(topRightRadius), WTFMove(bottomRightRadius), WTFMove(bottomLeftRadius)));
}

static String buildInsetString(const String& top, const String& right, const String& bottom, const String& left,
    const String& topLeftRadiusWidth, const String& topLeftRadiusHeight,
    const String& topRightRadiusWidth, const String& topRightRadiusHeight,
    const String& bottomRightRadiusWidth, const String& bottomRightRadiusHeight,
    const String& bottomLeftRadiusWidth, const String& bottomLeftRadiusHeight)
{
    StringBuilder result;
    result.append("inset("_s, top);

    bool showLeftArg = !left.isNull() && left != right;
    bool showBottomArg = !bottom.isNull() && (bottom != top || showLeftArg);
    bool showRightArg = !right.isNull() && (right != top || showBottomArg);
    if (showRightArg)
        result.append(' ', right);
    if (showBottomArg)
        result.append(' ', bottom);
    if (showLeftArg)
        result.append(' ', left);

    buildRadiiString(result, topLeftRadiusWidth, topLeftRadiusHeight, topRightRadiusWidth, topRightRadiusHeight, bottomRightRadiusWidth, bottomRightRadiusHeight, bottomLeftRadiusWidth, bottomLeftRadiusHeight);

    result.append(')');
    return result.toString();
}

String CSSInsetShapeValue::customCSSText() const
{
    String topLeftRadiusWidth;
    String topLeftRadiusHeight;
    String topRightRadiusWidth;
    String topRightRadiusHeight;
    String bottomRightRadiusWidth;
    String bottomRightRadiusHeight;
    String bottomLeftRadiusWidth;
    String bottomLeftRadiusHeight;

    updateCornerRadiusWidthAndHeight(topLeftRadius(), topLeftRadiusWidth, topLeftRadiusHeight);
    updateCornerRadiusWidthAndHeight(topRightRadius(), topRightRadiusWidth, topRightRadiusHeight);
    updateCornerRadiusWidthAndHeight(bottomRightRadius(), bottomRightRadiusWidth, bottomRightRadiusHeight);
    updateCornerRadiusWidthAndHeight(bottomLeftRadius(), bottomLeftRadiusWidth, bottomLeftRadiusHeight);

    return buildInsetString(m_top->cssText(), m_right->cssText(), m_bottom->cssText(), m_left->cssText(),
        topLeftRadiusWidth, topLeftRadiusHeight, topRightRadiusWidth, topRightRadiusHeight,
        bottomRightRadiusWidth, bottomRightRadiusHeight, bottomLeftRadiusWidth, bottomLeftRadiusHeight);
}

bool CSSInsetShapeValue::equals(const CSSInsetShapeValue& other) const
{
    return compareCSSValue(m_top, other.m_top)
        && compareCSSValue(m_right, other.m_right)
        && compareCSSValue(m_bottom, other.m_bottom)
        && compareCSSValue(m_left, other.m_left)
        && compareCSSValuePtr(m_topLeftRadius, other.m_topLeftRadius)
        && compareCSSValuePtr(m_topRightRadius, other.m_topRightRadius)
        && compareCSSValuePtr(m_bottomRightRadius, other.m_bottomRightRadius)
        && compareCSSValuePtr(m_bottomLeftRadius, other.m_bottomLeftRadius);
}

// MARK: -

Ref<CSSShapeValue> CSSShapeValue::create(WindRule windRule, Ref<CSSValue>&& fromCoordinates, CSSValueListBuilder&& shapeSegments)
{
    return adoptRef(*new CSSShapeValue(windRule, WTFMove(fromCoordinates), WTFMove(shapeSegments)));
}

CSSShapeValue::CSSShapeValue(WindRule windRule, Ref<CSSValue>&& fromCoordinates, CSSValueListBuilder&& shapeSegments)
    : CSSValueContainingVector(ClassType::Shape, CommaSeparator, WTFMove(shapeSegments))
    , m_fromCoordinates(WTFMove(fromCoordinates))
    , m_windRule(windRule)
{
}

String CSSShapeValue::customCSSText() const
{
    StringBuilder builder;
    builder.append("shape("_s);

    if (windRule() == WindRule::EvenOdd)
        builder.append("evenodd "_s);

    builder.append("from "_s, m_fromCoordinates->cssText(), ", "_s);
    serializeItems(builder);

    builder.append(')');
    return builder.toString();
}

bool CSSShapeValue::equals(const CSSShapeValue& other) const
{
    if (windRule() != other.windRule())
        return false;

    if (!compareCSSValue(m_fromCoordinates, other.m_fromCoordinates))
        return false;

    return itemsEqual(other);
}

} // namespace WebCore
