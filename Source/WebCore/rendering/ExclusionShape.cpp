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

// If the writingMode is vertical, then the BasicShape's (physical) x and y coordinates are swapped, so that
// line segments are parallel to the internal coordinate system's X axis.

PassOwnPtr<ExclusionShape> ExclusionShape::createExclusionShape(const BasicShape* basicShape, float logicalBoxWidth, float logicalBoxHeight, WritingMode writingMode)
{
    if (!basicShape)
        return nullptr;

    bool horizontalWritingMode = isHorizontalWritingMode(writingMode);
    float boxWidth = horizontalWritingMode ? logicalBoxWidth : logicalBoxHeight;
    float boxHeight = horizontalWritingMode ? logicalBoxHeight : logicalBoxWidth;
    OwnPtr<ExclusionShape> exclusionShape;

    switch (basicShape->type()) {

    case BasicShape::BASIC_SHAPE_RECTANGLE: {
        const BasicShapeRectangle* rectangle = static_cast<const BasicShapeRectangle*>(basicShape);
        float x = floatValueForLength(rectangle->x(), boxWidth);
        float y = floatValueForLength(rectangle->y(), boxHeight);
        float width = floatValueForLength(rectangle->width(), boxWidth);
        float height = floatValueForLength(rectangle->height(), boxHeight);
        Length radiusXLength = rectangle->cornerRadiusX();
        Length radiusYLength = rectangle->cornerRadiusY();
        float radiusX = radiusXLength.isUndefined() ? 0 : floatValueForLength(radiusXLength, boxWidth);
        float radiusY = radiusYLength.isUndefined() ? 0 : floatValueForLength(radiusYLength, boxHeight);

        exclusionShape = horizontalWritingMode
            ? createExclusionRectangle(FloatRect(x, y, width, height), FloatSize(radiusX, radiusY))
            : createExclusionRectangle(FloatRect(y, x, height, width), FloatSize(radiusY, radiusX));
        break;
    }

    case BasicShape::BASIC_SHAPE_CIRCLE: {
        const BasicShapeCircle* circle = static_cast<const BasicShapeCircle*>(basicShape);
        float centerX = floatValueForLength(circle->centerX(), boxWidth);
        float centerY = floatValueForLength(circle->centerY(), boxHeight);
        float radius =  floatValueForLength(circle->radius(), std::max(boxHeight, boxWidth));

        exclusionShape = horizontalWritingMode
            ? createExclusionCircle(FloatPoint(centerX, centerY), radius)
            : createExclusionCircle(FloatPoint(centerY, centerX), radius);
        break;
    }

    case BasicShape::BASIC_SHAPE_ELLIPSE: {
        const BasicShapeEllipse* ellipse = static_cast<const BasicShapeEllipse*>(basicShape);
        float centerX = floatValueForLength(ellipse->centerX(), boxWidth);
        float centerY = floatValueForLength(ellipse->centerY(), boxHeight);
        float radiusX = floatValueForLength(ellipse->radiusX(), boxWidth);
        float radiusY = floatValueForLength(ellipse->radiusY(), boxHeight);

        exclusionShape = horizontalWritingMode
            ? createExclusionEllipse(FloatPoint(centerX, centerY), FloatSize(radiusX, radiusY))
            : createExclusionEllipse(FloatPoint(centerY, centerX), FloatSize(radiusY, radiusX));
        break;
    }

    case BasicShape::BASIC_SHAPE_POLYGON: {
        const BasicShapePolygon* polygon = static_cast<const BasicShapePolygon*>(basicShape);
        const Vector<Length>& values = polygon->values();
        size_t valuesSize = values.size();
        ASSERT(!(valuesSize % 2));
        Vector<FloatPoint>* vertices = new Vector<FloatPoint>(valuesSize / 2);
        for (unsigned i = 0; i < valuesSize; i += 2) {
            FloatPoint vertex(
                floatValueForLength(values.at(i), boxWidth),
                floatValueForLength(values.at(i + 1), boxHeight));
            (*vertices)[i / 2] = horizontalWritingMode ? vertex : vertex.transposedPoint();
        }
        exclusionShape = createExclusionPolygon(adoptPtr(vertices), polygon->windRule());
        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }

    exclusionShape->m_logicalBoxWidth = logicalBoxWidth;
    exclusionShape->m_logicalBoxHeight = logicalBoxHeight;
    exclusionShape->m_writingMode = writingMode;

    return exclusionShape.release();
}

} // namespace WebCore
