/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 *                     2006 Rob Buis <buis@kde.org>
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

#include <math.h>

#include "FloatRect.h"
#include "FloatPoint.h"
#include "Path.h"

#define QUARTER 0.552 // approximation of control point positions on a bezier
                      // to simulate a quarter of a circle.
namespace WebCore {

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

    double dx = rx, dy = ry;
    // If rx is greater than half of the width of the rectangle
    // then set rx to half of the width (required in SVG spec)
    if (dx > width / 2)
        dx = width / 2;

    // If ry is greater than half of the height of the rectangle
    // then set ry to half of the height (required in SVG spec)
    if (dy > height / 2)
        dy = height / 2;

    path.moveTo(FloatPoint(x + dx, y));

    if (dx < width / 2)
        path.addLineTo(FloatPoint(x + width - rx, y));

    path.addBezierCurveTo(FloatPoint(x + width - dx * (1 - QUARTER), y), FloatPoint(x + width, y + dy * (1 - QUARTER)), FloatPoint(x + width, y + dy));

    if (dy < height / 2)
        path.addLineTo(FloatPoint(x + width, y + height - dy));

    path.addBezierCurveTo(FloatPoint(x + width, y + height - dy * (1 - QUARTER)), FloatPoint(x + width - dx * (1 - QUARTER), y + height), FloatPoint(x + width - dx, y + height));

    if (dx < width / 2)
        path.addLineTo(FloatPoint(x + dx, y + height));

    path.addBezierCurveTo(FloatPoint(x + dx * (1 - QUARTER), y + height), FloatPoint(x, y + height - dy * (1 - QUARTER)), FloatPoint(x, y + height - dy));

    if (dy < height / 2)
        path.addLineTo(FloatPoint(x, y + dy));

    path.addBezierCurveTo(FloatPoint(x, y + dy * (1 - QUARTER)), FloatPoint(x + dx * (1 - QUARTER), y), FloatPoint(x + dx, y));

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
    if (width < 0.0f || height < 0.0f)
        return path;
    
    path.moveTo(FloatPoint(x, y));
    path.addLineTo(FloatPoint(x + width, y));
    path.addLineTo(FloatPoint(x + width, y + height));
    path.addLineTo(FloatPoint(x, y + height));
    path.closeSubpath();

    return path;
}

Path Path::createEllipse(const FloatPoint&c, float rx, float ry)
{
    float cx = c.x();
    float cy = c.y();
    Path path;
    if (rx <= 0.0f || ry <= 0.0f)
        return path;

    double x = cx, y = cy;

    unsigned step = 0, num = 100;
    bool running = true;
    while (running)
    {
        if (step == num)
        {
            running = false;
            break;
        }

        double angle = double(step) / double(num) * 2.0 * M_PI;
        x = cx + cos(angle) * rx;
        y = cy + sin(angle) * ry;

        step++;
        if (step == 1)
            path.moveTo(FloatPoint(x, y));
        else
            path.addLineTo(FloatPoint(x, y));
    }

    path.closeSubpath();

    return path;
}

Path Path::createCircle(const FloatPoint& c, float r)
{
    return createEllipse(c, r, r);
}

Path Path::createLine(const FloatPoint& start, const FloatPoint& end)
{
    Path path;
    if (start.x() == end.x() && start.y() == end.y())
        return path;

    path.moveTo(start);
    path.addLineTo(end);

    return path;
}

}
