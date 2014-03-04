/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
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

#include "BasicShapes.h"

#include "BasicShapeFunctions.h"
#include "CalculationValue.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "LengthFunctions.h"
#include "Path.h"
#include "RenderBox.h"

namespace WebCore {

void BasicShapeCenterCoordinate::updateComputedLength()
{
    if (m_direction == TopLeft) {
        m_computedLength = m_length.isUndefined() ? Length(0, Fixed) : m_length;
        return;
    }
    if (m_length.isUndefined()) {
        m_computedLength = Length(100, Percent);
        return;
    }

    auto lhs = std::make_unique<CalcExpressionLength>(Length(100, Percent));
    auto rhs = std::make_unique<CalcExpressionLength>(m_length);
    auto op = std::make_unique<CalcExpressionBinaryOperation>(std::move(lhs), std::move(rhs), CalcSubtract);
    m_computedLength = Length(CalculationValue::create(std::move(op), CalculationRangeAll));
}

bool BasicShape::canBlend(const BasicShape* other) const
{
    // FIXME: Support animations between different shapes in the future.
    if (type() != other->type())
        return false;

    // Both shapes must use the same reference box.
    if (layoutBox() != other->layoutBox())
        return false;

    // Just polygons with same number of vertices can be animated.
    if (type() == BasicShape::BasicShapePolygonType
        && (static_cast<const BasicShapePolygon*>(this)->values().size() != static_cast<const BasicShapePolygon*>(other)->values().size()
        || static_cast<const BasicShapePolygon*>(this)->windRule() != static_cast<const BasicShapePolygon*>(other)->windRule()))
        return false;

    // Circles with keywords for radii coordinates cannot be animated.
    if (type() == BasicShape::BasicShapeCircleType) {
        const BasicShapeCircle* thisCircle = static_cast<const BasicShapeCircle*>(this);
        const BasicShapeCircle* otherCircle = static_cast<const BasicShapeCircle*>(other);
        if (!thisCircle->radius().canBlend(otherCircle->radius()))
            return false;
    }

    // Ellipses with keywords for radii coordinates cannot be animated.
    if (type() != BasicShape::BasicShapeEllipseType)
        return true;

    const BasicShapeEllipse* thisEllipse = static_cast<const BasicShapeEllipse*>(this);
    const BasicShapeEllipse* otherEllipse = static_cast<const BasicShapeEllipse*>(other);
    return (thisEllipse->radiusX().canBlend(otherEllipse->radiusX())
        && thisEllipse->radiusY().canBlend(otherEllipse->radiusY()));
}

void BasicShapeRectangle::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());
    path.addRoundedRect(
        FloatRect(
            floatValueForLength(m_x, boundingBox.width()) + boundingBox.x(),
            floatValueForLength(m_y, boundingBox.height()) + boundingBox.y(),
            floatValueForLength(m_width, boundingBox.width()),
            floatValueForLength(m_height, boundingBox.height())
        ),
        FloatSize(
            floatValueForLength(m_cornerRadiusX, boundingBox.width()),
            floatValueForLength(m_cornerRadiusY, boundingBox.height())
        )
    );
}

PassRefPtr<BasicShape> BasicShapeRectangle::blend(const BasicShape* other, double progress) const
{
    ASSERT(type() == other->type());

    const BasicShapeRectangle* o = static_cast<const BasicShapeRectangle*>(other);
    RefPtr<BasicShapeRectangle> result =  BasicShapeRectangle::create();
    result->setX(m_x.blend(o->x(), progress));
    result->setY(m_y.blend(o->y(), progress));
    result->setWidth(m_width.blend(o->width(), progress));
    result->setHeight(m_height.blend(o->height(), progress));
    result->setCornerRadiusX(m_cornerRadiusX.blend(o->cornerRadiusX(), progress));
    result->setCornerRadiusY(m_cornerRadiusY.blend(o->cornerRadiusY(), progress));
    return result.release();
}

