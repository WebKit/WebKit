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
    : CSSValue(CircleClass)
    , m_radius(WTFMove(radius))
    , m_centerX(WTFMove(centerX))
    , m_centerY(WTFMove(centerY))
{
}

Ref<CSSCircleValue> CSSCircleValue::create(RefPtr<CSSValue>&& radius, RefPtr<CSSValue>&& centerX, RefPtr<CSSValue>&& centerY)
{
    return adoptRef(*new CSSCircleValue(WTFMove(radius), WTFMove(centerX), WTFMove(centerY)));
}

struct SerializablePositionOffset {
    CSSValueID side;
    Ref<const CSSValue> amount;
};

static void serializePositionOffset(StringBuilder& builder, const SerializablePositionOffset& offset, const SerializablePositionOffset& other)
{
    if ((offset.side == CSSValueLeft && other.side == CSSValueTop) || (offset.side == CSSValueTop && other.side == CSSValueLeft)) {
        offset.amount->cssText(builder);
        return;
    }
    builder.append(nameLiteral(offset.side), ' ');
    offset.amount->cssText(builder);
}

static bool isZeroLength(const CSSValue& value)
{
    auto* primitive = dynamicDowncast<CSSPrimitiveValue>(value);
    return primitive && primitive->isLength() && primitive->isZero().value_or(false);
}

static SerializablePositionOffset buildSerializablePositionOffset(CSSValue* offset, CSSValueID defaultSide)
{
    CSSValueID side = defaultSide;
    RefPtr<const CSSValue> amount;

    if (!offset)
        side = CSSValueCenter;
    else if (offset->isValueID())
        side = offset->valueID();
    else if (offset->isPair()) {
        side = offset->first().valueID();
        amount = &offset->second();
    } else
        amount = offset;

    if (!amount)
        amount = CSSPrimitiveValue::create(Length(side == CSSValueCenter ? 50 : 0, LengthType::Percent));
    
    if (side == CSSValueCenter)
        side = defaultSide;
    else if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(*amount); (side == CSSValueRight || side == CSSValueBottom) && primitiveValue && primitiveValue->isPercentage()) {
        side = defaultSide;
        amount = CSSPrimitiveValue::create(Length(100 - primitiveValue->floatValue(), LengthType::Percent));
    } else if (isZeroLength(*amount)) {
        if (side == CSSValueRight || side == CSSValueBottom)
            amount = CSSPrimitiveValue::create(Length(100, LengthType::Percent));
        else
            amount = CSSPrimitiveValue::create(Length(0, LengthType::Percent));
        side = defaultSide;
    }

    return { side, amount.releaseNonNull() };
}

void CSSCircleValue::customCSSText(StringBuilder& builder) const
{
    builder.append("circle("_s);

    bool needsSeparator = false;

    if (m_radius && m_radius->valueID() != CSSValueClosestSide) {
        m_radius->cssText(builder);
        needsSeparator = true;
    }

    if (m_centerX) {
        ASSERT(m_centerY);

        if (needsSeparator)
            builder.append(" at "_s);
        else
            builder.append("at "_s);

        auto x = buildSerializablePositionOffset(m_centerX.get(), CSSValueLeft);
        auto y = buildSerializablePositionOffset(m_centerY.get(), CSSValueTop);

        serializePositionOffset(builder, x, y);
        builder.append(' ');
        serializePositionOffset(builder, y, x);
    }

    builder.append(')');
}

bool CSSCircleValue::equals(const CSSCircleValue& other) const
{
    return compareCSSValuePtr(m_centerX, other.m_centerX)
        && compareCSSValuePtr(m_centerY, other.m_centerY)
        && compareCSSValuePtr(m_radius, other.m_radius);
}

CSSEllipseValue::CSSEllipseValue(RefPtr<CSSValue>&& radiusX, RefPtr<CSSValue>&& radiusY, RefPtr<CSSValue>&& centerX, RefPtr<CSSValue>&& centerY)
    : CSSValue(EllipseClass)
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

void CSSEllipseValue::customCSSText(StringBuilder& builder) const
{
    builder.append("ellipse("_s);

    bool needsSeparator = false;

    if (m_radiusX) {
        ASSERT(m_radiusY);
        bool radiusXClosestSide = m_radiusX->valueID() == CSSValueClosestSide;
        bool radiusYClosestSide = m_radiusY->valueID() == CSSValueClosestSide;
        if (!radiusXClosestSide || !radiusYClosestSide) {
            m_radiusX->cssText(builder);
            builder.append(' ');
            m_radiusY->cssText(builder);
            needsSeparator = true;
        }
    }

    if (m_centerX) {
        ASSERT(m_centerY);

        auto x = buildSerializablePositionOffset(m_centerX.get(), CSSValueLeft);
        auto y = buildSerializablePositionOffset(m_centerY.get(), CSSValueTop);

        if (needsSeparator)
            builder.append(" at "_s);
        else
            builder.append("at "_s);
        serializePositionOffset(builder, x, y);
        builder.append(' ');
        serializePositionOffset(builder, y, x);
    }

    builder.append(')');
}

