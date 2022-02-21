/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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
#include "CanvasPath.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include <wtf/MathExtras.h>

namespace WebCore {

void CanvasPath::closePath()
{
    if (m_path.isEmpty())
        return;

    FloatRect boundRect = m_path.fastBoundingRect();
    if (boundRect.width() || boundRect.height())
        m_path.closeSubpath();
}

void CanvasPath::moveTo(float x, float y)
{
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    if (!hasInvertibleTransform())
        return;
    m_path.moveTo(FloatPoint(x, y));
}

void CanvasPath::lineTo(FloatPoint point)
{
    lineTo(point.x(), point.y());
}

void CanvasPath::lineTo(float x, float y)
{
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    if (!hasInvertibleTransform())
        return;

    FloatPoint p1 = FloatPoint(x, y);
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(p1);
    else if (p1 != m_path.currentPoint())
        m_path.addLineTo(p1);
}

void CanvasPath::quadraticCurveTo(float cpx, float cpy, float x, float y)
{
    if (!std::isfinite(cpx) || !std::isfinite(cpy) || !std::isfinite(x) || !std::isfinite(y))
        return;
    if (!hasInvertibleTransform())
        return;
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(FloatPoint(cpx, cpy));

    FloatPoint p1 = FloatPoint(x, y);
    FloatPoint cp = FloatPoint(cpx, cpy);
    if (p1 != m_path.currentPoint() || p1 != cp)
        m_path.addQuadCurveTo(cp, p1);
}

void CanvasPath::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
    if (!std::isfinite(cp1x) || !std::isfinite(cp1y) || !std::isfinite(cp2x) || !std::isfinite(cp2y) || !std::isfinite(x) || !std::isfinite(y))
        return;
    if (!hasInvertibleTransform())
        return;
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(FloatPoint(cp1x, cp1y));

    FloatPoint p1 = FloatPoint(x, y);
    FloatPoint cp1 = FloatPoint(cp1x, cp1y);
    FloatPoint cp2 = FloatPoint(cp2x, cp2y);
    if (p1 != m_path.currentPoint() || p1 != cp1 ||  p1 != cp2)
        m_path.addBezierCurveTo(cp1, cp2, p1);
}

ExceptionOr<void> CanvasPath::arcTo(float x1, float y1, float x2, float y2, float r)
{
    if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2) || !std::isfinite(r))
        return { };

    if (r < 0)
        return Exception { IndexSizeError };

    if (!hasInvertibleTransform())
        return { };

    FloatPoint p1 = FloatPoint(x1, y1);
    FloatPoint p2 = FloatPoint(x2, y2);

    if (!m_path.hasCurrentPoint())
        m_path.moveTo(p1);
    else if (p1 == m_path.currentPoint() || p1 == p2 || !r)
        lineTo(x1, y1);
    else
        m_path.addArcTo(p1, p2, r);

    return { };
}

static void normalizeAngles(float& startAngle, float& endAngle, bool anticlockwise)
{
    float newStartAngle = startAngle;
    if (newStartAngle < 0)
        newStartAngle = (2 * piFloat) + fmodf(newStartAngle, -(2 * piFloat));
    else
        newStartAngle = fmodf(newStartAngle, 2 * piFloat);

    float delta = newStartAngle - startAngle;
    startAngle = newStartAngle;
    endAngle = endAngle + delta;
    ASSERT(newStartAngle >= 0 && (newStartAngle < 2 * piFloat || WTF::areEssentiallyEqual<float>(newStartAngle, 2 * piFloat)));

    if (anticlockwise && startAngle - endAngle >= 2 * piFloat)
        endAngle = startAngle - 2 * piFloat;
    else if (!anticlockwise && endAngle - startAngle >= 2 * piFloat)
        endAngle = startAngle + 2 * piFloat;
}

ExceptionOr<void> CanvasPath::arc(float x, float y, float radius, float startAngle, float endAngle, bool anticlockwise)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius) || !std::isfinite(startAngle) || !std::isfinite(endAngle))
        return { };

    if (radius < 0)
        return Exception { IndexSizeError };

    if (!hasInvertibleTransform())
        return { };

    normalizeAngles(startAngle, endAngle, anticlockwise);

    if (!radius || startAngle == endAngle) {
        // The arc is empty but we still need to draw the connecting line.
        lineTo(x + radius * cosf(startAngle), y + radius * sinf(startAngle));
        return { };
    }

    m_path.addArc(FloatPoint(x, y), radius, startAngle, endAngle, anticlockwise);
    return { };
}
    
ExceptionOr<void> CanvasPath::ellipse(float x, float y, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, bool anticlockwise)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radiusX) || !std::isfinite(radiusY) || !std::isfinite(rotation) || !std::isfinite(startAngle) || !std::isfinite(endAngle))
        return { };

    if (radiusX < 0 || radiusY < 0)
        return Exception { IndexSizeError };

    if (!hasInvertibleTransform())
        return { };

    normalizeAngles(startAngle, endAngle, anticlockwise);

    if ((!radiusX && !radiusY) || startAngle == endAngle) {
        AffineTransform transform;
        transform.translate(x, y).rotate(rad2deg(rotation));

        lineTo(transform.mapPoint(FloatPoint(radiusX * cosf(startAngle), radiusY * sinf(startAngle))));
        return { };
    }

    if (!radiusX || !radiusY) {
        AffineTransform transform;
        transform.translate(x, y).rotate(rad2deg(rotation));

        lineTo(transform.mapPoint(FloatPoint(radiusX * cosf(startAngle), radiusY * sinf(startAngle))));

        if (!anticlockwise) {
            for (float angle = startAngle - fmodf(startAngle, piOverTwoFloat) + piOverTwoFloat; angle < endAngle; angle += piOverTwoFloat)
                lineTo(transform.mapPoint(FloatPoint(radiusX * cosf(angle), radiusY * sinf(angle))));
        } else {
            for (float angle = startAngle - fmodf(startAngle, piOverTwoFloat); angle > endAngle; angle -= piOverTwoFloat)
                lineTo(transform.mapPoint(FloatPoint(radiusX * cosf(angle), radiusY * sinf(angle))));
        }

        lineTo(transform.mapPoint(FloatPoint(radiusX * cosf(endAngle), radiusY * sinf(endAngle))));
        return { };
    }

    m_path.addEllipse(FloatPoint(x, y), radiusX, radiusY, rotation, startAngle, endAngle, anticlockwise);
    return { };
}

void CanvasPath::rect(float x, float y, float width, float height)
{
    if (!hasInvertibleTransform())
        return;

    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height))
        return;

    if (!width && !height) {
        m_path.moveTo(FloatPoint(x, y));
        return;
    }

    m_path.addRect(FloatRect(x, y, width, height));
}

float CanvasPath::currentX() const
{
    return m_path.currentPoint().x();
}

float CanvasPath::currentY() const
{
    return m_path.currentPoint().y();
}

}