void DeprecatedBasicShapeCircle::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());
    float diagonal = sqrtf((boundingBox.width() * boundingBox.width() + boundingBox.height() * boundingBox.height()) / 2);
    float centerX = floatValueForLength(m_centerX, boundingBox.width());
    float centerY = floatValueForLength(m_centerY, boundingBox.height());
    float radius = floatValueForLength(m_radius, diagonal);
    path.addEllipse(FloatRect(
        centerX - radius + boundingBox.x(),
        centerY - radius + boundingBox.y(),
        radius * 2,
        radius * 2
    ));
}

PassRefPtr<BasicShape> DeprecatedBasicShapeCircle::blend(const BasicShape* other, double progress) const
{
    ASSERT(type() == other->type());

    const DeprecatedBasicShapeCircle* o = static_cast<const DeprecatedBasicShapeCircle*>(other);
    RefPtr<DeprecatedBasicShapeCircle> result =  DeprecatedBasicShapeCircle::create();
    result->setCenterX(m_centerX.blend(o->centerX(), progress));
    result->setCenterY(m_centerY.blend(o->centerY(), progress));
    result->setRadius(m_radius.blend(o->radius(), progress));
    return result.release();
}

float BasicShapeCircle::floatValueForRadiusInBox(float boxWidth, float boxHeight) const
{
    if (m_radius.type() == BasicShapeRadius::Value)
        return floatValueForLength(m_radius.value(), sqrtf((boxWidth * boxWidth + boxHeight * boxHeight) / 2));

    float centerX = floatValueForCenterCoordinate(m_centerX, boxWidth);
    float centerY = floatValueForCenterCoordinate(m_centerY, boxHeight);

    if (m_radius.type() == BasicShapeRadius::ClosestSide)
        return std::min(std::min(centerX, boxWidth - centerX), std::min(centerY, boxHeight - centerY));

    // If radius.type() == BasicShapeRadius::FarthestSide.
    return std::max(std::max(centerX, boxWidth - centerX), std::max(centerY, boxHeight - centerY));
}

void BasicShapeCircle::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());

    float centerX = floatValueForCenterCoordinate(m_centerX, boundingBox.width());
    float centerY = floatValueForCenterCoordinate(m_centerY, boundingBox.height());
    float radius = floatValueForRadiusInBox(boundingBox.width(), boundingBox.height());
    path.addEllipse(FloatRect(
        centerX - radius + boundingBox.x(),
        centerY - radius + boundingBox.y(),
        radius * 2,
        radius * 2
    ));
}

PassRefPtr<BasicShape> BasicShapeCircle::blend(const BasicShape* other, double progress) const
{
    ASSERT(type() == other->type());
    const BasicShapeCircle* o = static_cast<const BasicShapeCircle*>(other);
    RefPtr<BasicShapeCircle> result =  BasicShapeCircle::create();

    result->setCenterX(m_centerX.blend(o->centerX(), progress));
    result->setCenterY(m_centerY.blend(o->centerY(), progress));
    result->setRadius(m_radius.blend(o->radius(), progress));
    return result.release();
}

void DeprecatedBasicShapeEllipse::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());
    float centerX = floatValueForLength(m_centerX, boundingBox.width());
    float centerY = floatValueForLength(m_centerY, boundingBox.height());
    float radiusX = floatValueForLength(m_radiusX, boundingBox.width());
    float radiusY = floatValueForLength(m_radiusY, boundingBox.height());
    path.addEllipse(FloatRect(
        centerX - radiusX + boundingBox.x(),
        centerY - radiusY + boundingBox.y(),
        radiusX * 2,
        radiusY * 2
    ));
}

PassRefPtr<BasicShape> DeprecatedBasicShapeEllipse::blend(const BasicShape* other, double progress) const
{
    ASSERT(type() == other->type());

    const DeprecatedBasicShapeEllipse* o = static_cast<const DeprecatedBasicShapeEllipse*>(other);
    RefPtr<DeprecatedBasicShapeEllipse> result = DeprecatedBasicShapeEllipse::create();
    result->setCenterX(m_centerX.blend(o->centerX(), progress));
    result->setCenterY(m_centerY.blend(o->centerY(), progress));
    result->setRadiusX(m_radiusX.blend(o->radiusX(), progress));
    result->setRadiusY(m_radiusY.blend(o->radiusY(), progress));
    return result.release();
}

float BasicShapeEllipse::floatValueForRadiusInBox(const BasicShapeRadius& radius, float center, float boxWidthOrHeight) const
{
    if (radius.type() == BasicShapeRadius::Value)
        return floatValueForLength(radius.value(), boxWidthOrHeight);

    if (radius.type() == BasicShapeRadius::ClosestSide)
        return std::min(center, boxWidthOrHeight - center);

    ASSERT(radius.type() == BasicShapeRadius::FarthestSide);
    return std::max(center, boxWidthOrHeight - center);
}

void BasicShapeEllipse::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());

    float centerX = floatValueForCenterCoordinate(m_centerX, boundingBox.width());
    float centerY = floatValueForCenterCoordinate(m_centerY, boundingBox.height());
    float radiusX = floatValueForRadiusInBox(m_radiusX, centerX, boundingBox.width());
    float radiusY = floatValueForRadiusInBox(m_radiusY, centerY, boundingBox.height());
    path.addEllipse(FloatRect(
        centerX - radiusX + boundingBox.x(),
        centerY - radiusY + boundingBox.y(),
        radiusX * 2,
        radiusY * 2));
}

PassRefPtr<BasicShape> BasicShapeEllipse::blend(const BasicShape* other, double progress) const
{
    ASSERT(type() == other->type());
    const BasicShapeEllipse* o = static_cast<const BasicShapeEllipse*>(other);
    RefPtr<BasicShapeEllipse> result = BasicShapeEllipse::create();

    if (m_radiusX.type() != BasicShapeRadius::Value || o->radiusX().type() != BasicShapeRadius::Value
        || m_radiusY.type() != BasicShapeRadius::Value || o->radiusY().type() != BasicShapeRadius::Value) {
        result->setCenterX(o->centerX());
        result->setCenterY(o->centerY());
        result->setRadiusX(o->radiusX());
        result->setRadiusY(o->radiusY());
        return result;
    }

    result->setCenterX(m_centerX.blend(o->centerX(), progress));
    result->setCenterY(m_centerY.blend(o->centerY(), progress));
    result->setRadiusX(m_radiusX.blend(o->radiusX(), progress));
    result->setRadiusY(m_radiusY.blend(o->radiusY(), progress));
    return result.release();
}

void BasicShapePolygon::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());
    ASSERT(!(m_values.size() % 2));
    size_t length = m_values.size();
    
    if (!length)
        return;

    path.moveTo(FloatPoint(floatValueForLength(m_values.at(0), boundingBox.width()) + boundingBox.x(),
        floatValueForLength(m_values.at(1), boundingBox.height()) + boundingBox.y()));
    for (size_t i = 2; i < length; i = i + 2) {
        path.addLineTo(FloatPoint(floatValueForLength(m_values.at(i), boundingBox.width()) + boundingBox.x(),
            floatValueForLength(m_values.at(i + 1), boundingBox.height()) + boundingBox.y()));
    }
    path.closeSubpath();
}

PassRefPtr<BasicShape> BasicShapePolygon::blend(const BasicShape* other, double progress) const
{
    ASSERT(type() == other->type());

    const BasicShapePolygon* o = static_cast<const BasicShapePolygon*>(other);
    ASSERT(m_values.size() == o->values().size());
    ASSERT(!(m_values.size() % 2));

    size_t length = m_values.size();
    RefPtr<BasicShapePolygon> result = BasicShapePolygon::create();
    if (!length)
        return result.release();

    result->setWindRule(o->windRule());

    for (size_t i = 0; i < length; i = i + 2) {
        result->appendPoint(m_values.at(i).blend(o->values().at(i), progress),
            m_values.at(i + 1).blend(o->values().at(i + 1), progress));
    }

    return result.release();
}

