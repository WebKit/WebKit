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
#if SVG_SUPPORT
#include "KCanvasCreator.h"

#include <math.h>

#include "KCanvasPath.h"
#include "KRenderingDevice.h"
#include "KCanvasContainer.h"

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

KCanvasPath* KCanvasCreator::createRoundedRectangle(float x, float y, float width, float height, float rx, float ry) const
{
    KCanvasPath* path = renderingDevice()->createPath();

    if (width <= 0.0f || height <= 0.0f || !path)
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

    path->moveTo(x + nrx, y);

    if (nrx < width / 2)
        path->lineTo(x + width - rx, y);

    path->curveTo(x + width - nrx * (1 - 0.552), y, x + width, y + nry * (1 - 0.552), x + width, y + nry);

    if (nry < height / 2)
        path->lineTo(x + width, y + height - nry);

    path->curveTo(x + width, y + height - nry * (1 - 0.552), x + width - nrx * (1 - 0.552), y + height, x + width - nrx, y + height);

    if (nrx < width / 2)
        path->lineTo(x + nrx, y + height);

    path->curveTo(x + nrx * (1 - 0.552), y + height, x, y + height - nry * (1 - 0.552), x, y + height - nry);

    if (nry < height / 2)
        path->lineTo(x, y + nry);

    path->curveTo(x, y + nry * (1 - 0.552), x + nrx * (1 - 0.552), y, x + nrx, y);

    path->closeSubpath();
    return path;
}

KCanvasPath* KCanvasCreator::createRectangle(float x, float y, float width, float height) const
{
    KCanvasPath* path = renderingDevice()->createPath();
    
    if (width <= 0.0f || height <= 0.0f || !path)
        return path;
    
    path->moveTo(x, y);
    path->lineTo(x + width, y);
    path->lineTo(x + width, y + height);
    path->lineTo(x, y + height);
    path->closeSubpath();
    return path;
}

KCanvasPath* KCanvasCreator::createEllipse(float cx, float cy, float rx, float ry) const
{
    KCanvasPath* path = renderingDevice()->createPath();

    if (rx <= 0.0f || ry <= 0.0f || !path)
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
            path->moveTo(x, y);
        else
            path->lineTo(x, y);
    }

    path->closeSubpath();
    return path;
}

KCanvasPath* KCanvasCreator::createCircle(float cx, float cy, float r) const
{
    return createEllipse(cx, cy, r, r);
}

KCanvasPath* KCanvasCreator::createLine(float x1, float y1, float x2, float y2) const
{
    KCanvasPath* path = renderingDevice()->createPath();
    
    if ((x1 == x2 && y1 == y2) || !path)
        return path;

    path->moveTo(x1, y1);
    path->lineTo(x2, y2);
    return path;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

