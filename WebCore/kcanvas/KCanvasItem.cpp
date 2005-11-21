/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

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
#include <qrect.h>
#include <kdebug.h>

#include "kcanvas/KCanvas.h"
#include "kcanvas/RenderPath.h"
#include "KCanvasMatrix.h"
#include "KRenderingDevice.h"
#include "KCanvasContainer.h"
#include "KRenderingFillPainter.h"
#include "KRenderingStrokePainter.h"

#include "KCanvasRenderingStyle.h"
#include "SVGRenderStyle.h"

class RenderPath::Private
{
public:
    Private()
    {
        path = 0;
        style = 0;
    }

    ~Private()
    {
        delete style;
    }

    KSVG::KCanvasRenderingStyle *style;
    KCanvasUserData path;

    QRect fillBBox, strokeBbox;
    QMatrix matrix;
};        

// RenderPath
RenderPath::RenderPath(khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node) : RenderObject((DOM::NodeImpl *)node), d(new Private())
{
    Q_ASSERT(style != 0);
    d->style = new KSVG::KCanvasRenderingStyle(canvas(), style);
}

RenderPath::~RenderPath()
{
    if(d->path && canvas() && canvas()->renderingDevice())
        canvas()->renderingDevice()->deletePath(d->path);
    delete d;
}

void RenderPath::setStyle(khtml::RenderStyle *style)
{
    d->style->updateStyle(style, this);
    khtml::RenderObject::setStyle(style);
}

QMatrix RenderPath::localTransform() const
{
    return d->matrix;
}

void RenderPath::setLocalTransform(const QMatrix &matrix)
{
    d->matrix = matrix;
}

bool RenderPath::fillContains(const QPoint &p) const
{
    if(d->path && d->style && canvas() && canvas()->renderingDevice())
        return hitsPath(p, true);

    return false;
}

bool RenderPath::strokeContains(const QPoint &p) const
{
    if(d->path && d->style && canvas() && canvas()->renderingDevice())
        return hitsPath(p, false);

    return false;
}

QRect RenderPath::bbox(bool includeStroke) const
{
    QRect result;
    
    if (!d->path || !canvas() || !canvas()->renderingDevice())
        return result;

    if (includeStroke) {
        if(!d->strokeBbox.isValid())
            d->strokeBbox = bboxPath(true);
        result = d->strokeBbox;
    } else {
        if(!d->fillBBox.isValid())
            d->fillBBox = bboxPath(false);
        result = d->fillBBox;
    }
    
    return result;
}

bool RenderPath::hitsPath(const QPoint &hitPoint, bool fill) const
{
    return false;
}

QRect RenderPath::bboxPath(bool includeStroke, bool includeTransforms) const
{
    return QRect();
}

void RenderPath::setupForDraw() const
{
    if(d->path && d->style && canvas() && canvas()->renderingDevice())
    {
        if(d->style->fillPainter() && d->style->fillPainter()->paintServer())
            d->style->fillPainter()->paintServer()->setActiveClient(this);

        if(d->style->strokePainter() && d->style->strokePainter()->paintServer())
            d->style->strokePainter()->paintServer()->setActiveClient(this);
    }
}

void RenderPath::changePath(KCanvasUserData newPath)
{
    if(canvas() && canvas()->renderingDevice())
    {
        canvas()->renderingDevice()->setCurrentPath(newPath);
        if (d->path)
            canvas()->renderingDevice()->deletePath(d->path);

        d->path = newPath;
    }
}

KCanvasUserData RenderPath::path() const
{
    return d->path;
}

KSVG::KCanvasRenderingStyle *RenderPath::canvasStyle() const
{
    return d->style;
}

const KCanvasCommonArgs RenderPath::commonArgs() const
{
    KCanvasCommonArgs args;
    args.setPath(path());
    args.setStyle(canvasStyle());
    return args;
}

// vim:ts=4:noet
