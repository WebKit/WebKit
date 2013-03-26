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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ExclusionShape.h"

#include "BasicShapeFunctions.h"
#include "ExclusionPolygon.h"
#include "ExclusionRectangle.h"
#include "FloatSize.h"
#include "LengthFunctions.h"
#include "WindRule.h"
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

static PassOwnPtr<ExclusionShape> createExclusionRectangle(const FloatRect& bounds, const FloatSize& radii)
{
    ASSERT(bounds.width() >= 0 && bounds.height() >= 0 && radii.width() >= 0 && radii.height() >= 0);
    return adoptPtr(new ExclusionRectangle(bounds, radii));
}

static PassOwnPtr<ExclusionShape> createExclusionCircle(const FloatPoint& center, float radius)
{
    ASSERT(radius >= 0);
    return adoptPtr(new ExclusionRectangle(FloatRect(center.x() - radius, center.y() - radius, radius*2, radius*2), FloatSize(radius, radius)));
}

static PassOwnPtr<ExclusionShape> createExclusionEllipse(const FloatPoint& center, const FloatSize& radii)
{
    ASSERT(radii.width() >= 0 && radii.height() >= 0);
    return adoptPtr(new ExclusionRectangle(FloatRect(center.x() - radii.width(), center.y() - radii.height(), radii.width()*2, radii.height()*2), radii));
}

static PassOwnPtr<ExclusionShape> createExclusionPolygon(PassOwnPtr<Vector<FloatPoint> > vertices, WindRule fillRule)
{
    return adoptPtr(new ExclusionPolygon(vertices, fillRule));
}

static inline FloatRect physicalRectToLogical(const FloatRect& rect, float logicalBoxHeight, WritingMode writingMode)
{
    if (isHorizontalWritingMode(writingMode))
        return rect;
    if (isFlippedBlocksWritingMode(writingMode))
        return FloatRect(rect.y(), logicalBoxHeight - rect.maxX(), rect.height(), rect.width());
    return rect.transposedRect();
}

static inline FloatPoint physicalPointToLogical(const FloatPoint& point, float logicalBoxHeight, WritingMode writingMode)
{
    if (isHorizontalWritingMode(writingMode))
        return point;
    if (isFlippedBlocksWritingMode(writingMode))
        return FloatPoint(point.y(), logicalBoxHeight - point.x());
    return point.transposedPoint();
}

static inline FloatSize physicalSizeToLogical(const FloatSize& size, WritingMode writingMode)
{
    if (isHorizontalWritingMode(writingMode))
        return size;
    return size.transposedSize();
}

PassOwnPtr<ExclusionShape> ExclusionShape::createExclusionShape(const BasicShape* basicShape, float logicalBoxWidth, float logicalBoxHeight, WritingMode writingMode, Length margin, Length padding)
{
    ASSERT(basicShape);

    bool horizontalWritingMode = isHorizontalWritingMode(writingMode);
    float boxWidth = horizontalWritingMode ? logicalBoxWidth : logicalBoxHeight;
    float boxHeight = horizontalWritingMode ? logicalBoxHeight : logicalBoxWidth;
    OwnPtr<ExclusionShape> exclusionShape;

    switch (basicShape->type()) {

    case BasicShape::BASIC_SHAPE_RECTANGLE: {
        const BasicShapeRectangle* rectangle = static_cast<const BasicShapeRectangle*>(basicShape);
        FloatRect bounds(
            floatValueForLength(rectangle->x(), boxWidth),
            floatValueForLength(rectangle->y(), boxHeight),
            floatValueForLength(rectangle->width(), boxWidth),
            floatValueForLength(rectangle->height(), boxHeight));
        Length radiusXLength = rectangle->cornerRadiusX();
        Length radiusYLength = rectangle->cornerRadiusY();
        FloatSize cornerRadii(
            radiusXLength.isUndefined() ? 0 : floatValueForLength(radiusXLength, boxWidth),
            radiusYLength.isUndefined() ? 0 : floatValueForLength(radiusYLength, boxHeight));
        FloatRect logicalBounds = physicalRectToLogical(bounds, logicalBoxHeight, writingMode);

        exclusionShape = createExclusionRectangle(logicalBounds, physicalSizeToLogical(cornerRadii, writingMode));
        exclusionShape->m_boundingBox = logicalBounds;
        break;
    }

    case BasicShape::BASIC_SHAPE_CIRCLE: {
        const BasicShapeCircle* circle = static_cast<const BasicShapeCircle*>(basicShape);
        float centerX = floatValueForLength(circle->centerX(), boxWidth);
        float centerY = floatValueForLength(circle->centerY(), boxHeight);
        float radius = floatValueForLength(circle->radius(), std::min(boxHeight, boxWidth));
        FloatPoint logicalCenter = physicalPointToLogical(FloatPoint(centerX, centerY), logicalBoxHeight, writingMode);

        exclusionShape = createExclusionCircle(logicalCenter, radius);
        exclusionShape->m_boundingBox = FloatRect(logicalCenter.x() - radius, logicalCenter.y() - radius, radius * 2, radius * 2);
        break;
    }

    case BasicShape::BASIC_SHAPE_ELLIPSE: {
        const BasicShapeEllipse* ellipse = static_cast<const BasicShapeEllipse*>(basicShape);
        float centerX = floatValueForLength(ellipse->centerX(), boxWidth);
        float centerY = floatValueForLength(ellipse->centerY(), boxHeight);
        float radiusX = floatValueForLength(ellipse->radiusX(), boxWidth);
        float radiusY = floatValueForLength(ellipse->radiusY(), boxHeight);
        FloatPoint logicalCenter = physicalPointToLogical(FloatPoint(centerX, centerY), logicalBoxHeight, writingMode);
        FloatSize logicalRadii = physicalSizeToLogical(FloatSize(radiusX, radiusY), writingMode);

        exclusionShape = createExclusionEllipse(logicalCenter, logicalRadii);
        exclusionShape->m_boundingBox = FloatRect(logicalCenter - logicalRadii, logicalRadii + logicalRadii);
        break;
    }

    case BasicShape::BASIC_SHAPE_POLYGON: {
        const BasicShapePolygon* polygon = static_cast<const BasicShapePolygon*>(basicShape);
        const Vector<Length>& values = polygon->values();
        size_t valuesSize = values.size();
        ASSERT(!(valuesSize % 2));
        FloatRect boundingBox;
        Vector<FloatPoint>* vertices = new Vector<FloatPoint>(valuesSize / 2);
        for (unsigned i = 0; i < valuesSize; i += 2) {
            FloatPoint vertex(
                floatValueForLength(values.at(i), boxWidth),
                floatValueForLength(values.at(i + 1), boxHeight));
            (*vertices)[i / 2] = physicalPointToLogical(vertex, logicalBoxHeight, writingMode);
            if (!i)
                boundingBox.setLocation(vertex);
            else
                boundingBox.extend(vertex);
        }
        exclusionShape = createExclusionPolygon(adoptPtr(vertices), polygon->windRule());
        exclusionShape->m_boundingBox = boundingBox;
        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }

    exclusionShape->m_logicalBoxWidth = logicalBoxWidth;
    exclusionShape->m_logicalBoxHeight = logicalBoxHeight;
    exclusionShape->m_writingMode = writingMode;
    exclusionShape->m_margin = floatValueForLength(margin, 0);
    exclusionShape->m_padding = floatValueForLength(padding, 0);

    return exclusionShape.release();
}

} // namespace WebCore
