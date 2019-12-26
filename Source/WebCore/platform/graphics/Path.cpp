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
#include <math.h>
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

#if !USE(DIRECT2D)
float Path::length() const
{
    PathTraversalState traversalState(PathTraversalState::Action::TotalLength);

    apply([&traversalState](const PathElement& element) {
        traversalState.processPathElement(element);
    });

    return traversalState.totalLength();
}
#endif

PathTraversalState Path::traversalStateAtLength(float length) const
{
    PathTraversalState traversalState(PathTraversalState::Action::VectorAtLength, length);

    apply([&traversalState](const PathElement& element) {
        traversalState.processPathElement(element);
    });

    return traversalState;
}

FloatPoint Path::pointAtLength(float length) const
{
    return traversalStateAtLength(length).current();
}

void Path::addRoundedRect(const FloatRect& rect, const FloatSize& roundingRadii, RoundedRectStrategy strategy)
{
    if (rect.isEmpty())
        return;

    FloatSize radius(roundingRadii);
    FloatSize halfSize = rect.size() / 2;

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

    if (strategy == RoundedRectStrategy::PreferNative) {
#if USE(CG) || USE(DIRECT2D)
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

void Path::addBeziersForRoundedRect(const FloatRect& rect, const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius)
{
    moveTo(FloatPoint(rect.x() + topLeftRadius.width(), rect.y()));

    addLineTo(FloatPoint(rect.maxX() - topRightRadius.width(), rect.y()));
    if (topRightRadius.width() > 0 || topRightRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.maxX() - topRightRadius.width() * circleControlPoint(), rect.y()),
            FloatPoint(rect.maxX(), rect.y() + topRightRadius.height() * circleControlPoint()),
            FloatPoint(rect.maxX(), rect.y() + topRightRadius.height()));
    addLineTo(FloatPoint(rect.maxX(), rect.maxY() - bottomRightRadius.height()));
    if (bottomRightRadius.width() > 0 || bottomRightRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.maxX(), rect.maxY() - bottomRightRadius.height() * circleControlPoint()),
            FloatPoint(rect.maxX() - bottomRightRadius.width() * circleControlPoint(), rect.maxY()),
            FloatPoint(rect.maxX() - bottomRightRadius.width(), rect.maxY()));
    addLineTo(FloatPoint(rect.x() + bottomLeftRadius.width(), rect.maxY()));
    if (bottomLeftRadius.width() > 0 || bottomLeftRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.x() + bottomLeftRadius.width() * circleControlPoint(), rect.maxY()),
            FloatPoint(rect.x(), rect.maxY() - bottomLeftRadius.height() * circleControlPoint()),
            FloatPoint(rect.x(), rect.maxY() - bottomLeftRadius.height()));
    addLineTo(FloatPoint(rect.x(), rect.y() + topLeftRadius.height()));
    if (topLeftRadius.width() > 0 || topLeftRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.x(), rect.y() + topLeftRadius.height() * circleControlPoint()),
            FloatPoint(rect.x() + topLeftRadius.width() * circleControlPoint(), rect.y()),
            FloatPoint(rect.x() + topLeftRadius.width(), rect.y()));

    closeSubpath();
}

#if !USE(CG) && !USE(DIRECT2D)
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
    bool isFirst = true;
    path.apply([&stream, &isFirst](const PathElement& element) {
        if (!isFirst)
            stream << ", ";
        isFirst = false;
        switch (element.type) {
        case PathElement::Type::MoveToPoint: // The points member will contain 1 value.
            stream << "move to " << element.points[0];
            break;
        case PathElement::Type::AddLineToPoint: // The points member will contain 1 value.
            stream << "add line to " << element.points[0];
            break;
        case PathElement::Type::AddQuadCurveToPoint: // The points member will contain 2 values.
            stream << "add quad curve to " << element.points[0] << " " << element.points[1];
            break;
        case PathElement::Type::AddCurveToPoint: // The points member will contain 3 values.
            stream << "add curve to " << element.points[0] << " " << element.points[1] << " " << element.points[2];
            break;
        case PathElement::Type::CloseSubpath: // The points member will contain no values.
            stream << "close subpath";
            break;
        }
    });
    
    return stream;
}

}
