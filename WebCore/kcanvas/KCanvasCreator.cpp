/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    
    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "KCanvasCreator.h"

#include <math.h>

#include "KRenderingDevice.h"
#include "RenderSVGContainer.h"
#include "Path.h"

namespace WebCore {

KCanvasCreator *KCanvasCreator::s_creator = 0;

KCanvasCreator::KCanvasCreator()
{
}

KCanvasCreator::~KCanvasCreator()
{
}

KCanvasCreator *KCanvasCreator::self()
{
    if(!s_creator)
        s_creator = new KCanvasCreator();

    return s_creator;
}

Path KCanvasCreator::createRoundedRectangle(const FloatRect& box, const FloatSize& roundingRadii) const
{
    Path path;
    float x = box.x();
    float y = box.y();
    float width = box.width();
    float height = box.height();
    float rx = roundingRadii.width();
    float ry = roundingRadii.height();
    if (width <= 0.0f || height <= 0.0f)
        return path;

    double nrx = rx, nry = ry;
    // If rx is greater than half of the width of the rectangle
    // then set rx to half of the width (required in SVG spec)
    if (nrx > width / 2)
        nrx = width / 2;

    // If ry is greater than half of the height of the rectangle
    // then set ry to half of the height (required in SVG spec)
    if (nry > height / 2)
        nry = height / 2;

    path.moveTo(FloatPoint(x + nrx, y));

    if (nrx < width / 2)
        path.addLineTo(FloatPoint(x + width - rx, y));

    path.addBezierCurveTo(FloatPoint(x + width - nrx * (1 - 0.552), y), FloatPoint(x + width, y + nry * (1 - 0.552)), FloatPoint(x + width, y + nry));

    if (nry < height / 2)
        path.addLineTo(FloatPoint(x + width, y + height - nry));

    path.addBezierCurveTo(FloatPoint(x + width, y + height - nry * (1 - 0.552)), FloatPoint(x + width - nrx * (1 - 0.552), y + height), FloatPoint(x + width - nrx, y + height));

    if (nrx < width / 2)
        path.addLineTo(FloatPoint(x + nrx, y + height));

    path.addBezierCurveTo(FloatPoint(x + nrx * (1 - 0.552), y + height), FloatPoint(x, y + height - nry * (1 - 0.552)), FloatPoint(x, y + height - nry));

    if (nry < height / 2)
        path.addLineTo(FloatPoint(x, y + nry));

    path.addBezierCurveTo(FloatPoint(x, y + nry * (1 - 0.552)), FloatPoint(x + nrx * (1 - 0.552), y), FloatPoint(x + nrx, y));

    path.closeSubpath();

    return path;
}

Path KCanvasCreator::createRectangle(const FloatRect& box) const
{
    Path path;
    float x = box.x();
    float y = box.y();
    float width = box.width();
    float height = box.height();
    if (width < 0.0f || height < 0.0f)
        return path;
    
    path.moveTo(FloatPoint(x, y));
    path.addLineTo(FloatPoint(x + width, y));
    path.addLineTo(FloatPoint(x + width, y + height));
    path.addLineTo(FloatPoint(x, y + height));
    path.closeSubpath();

    return path;
}

Path KCanvasCreator::createEllipse(const FloatPoint&c, float rx, float ry) const
{
    float cx = c.x();
    float cy = c.y();
    Path path;
    if (rx <= 0.0f || ry <= 0.0f)
        return path;

    // Ellipse creation - nice & clean agg2 code
    double x = cx, y = cy;

    unsigned step = 0, num = 100;
    bool running = true;
    while(running)
    {
        if(step == num)
        {
            running = false;
            break;
        }

        double angle = double(step) / double(num) * 2.0 * M_PI;
        x = cx + cos(angle) * rx;
        y = cy + sin(angle) * ry;

        step++;
        if(step == 1)
            path.moveTo(FloatPoint(x, y));
        else
            path.addLineTo(FloatPoint(x, y));
    }

    path.closeSubpath();

    return path;
}

Path KCanvasCreator::createCircle(const FloatPoint& c, float r) const
{
    return createEllipse(c, r, r);
}

Path KCanvasCreator::createLine(const FloatPoint& start, const FloatPoint& end) const
{
    Path path;
    if (start.x() == end.x() && start.y() == end.y())
        return path;

    path.moveTo(start);
    path.addLineTo(end);

    return path;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