bool CSSEllipseValue::equals(const CSSEllipseValue& other) const
{
    return compareCSSValuePtr(m_centerX, other.m_centerX)
        && compareCSSValuePtr(m_centerY, other.m_centerY)
        && compareCSSValuePtr(m_radiusX, other.m_radiusX)
        && compareCSSValuePtr(m_radiusY, other.m_radiusY);
}

CSSXywhValue::CSSXywhValue(Ref<CSSValue>&& insetX, Ref<CSSValue>&& insetY, Ref<CSSValue>&& width, Ref<CSSValue>&& height, RefPtr<CSSValue>&& topLeftRadius, RefPtr<CSSValue>&& topRightRadius, RefPtr<CSSValue>&& bottomRightRadius, RefPtr<CSSValue>&& bottomLeftRadius)
    : CSSValue(XywhShapeClass)
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

static std::optional<std::pair<Ref<const CSSValue>, Ref<const CSSValue>>> extractCornerRadiusWidthAndHeight(const CSSValue* corner)
{
    if (!corner)
        return std::nullopt;
    return { { Ref { corner->first() }, Ref { corner->second() } } };
}

static bool isZeroPX(const CSSValue& value)
{
    if (auto primitive = dynamicDowncast<CSSPrimitiveValue>(value))
        return primitive->isPx() && *primitive->isZero();
    return false;
}

static void serializeRadii(StringBuilder& builder, std::array<const CSSValue*, 4> corners)
{
    RefPtr<const CSSValue> horizontalRadii[4];
    RefPtr<const CSSValue> verticalRadii[4];

    unsigned numberOfCorners = 4;
    for (unsigned i = 0; i < 4; ++i) {
        auto value = extractCornerRadiusWidthAndHeight(corners[i]);
        if (!value) {
            numberOfCorners = i;
            break;
        }

        horizontalRadii[i] = value->first.ptr();
        verticalRadii[i] = value->second.ptr();
    }

    if (!numberOfCorners)
        return;

    ASSERT(numberOfCorners == 4);

    auto areDefaultCornerRadii = [&](const auto (&r)[4]) {
        if (!r[3]->equals(*r[1]))
            return false;
        else if (!r[2]->equals(*r[0]))
            return false;
        else if (!r[1]->equals(*r[0]))
            return false;
        else
            return isZeroPX(*r[0]);
    };

    if (areDefaultCornerRadii(horizontalRadii) && areDefaultCornerRadii(verticalRadii))
        return;

    builder.append(" round "_s);

    // Beyond this point, the logic in this function is identical to that
    // of ShorthandSerializer::serializeBorderRadius(). It would be good to
    // find a way to share.

    bool serializeBoth = false;
    for (unsigned i = 0; i < 4; ++i) {
        if (!horizontalRadii[i]->equals(*verticalRadii[i])) {
            serializeBoth = true;
            break;
        }
    }

    auto serializeRadiiAxis = [&](const auto (&r)[4]) {
        if (!r[3]->equals(*r[1])) {
            r[0]->cssText(builder);
            builder.append(' ');
            r[1]->cssText(builder);
            builder.append(' ');
            r[2]->cssText(builder);
            builder.append(' ');
            r[3]->cssText(builder);
        } else if (!r[2]->equals(*r[0])) {
            r[0]->cssText(builder);
            builder.append(' ');
            r[1]->cssText(builder);
            builder.append(' ');
            r[2]->cssText(builder);
        } else if (!r[1]->equals(*r[0])) {
            r[0]->cssText(builder);
            builder.append(' ');
            r[1]->cssText(builder);
        } else
            r[0]->cssText(builder);
    };

    serializeRadiiAxis(horizontalRadii);
    if (serializeBoth) {
        builder.append(" / "_s);
        serializeRadiiAxis(verticalRadii);
    }
}

