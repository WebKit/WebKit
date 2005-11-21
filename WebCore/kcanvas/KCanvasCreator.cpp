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
#include <math.h>

#include <kdebug.h>
#include <kstaticdeleter.h>

#include "kcanvas/KCanvas.h"
#include "KCanvasPath.h"
#include "KCanvasCreator.h"
#include "KRenderingDevice.h"
#include "KCanvasContainer.h"

static KStaticDeleter<KCanvasCreator> canvasCreatorDeleter;
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
        s_creator = canvasCreatorDeleter.setObject(s_creator, new KCanvasCreator());

    return s_creator;
}

KCPathDataList KCanvasCreator::createRoundedRectangle(float x, float y, float width, float height, float rx, float ry) const
{
    KCPathDataList list;

    if (width <= 0.0f || height <= 0.0f)
        return list;

    double nrx = rx, nry = ry;
    // If rx is greater than half of the width of the rectangle
    // then set rx to half of the width (required in SVG spec)
    if(nrx > width / 2)
        nrx = width / 2;

    // If ry is greater than half of the height of the rectangle
    // then set ry to half of the height (required in SVG spec)
    if(nry > height / 2)
        nry = height / 2;

    list.moveTo(x + nrx, y);
    list.curveTo(x + nrx * (1 - 0.552), y, x, y + nry * (1 - 0.552), x, y + nry);

    if(nry < height / 2)
        list.lineTo(x, y + height - nry);

    list.curveTo(x, y + height - nry * (1 - 0.552), x + nrx * (1 - 0.552), y + height, x + nrx, y + height);

    if(nrx < width / 2)
        list.lineTo(x + width - nrx, y + height);

    list.curveTo(x + width - nrx * (1 - 0.552), y + height, x + width, y + height - nry * (1 - 0.552), x + width, y + height - nry);

    if(nry < height / 2)
        list.lineTo(x + width, y + nry);

    list.curveTo(x + width, y + nry * (1 - 0.552), x + width - nrx * (1 - 0.552), y, x + width - nrx, y);

    if(nrx < width / 2)
        list.lineTo(x + nrx, y);

    list.closeSubpath();
    return list;
}

KCPathDataList KCanvasCreator::createRectangle(float x, float y, float width, float height) const
{
    KCPathDataList list;
    
    if (width <= 0.0f || height <= 0.0f)
        return list;
    
    list.moveTo(x, y);
    list.lineTo(x + width, y);
    list.lineTo(x + width, y + height);
    list.lineTo(x, y + height);
    list.closeSubpath();
    return list;
}

KCPathDataList KCanvasCreator::createEllipse(float cx, float cy, float rx, float ry) const
{
    KCPathDataList list;

    if (rx <= 0.0f || ry <= 0.0f)
        return list;

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
            list.moveTo(x, y);
        else
            list.lineTo(x, y);
    }

    list.closeSubpath();
    return list;
}

KCPathDataList KCanvasCreator::createCircle(float cx, float cy, float r) const
{
    return createEllipse(cx, cy, r, r);
}

KCPathDataList KCanvasCreator::createLine(float x1, float y1, float x2, float y2) const
{
    KCPathDataList list;
    
    if (x1 == x2 && y1 == y2)
        return list;

    list.moveTo(x1, y1);
    list.lineTo(x2, y2);
    return list;
}

KCanvasUserData KCanvasCreator::createCanvasPathData(KRenderingDevice *device, const KCPathDataList &pathData) const
{
    device->startPath();

    int dataLength = pathData.count();
    if(dataLength == 0)
    {
        device->endPath();
        return device->currentPath();
    }

    KCPathDataList::ConstIterator it;
    KCPathDataList::ConstIterator end = pathData.end();

    // There are pure-moveto paths which reference
    // paint servers *bah* -> Do NOT render them!
    bool valid = false;
    for(it = pathData.begin(); it != end; ++it)
    {
        KCPathData data = *it;        

        if(data.cmd == CMD_MOVE)
            device->moveTo(data.x3, data.y3);
        else if(data.cmd == CMD_LINE)
        {
            device->lineTo(data.x3, data.y3);
            valid = true;
        }
        else if(data.cmd == CMD_CLOSE_SUBPATH)
            device->closeSubpath();
        else
        {
            device->curveTo(data.x1, data.y1, data.x2, data.y2, data.x3, data.y3);
            valid = true;
        }
    }

    if(!valid)
    {
        device->endPath();
        device->deletePath(device->currentPath());
        device->startPath();
    }

    device->endPath();
    return device->currentPath();
}

// vim:ts=4:noet
