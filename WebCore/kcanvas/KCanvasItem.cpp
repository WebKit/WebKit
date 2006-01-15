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
#include "IntRect.h"
#include <kdebug.h>
#include <kxmlcore/Assertions.h>

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
    RefPtr<KCanvasPath> path;

    FloatRect fillBBox, strokeBbox;
    QMatrix matrix;
};        

// RenderPath
RenderPath::RenderPath(khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node) : RenderObject((DOM::NodeImpl *)node), d(new Private())
{
    Q_ASSERT(style != 0);
}

RenderPath::~RenderPath()
{
    delete d;
}

QMatrix RenderPath::localTransform() const
{
    return d->matrix;
}

void RenderPath::setLocalTransform(const QMatrix &matrix)
{
    d->matrix = matrix;
}

bool RenderPath::fillContains(const FloatPoint &p) const
{
    if(d->path)
        return hitsPath(p, true);

    return false;
}

bool RenderPath::strokeContains(const FloatPoint &p) const
{
    if(d->path)
        return hitsPath(p, false);

    return false;
}

FloatRect RenderPath::relativeBBox(bool includeStroke) const
{
    FloatRect result;
    
    if (!d->path)
        return result;

    if (includeStroke) {
        if(!d->strokeBbox.isValid())
            d->strokeBbox = bboxForPath(true);
        result = d->strokeBbox;
    } else {
        if(!d->fillBBox.isValid())
            d->fillBBox = bboxForPath(false);
        result = d->fillBBox;
    }
    
    return result;
}

void RenderPath::changePath(KCanvasPath* newPath)
{
    d->path = newPath;
}

KCanvasPath* RenderPath::path() const
{
    return d->path.get();
}

// vim:ts=4:noet