void BasicShapeInsetRectangle::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());
    float left = floatValueForLength(m_left, boundingBox.width());
    float top = floatValueForLength(m_top, boundingBox.height());
    path.addRoundedRect(
        FloatRect(
            left + boundingBox.x(),
            top + boundingBox.y(),
            std::max<float>(boundingBox.width() - left - floatValueForLength(m_right, boundingBox.width()), 0),
            std::max<float>(boundingBox.height() - top - floatValueForLength(m_bottom, boundingBox.height()), 0)
        ),
        FloatSize(
            floatValueForLength(m_cornerRadiusX, boundingBox.width()),
            floatValueForLength(m_cornerRadiusY, boundingBox.height())
        )
    );
}

PassRefPtr<BasicShape> BasicShapeInsetRectangle::blend(const BasicShape* other, double progress) const
{
    ASSERT(type() == other->type());

    const BasicShapeInsetRectangle* o = static_cast<const BasicShapeInsetRectangle*>(other);
    RefPtr<BasicShapeInsetRectangle> result =  BasicShapeInsetRectangle::create();
    result->setTop(m_top.blend(o->top(), progress));
    result->setRight(m_right.blend(o->right(), progress));
    result->setBottom(m_bottom.blend(o->bottom(), progress));
    result->setLeft(m_left.blend(o->left(), progress));
    result->setCornerRadiusX(m_cornerRadiusX.blend(o->cornerRadiusX(), progress));
    result->setCornerRadiusY(m_cornerRadiusY.blend(o->cornerRadiusY(), progress));
    return result.release();
}

static FloatSize floatSizeForLengthSize(const LengthSize& lengthSize, const FloatRect& boundingBox)
{
    return FloatSize(floatValueForLength(lengthSize.width(), boundingBox.width()),
        floatValueForLength(lengthSize.height(), boundingBox.height()));
}

void BasicShapeInset::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());
    float left = floatValueForLength(m_left, boundingBox.width());
    float top = floatValueForLength(m_top, boundingBox.height());
    FloatRoundedRect r = FloatRoundedRect(
        FloatRect(
            left + boundingBox.x(),
            top + boundingBox.y(),
            std::max<float>(boundingBox.width() - left - floatValueForLength(m_right, boundingBox.width()), 0),
            std::max<float>(boundingBox.height() - top - floatValueForLength(m_bottom, boundingBox.height()), 0)
        ),
        floatSizeForLengthSize(m_topLeftRadius, boundingBox),
        floatSizeForLengthSize(m_topRightRadius, boundingBox),
        floatSizeForLengthSize(m_bottomLeftRadius, boundingBox),
        floatSizeForLengthSize(m_bottomRightRadius, boundingBox)
    );
    path.addRoundedRect(r);
}

PassRefPtr<BasicShape> BasicShapeInset::blend(const BasicShape* other, double progress) const
{
    ASSERT(type() == other->type());

    const BasicShapeInset* o = static_cast<const BasicShapeInset*>(other);
    RefPtr<BasicShapeInset> result =  BasicShapeInset::create();
    result->setTop(m_top.blend(o->top(), progress));
    result->setRight(m_right.blend(o->right(), progress));
    result->setBottom(m_bottom.blend(o->bottom(), progress));
    result->setLeft(m_left.blend(o->left(), progress));

    result->setTopLeftRadius(m_topLeftRadius.blend(o->topLeftRadius(), progress));
    result->setTopRightRadius(m_topRightRadius.blend(o->topRightRadius(), progress));
    result->setBottomRightRadius(m_bottomRightRadius.blend(o->bottomRightRadius(), progress));
    result->setBottomLeftRadius(m_bottomLeftRadius.blend(o->bottomLeftRadius(), progress));

    return result.release();
}
}
