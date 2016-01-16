/*
 * Copyright (C) 2003, 2006 Apple Inc.  All rights reserved.
 *                     2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */


#include "config.h"
#include "Path.h"

#include "FloatPoint.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "PathTraversalState.h"
#include "RoundedRect.h"
#include "TextStream.h"
#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {

float Path::length() const
{
    PathTraversalState traversalState(PathTraversalState::Action::TotalLength);

    apply([&traversalState](const PathElement& element) {
        traversalState.processPathElement(element);
    });

    return traversalState.totalLength();
}

PathTraversalState Path::traversalStateAtLength(float length, bool& success) const
{
    PathTraversalState traversalState(PathTraversalState::Action::VectorAtLength, length);

    apply([&traversalState](const PathElement& element) {
        traversalState.processPathElement(element);
    });

    success = traversalState.success();
    return traversalState;
}

FloatPoint Path::pointAtLength(float length, bool& success) const
{
    return traversalStateAtLength(length, success).current();
}

float Path::normalAngleAtLength(float length, bool& success) const
{
    return traversalStateAtLength(length, success).normalAngle();
}

void Path::addRoundedRect(const FloatRect& rect, const FloatSize& roundingRadii, RoundedRectStrategy strategy)
{
    if (rect.isEmpty())
        return;

    FloatSize radius(roundingRadii);
    FloatSize halfSize(rect.width() / 2, rect.height() / 2);

    // Apply the SVG corner radius constraints, per the rect section of the SVG shapes spec: if
    // one of rx,ry is negative, then the other corner radius value is used. If both values are
    // negative then rx = ry = 0. If rx is greater than half of the width of the rectangle
    // then set rx to half of the width; ry is handled similarly.

    if (radius.width() < 0)
        radius.setWidth((radius.height() < 0) ? 0 : radius.height());

    if (radius.height() < 0)
        radius.setHeight(radius.width());

    if (radius.width() > halfSize.width())
        radius.setWidth(halfSize.width());

    if (radius.height() > halfSize.height())
        radius.setHeight(halfSize.height());

    addRoundedRect(FloatRoundedRect(rect, radius, radius, radius, radius), strategy);
}

void Path::addRoundedRect(const FloatRoundedRect& r, RoundedRectStrategy strategy)
{
    if (r.isEmpty())
        return;

    const FloatRoundedRect::Radii& radii = r.radii();
    const FloatRect& rect = r.rect();

    if (!r.isRenderable()) {
        // If all the radii cannot be accommodated, return a rect.
        addRect(rect);
        return;
    }

    if (strategy == PreferNativeRoundedRect) {
#if USE(CG)
        platformAddPathForRoundedRect(rect, radii.topLeft(), radii.topRight(), radii.bottomLeft(), radii.bottomRight());
        return;
#endif
    }

    addBeziersForRoundedRect(rect, radii.topLeft(), radii.topRight(), radii.bottomLeft(), radii.bottomRight());
}

void Path::addRoundedRect(const RoundedRect& r)
{
    addRoundedRect(FloatRoundedRect(r));
}

// Approximation of control point positions on a bezier to simulate a quarter of a circle.
// This is 1-kappa, where kappa = 4 * (sqrt(2) - 1) / 3
static const float gCircleControlPoint = 0.447715f;

void Path::addBeziersForRoundedRect(const FloatRect& rect, const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius)
{
    moveTo(FloatPoint(rect.x() + topLeftRadius.width(), rect.y()));

    addLineTo(FloatPoint(rect.maxX() - topRightRadius.width(), rect.y()));
    if (topRightRadius.width() > 0 || topRightRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.maxX() - topRightRadius.width() * gCircleControlPoint, rect.y()),
            FloatPoint(rect.maxX(), rect.y() + topRightRadius.height() * gCircleControlPoint),
            FloatPoint(rect.maxX(), rect.y() + topRightRadius.height()));
    addLineTo(FloatPoint(rect.maxX(), rect.maxY() - bottomRightRadius.height()));
    if (bottomRightRadius.width() > 0 || bottomRightRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.maxX(), rect.maxY() - bottomRightRadius.height() * gCircleControlPoint),
            FloatPoint(rect.maxX() - bottomRightRadius.width() * gCircleControlPoint, rect.maxY()),
            FloatPoint(rect.maxX() - bottomRightRadius.width(), rect.maxY()));
    addLineTo(FloatPoint(rect.x() + bottomLeftRadius.width(), rect.maxY()));
    if (bottomLeftRadius.width() > 0 || bottomLeftRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.x() + bottomLeftRadius.width() * gCircleControlPoint, rect.maxY()),
            FloatPoint(rect.x(), rect.maxY() - bottomLeftRadius.height() * gCircleControlPoint),
            FloatPoint(rect.x(), rect.maxY() - bottomLeftRadius.height()));
    addLineTo(FloatPoint(rect.x(), rect.y() + topLeftRadius.height()));
    if (topLeftRadius.width() > 0 || topLeftRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.x(), rect.y() + topLeftRadius.height() * gCircleControlPoint),
            FloatPoint(rect.x() + topLeftRadius.width() * gCircleControlPoint, rect.y()),
            FloatPoint(rect.x() + topLeftRadius.width(), rect.y()));

    closeSubpath();
}

#if !USE(CG)
Path Path::polygonPathFromPoints(const Vector<FloatPoint>& points)
{
    Path path;
    if (points.size() < 2)
        return path;

    path.moveTo(points[0]);
    for (size_t i = 1; i < points.size(); ++i)
        path.addLineTo(points[i]);

    path.closeSubpath();
    return path;
}

FloatRect Path::fastBoundingRect() const
{
    return boundingRect();
}
#endif

#ifndef NDEBUG
void Path::dump() const
{
    TextStream stream;
    stream << *this;
    WTFLogAlways("%s", stream.release().utf8().data());
}
#endif

TextStream& operator<<(TextStream& stream, const Path& path)
{
    TextStream::GroupScope group(stream);
    stream << "path " << &path;

    path.apply([&stream](const PathElement& element) {
        switch (element.type) {
        case PathElementMoveToPoint: // The points member will contain 1 value.
            stream << " move to " << element.points[0];
            break;
        case PathElementAddLineToPoint: // The points member will contain 1 value.
            stream << " add line to " << element.points[0];
            break;
        case PathElementAddQuadCurveToPoint: // The points member will contain 2 values.
            stream << " add quad curve to " << element.points[0] << " " << element.points[1];
            break;
        case PathElementAddCurveToPoint: // The points member will contain 3 values.
            stream << " add curve to " << element.points[0] << " " << element.points[1] << " " << element.points[2];
            break;
        case PathElementCloseSubpath: // The points member will contain no values.
            stream << " close subpath";
            break;
        }
    });
    
    return stream;
}

}
