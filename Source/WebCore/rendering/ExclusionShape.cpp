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
#include "ExclusionRectangle.h"
#include "LengthFunctions.h"
#include "NotImplemented.h"
#include "WindRule.h"
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

static PassOwnPtr<ExclusionShape> createExclusionRectangle(float x, float y, float width, float height, float rx, float ry)
{
    ASSERT(width >= 0 && height >= 0 && rx >= 0 && ry >= 0);
    return adoptPtr(new ExclusionRectangle(x, y, width, height, rx, ry));
}

static PassOwnPtr<ExclusionShape> createExclusionCircle(float cx, float cy, float radius)
{
    ASSERT(radius >= 0);
    return adoptPtr(new ExclusionRectangle(cx - radius, cy - radius, cx + radius, cy + radius, radius, radius));
}

static PassOwnPtr<ExclusionShape> createExclusionEllipse(float cx, float cy, float rx, float ry)
{
    ASSERT(rx >= 0 && ry >= 0);
    return adoptPtr(new ExclusionRectangle(cx - rx, cy - ry, cx + rx, cy + ry, rx, ry));
}

PassOwnPtr<ExclusionShape> ExclusionShape::createExclusionShape(const BasicShape* wrapShape, float borderBoxLogicalWidth, float borderBoxLogicalHeight)
{
    if (!wrapShape)
        return nullptr;

    switch (wrapShape->type()) {
    case BasicShape::BASIC_SHAPE_RECTANGLE: {
        const BasicShapeRectangle* rectangle = static_cast<const BasicShapeRectangle*>(wrapShape);
        Length rx = rectangle->cornerRadiusX();
        Length ry = rectangle->cornerRadiusY();
        return createExclusionRectangle(
            floatValueForLength(rectangle->x(), borderBoxLogicalWidth),
            floatValueForLength(rectangle->y(), borderBoxLogicalHeight),
            floatValueForLength(rectangle->width(), borderBoxLogicalWidth),
            floatValueForLength(rectangle->height(), borderBoxLogicalHeight),
            rx.isUndefined() ? 0 : floatValueForLength(rx, borderBoxLogicalWidth),
            ry.isUndefined() ? 0 : floatValueForLength(ry, borderBoxLogicalHeight) );
    }

    case BasicShape::BASIC_SHAPE_CIRCLE: {
        const BasicShapeCircle* circle = static_cast<const BasicShapeCircle*>(wrapShape);
        return createExclusionCircle(
            floatValueForLength(circle->centerX(), borderBoxLogicalWidth),
            floatValueForLength(circle->centerY(), borderBoxLogicalHeight),
            floatValueForLength(circle->radius(), std::max(borderBoxLogicalHeight, borderBoxLogicalWidth)) );
    }

    case BasicShape::BASIC_SHAPE_ELLIPSE: {
        const BasicShapeEllipse* ellipse = static_cast<const BasicShapeEllipse*>(wrapShape);
        return createExclusionEllipse(
            floatValueForLength(ellipse->centerX(), borderBoxLogicalWidth),
            floatValueForLength(ellipse->centerY(), borderBoxLogicalHeight),
            floatValueForLength(ellipse->radiusX(), borderBoxLogicalWidth),
            floatValueForLength(ellipse->radiusY(), borderBoxLogicalHeight) );
    }

    case BasicShape::BASIC_SHAPE_POLYGON:
        notImplemented();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebCore
