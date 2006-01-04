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
#include "KCanvasRenderingStyle.h"
#include "KRenderingDevice.h"
#include "KCanvasContainer.h"
#include "SVGStyledElementImpl.h"

class KCanvasContainer::Private
{
public:
    Private() : drawsContents(true), slice(false) { }    
    ~Private() { }

    bool drawsContents : 1;
    bool slice : 1;
    QMatrix matrix;
};

KCanvasContainer::KCanvasContainer(KSVG::SVGStyledElementImpl *node)
: khtml::RenderContainer(node), d(new Private())
{
    setReplaced(true);
}

KCanvasContainer::~KCanvasContainer()
{
    delete d;
}

bool KCanvasContainer::drawsContents() const
{
    return d->drawsContents;
}

void KCanvasContainer::setDrawsContents(bool drawsContents)
{
    d->drawsContents = drawsContents;
}

QMatrix KCanvasContainer::localTransform() const
{
    return d->matrix;
}

void KCanvasContainer::setLocalTransform(const QMatrix &matrix)
{
    d->matrix = matrix;
}

bool KCanvasContainer::fillContains(const QPointF &p) const
{
    khtml::RenderObject *current = firstChild();
    for(; current != 0; current = current->nextSibling())
    {
        if(current->isRenderPath() && static_cast<RenderPath *>(current)->fillContains(p))
            return true;
    }

    return false;
}

bool KCanvasContainer::strokeContains(const QPointF &p) const
{
    khtml::RenderObject *current = firstChild();
    for(; current != 0; current = current->nextSibling())
    {
        if(current->isRenderPath() && static_cast<RenderPath *>(current)->strokeContains(p))
            return true;
    }

    return false;
}

QRectF KCanvasContainer::relativeBBox(bool includeStroke) const
{
    QRectF rect;
    
    khtml::RenderObject *current = firstChild();
    for(; current != 0; current = current->nextSibling()) {
        QRectF childBBox = current->relativeBBox(includeStroke);
        QRectF mappedBBox = current->localTransform().mapRect(childBBox);
        rect = rect.unite(mappedBBox);
    }

    return rect;
}

void KCanvasContainer::setSlice(bool slice)
{
    d->slice = slice;
}

bool KCanvasContainer::slice() const
{
    return d->slice;
}

KCanvasMatrix KCanvasContainer::getAspectRatio(const QRectF logical, const QRectF physical) const
{
    KCanvasMatrix temp;

    float logicX = logical.x();
    float logicY = logical.y();
    float logicWidth = logical.width();
    float logicHeight = logical.height();
    float physWidth = physical.width();
    float physHeight = physical.height();

    float vpar = logicWidth / logicHeight;
    float svgar = physWidth / physHeight;

    if(align() == ALIGN_NONE)
    {
        temp.scale(physWidth / logicWidth, physHeight / logicHeight);
        temp.translate(-logicX, -logicY);
    }
    else if((vpar < svgar && !slice()) || (vpar >= svgar && slice()))
    {
        temp.scale(physHeight / logicHeight, physHeight / logicHeight);

        if(align() == ALIGN_XMINYMIN || align() == ALIGN_XMINYMID || align() == ALIGN_XMINYMAX)
            temp.translate(-logicX, -logicY);
        else if(align() == ALIGN_XMIDYMIN || align() == ALIGN_XMIDYMID || align() == ALIGN_XMIDYMAX)
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight) / 2, -logicY);
        else
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight), -logicY);
    }
    else
    {
        temp.scale(physWidth / logicWidth, physWidth / logicWidth);

        if(align() == ALIGN_XMINYMIN || align() == ALIGN_XMIDYMIN || align() == ALIGN_XMAXYMIN)
            temp.translate(-logicX, -logicY);
        else if(align() == ALIGN_XMINYMID || align() == ALIGN_XMIDYMID || align() == ALIGN_XMAXYMID)
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth) / 2);
        else
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth));
    }

    return temp;
}

// vim:ts=4:noet