void CSSXywhValue::customCSSText(StringBuilder& builder) const
{
    builder.append("xywh("_s);

    m_insetX->cssText(builder);
    builder.append(' ');
    m_insetY->cssText(builder);
    builder.append(' ');
    m_width->cssText(builder);
    builder.append(' ');
    m_height->cssText(builder);

    serializeRadii(builder, { topLeftRadius(), topRightRadius(), bottomRightRadius(), bottomLeftRadius() });

    builder.append(')');
}

CSSRectShapeValue::CSSRectShapeValue(Ref<CSSValue>&& top, Ref<CSSValue>&& right, Ref<CSSValue>&& bottom, Ref<CSSValue>&& left, RefPtr<CSSValue>&& topLeftRadius, RefPtr<CSSValue>&& topRightRadius, RefPtr<CSSValue>&& bottomRightRadius, RefPtr<CSSValue>&& bottomLeftRadius)
    : CSSValue(RectShapeClass)
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

void CSSRectShapeValue::customCSSText(StringBuilder& builder) const
{
    builder.append("rect("_s);

    m_top->cssText(builder);
    builder.append(' ');
    m_right->cssText(builder);
    builder.append(' ');
    m_bottom->cssText(builder);
    builder.append(' ');
    m_left->cssText(builder);

    serializeRadii(builder, { topLeftRadius(), topRightRadius(), bottomRightRadius(), bottomLeftRadius() });

    builder.append(')');
}

CSSPathValue::CSSPathValue(SVGPathByteStream data, WindRule rule)
    : CSSValue(PathClass)
    , m_pathData(WTFMove(data))
    , m_windRule(rule)
{
}

Ref<CSSPathValue> CSSPathValue::create(SVGPathByteStream data, WindRule rule)
{
    return adoptRef(*new CSSPathValue(WTFMove(data), rule));
}

void CSSPathValue::customCSSText(StringBuilder& builder) const
{
    if (m_windRule == WindRule::EvenOdd)
        builder.append("path(evenodd, \""_s);
    else
        builder.append("path(\""_s);

    buildStringFromByteStream(m_pathData, builder, UnalteredParsing);

    builder.append("\")"_s);
}

bool CSSPathValue::equals(const CSSPathValue& other) const
{
    return m_pathData == other.m_pathData && m_windRule == other.m_windRule;
}

CSSPolygonValue::CSSPolygonValue(CSSValueListBuilder values, WindRule rule)
    : CSSValueContainingVector(PolygonClass, SpaceSeparator, WTFMove(values))
    , m_windRule(rule)
{
}

Ref<CSSPolygonValue> CSSPolygonValue::create(CSSValueListBuilder values, WindRule rule)
{
    return adoptRef(*new CSSPolygonValue(WTFMove(values), rule));
}

void CSSPolygonValue::customCSSText(StringBuilder& builder) const
{
    ASSERT(!(length() % 2));

    if (m_windRule == WindRule::EvenOdd)
        builder.append("polygon(evenodd, "_s);
    else
        builder.append("polygon("_s);

    for (size_t i = 0; i < length(); i += 2) {
        builder.append(i ? ", "_s : ""_s);
        item(i)->cssText(builder);
        builder.append(' ');
        item(i + 1)->cssText(builder);
    }

    builder.append(')');
}

bool CSSPolygonValue::equals(const CSSPolygonValue& other) const
{
    return m_windRule == other.m_windRule && itemsEqual(other);
}

CSSInsetShapeValue::CSSInsetShapeValue(Ref<CSSValue>&& top, Ref<CSSValue>&& right, Ref<CSSValue>&& bottom, Ref<CSSValue>&& left, RefPtr<CSSValue>&& topLeftRadius, RefPtr<CSSValue>&& topRightRadius, RefPtr<CSSValue>&& bottomRightRadius, RefPtr<CSSValue>&& bottomLeftRadius)
    : CSSValue(InsetShapeClass)
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

void CSSInsetShapeValue::customCSSText(StringBuilder& builder) const
{
    bool showLeftArg = !compareCSSValue(m_left, m_right);
    bool showBottomArg = (!compareCSSValue(m_bottom, m_top) || showLeftArg);
    bool showRightArg = (!compareCSSValue(m_right, m_top) || showBottomArg);

    builder.append("inset("_s);
    m_top->cssText(builder);

    if (showRightArg) {
        builder.append(' ');
        m_right->cssText(builder);
    }
    if (showBottomArg) {
        builder.append(' ');
        m_bottom->cssText(builder);
    }
    if (showLeftArg) {
        builder.append(' ');
        m_left->cssText(builder);
    }

    serializeRadii(builder, { topLeftRadius(), topRightRadius(), bottomRightRadius(), bottomLeftRadius() });

    builder.append(')');
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

} // namespace WebCore
