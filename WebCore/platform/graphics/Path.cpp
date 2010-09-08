/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "PathTraversalState.h"
#include <math.h>
#include <wtf/MathExtras.h>

static const float QUARTER = 0.552f; // approximation of control point positions on a bezier
                              // to simulate a quarter of a circle.
namespace WebCore {

#if !PLATFORM(OPENVG) && !PLATFORM(QT)
static void pathLengthApplierFunction(void* info, const PathElement* element)
{
    PathTraversalState& traversalState = *static_cast<PathTraversalState*>(info);
    if (traversalState.m_success)
        return;
    traversalState.m_previous = traversalState.m_current;
    FloatPoint* points = element->points;
    float segmentLength = 0.0f;
    switch (element->type) {
        case PathElementMoveToPoint:
            segmentLength = traversalState.moveTo(points[0]);
            break;
        case PathElementAddLineToPoint:
            segmentLength = traversalState.lineTo(points[0]);
            break;
        case PathElementAddQuadCurveToPoint:
            segmentLength = traversalState.quadraticBezierTo(points[0], points[1]);
            break;
        case PathElementAddCurveToPoint:
            segmentLength = traversalState.cubicBezierTo(points[0], points[1], points[2]);
            break;
        case PathElementCloseSubpath:
            segmentLength = traversalState.closeSubpath();
            break;
    }
    traversalState.m_totalLength += segmentLength; 
    if ((traversalState.m_action == PathTraversalState::TraversalPointAtLength || 
         traversalState.m_action == PathTraversalState::TraversalNormalAngleAtLength) &&
        (traversalState.m_totalLength >= traversalState.m_desiredLength)) {
        FloatSize change = traversalState.m_current - traversalState.m_previous;
        float slope = atan2f(change.height(), change.width());

        if (traversalState.m_action == PathTraversalState::TraversalPointAtLength) {
            float offset = traversalState.m_desiredLength - traversalState.m_totalLength;
            traversalState.m_current.move(offset * cosf(slope), offset * sinf(slope));
        } else {
            static const float rad2deg = 180.0f / piFloat;
            traversalState.m_normalAngle = slope * rad2deg;
        }

        traversalState.m_success = true;
    }
}

float Path::length()
{
    PathTraversalState traversalState(PathTraversalState::TraversalTotalLength);
    apply(&traversalState, pathLengthApplierFunction);
    return traversalState.m_totalLength;
}

FloatPoint Path::pointAtLength(float length, bool& ok)
{
    PathTraversalState traversalState(PathTraversalState::TraversalPointAtLength);
    traversalState.m_desiredLength = length;
    apply(&traversalState, pathLengthApplierFunction);
    ok = traversalState.m_success;
    return traversalState.m_current;
}

float Path::normalAngleAtLength(float length, bool& ok)
{
    PathTraversalState traversalState(PathTraversalState::TraversalNormalAngleAtLength);
    traversalState.m_desiredLength = length;
    apply(&traversalState, pathLengthApplierFunction);
    ok = traversalState.m_success;
    return traversalState.m_normalAngle;
}
#endif

Path Path::createRoundedRectangle(const FloatRect& rectangle, const FloatSize& roundingRadii)
{
    Path path;
    float x = rectangle.x();
    float y = rectangle.y();
    float width = rectangle.width();
    float height = rectangle.height();
    float rx = roundingRadii.width();
    float ry = roundingRadii.height();
    if (width <= 0.0f || height <= 0.0f)
        return path;

    float dx = rx, dy = ry;
    // If rx is greater than half of the width of the rectangle
    // then set rx to half of the width (required in SVG spec)
    if (dx > width * 0.5f)
        dx = width * 0.5f;

    // If ry is greater than half of the height of the rectangle
    // then set ry to half of the height (required in SVG spec)
    if (dy > height * 0.5f)
        dy = height * 0.5f;

    path.moveTo(FloatPoint(x + dx, y));

    if (dx < width * 0.5f)
        path.addLineTo(FloatPoint(x + width - rx, y));

    path.addBezierCurveTo(FloatPoint(x + width - dx * (1 - QUARTER), y), FloatPoint(x + width, y + dy * (1 - QUARTER)), FloatPoint(x + width, y + dy));

    if (dy < height * 0.5)
        path.addLineTo(FloatPoint(x + width, y + height - dy));

    path.addBezierCurveTo(FloatPoint(x + width, y + height - dy * (1 - QUARTER)), FloatPoint(x + width - dx * (1 - QUARTER), y + height), FloatPoint(x + width - dx, y + height));

    if (dx < width * 0.5)
        path.addLineTo(FloatPoint(x + dx, y + height));

    path.addBezierCurveTo(FloatPoint(x + dx * (1 - QUARTER), y + height), FloatPoint(x, y + height - dy * (1 - QUARTER)), FloatPoint(x, y + height - dy));

    if (dy < height * 0.5)
        path.addLineTo(FloatPoint(x, y + dy));

    path.addBezierCurveTo(FloatPoint(x, y + dy * (1 - QUARTER)), FloatPoint(x + dx * (1 - QUARTER), y), FloatPoint(x + dx, y));

    path.closeSubpath();

    return path;
}

Path Path::createRoundedRectangle(const FloatRect& rectangle, const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius)
{
    Path path;

    float width = rectangle.width();
    float height = rectangle.height();
    if (width <= 0.0 || height <= 0.0)
        return path;

    if (width < topLeftRadius.width() + topRightRadius.width()
            || width < bottomLeftRadius.width() + bottomRightRadius.width()
            || height < topLeftRadius.height() + bottomLeftRadius.height()
            || height < topRightRadius.height() + bottomRightRadius.height())
        // If all the radii cannot be accommodated, return a rect.
        return createRectangle(rectangle);

    float x = rectangle.x();
    float y = rectangle.y();

    path.moveTo(FloatPoint(x + topLeftRadius.width(), y));

    path.addLineTo(FloatPoint(x + width - topRightRadius.width(), y));

    path.addBezierCurveTo(FloatPoint(x + width - topRightRadius.width() * (1 - QUARTER), y), FloatPoint(x + width, y + topRightRadius.height() * (1 - QUARTER)), FloatPoint(x + width, y + topRightRadius.height()));

    path.addLineTo(FloatPoint(x + width, y + height - bottomRightRadius.height()));

    path.addBezierCurveTo(FloatPoint(x + width, y + height - bottomRightRadius.height() * (1 - QUARTER)), FloatPoint(x + width - bottomRightRadius.width() * (1 - QUARTER), y + height), FloatPoint(x + width - bottomRightRadius.width(), y + height));

    path.addLineTo(FloatPoint(x + bottomLeftRadius.width(), y + height));

    path.addBezierCurveTo(FloatPoint(x + bottomLeftRadius.width() * (1 - QUARTER), y + height), FloatPoint(x, y + height - bottomLeftRadius.height() * (1 - QUARTER)), FloatPoint(x, y + height - bottomLeftRadius.height()));

    path.addLineTo(FloatPoint(x, y + topLeftRadius.height()));

    path.addBezierCurveTo(FloatPoint(x, y + topLeftRadius.height() * (1 - QUARTER)), FloatPoint(x + topLeftRadius.width() * (1 - QUARTER), y), FloatPoint(x + topLeftRadius.width(), y));

    path.closeSubpath();

    return path;
}

Path Path::createRectangle(const FloatRect& rectangle)
{
    Path path;
    float x = rectangle.x();
    float y = rectangle.y();
    float width = rectangle.width();
    float height = rectangle.height();
    if (width <= 0.0f || height <= 0.0f)
        return path;
    
    path.moveTo(FloatPoint(x, y));
    path.addLineTo(FloatPoint(x + width, y));
    path.addLineTo(FloatPoint(x + width, y + height));
    path.addLineTo(FloatPoint(x, y + height));
    path.closeSubpath();

    return path;
}

Path Path::createEllipse(const FloatPoint& center, float rx, float ry)
{
    float cx = center.x();
    float cy = center.y();
    Path path;
    if (rx <= 0.0f || ry <= 0.0f)
        return path;

    float x = cx;
    float y = cy;

    unsigned step = 0, num = 100;
    bool running = true;
    while (running)
    {
        if (step == num)
        {
            running = false;
            break;
        }

        float angle = static_cast<float>(step) / static_cast<float>(num) * 2.0f * piFloat;
        x = cx + cosf(angle) * rx;
        y = cy + sinf(angle) * ry;

        step++;
        if (step == 1)
            path.moveTo(FloatPoint(x, y));
        else
            path.addLineTo(FloatPoint(x, y));
    }

    path.closeSubpath();

    return path;
}

Path Path::createCircle(const FloatPoint& center, float r)
{
    return createEllipse(center, r, r);
}

Path Path::createLine(const FloatPoint& start, const FloatPoint& end)
{
    Path path;

    path.moveTo(start);
    path.addLineTo(end);

    return path;
}

}
