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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <kdom/kdom.h>
#include "SVGRectImpl.h"
#include "SVGMatrixImpl.h"
#include "SVGElementImpl.h"
#include "SVGLocatableImpl.h"
#include "SVGSVGElementImpl.h"

#include <kcanvas/KCanvasItem.h>

using namespace KSVG;

SVGLocatableImpl::SVGLocatableImpl()
{
}

SVGLocatableImpl::~SVGLocatableImpl()
{
}

SVGElementImpl *SVGLocatableImpl::nearestViewportElement() const
{
    const SVGStyledElementImpl *e = dynamic_cast<const SVGStyledElementImpl *>(this);
    KDOM::NodeImpl *n = e->parentNode();
    while(n && n->nodeType() != KDOM::DOCUMENT_NODE)
    {
        if(n->nodeType() == KDOM::ELEMENT_NODE &&
            (n->id() == ID_SVG || n->id() == ID_SYMBOL ||
             n->id() == ID_FOREIGNOBJECT || n->id() == ID_IMAGE))
            return static_cast<SVGElementImpl *>(n);

        n = n->parentNode();
    }

    return 0;
}

SVGElementImpl *SVGLocatableImpl::farthestViewportElement() const
{
    // FIXME : likely this will be always the <svg> farthest away.
    // If we have a different implementation of documentElement(), one
    // that give the documentElement() of the svg fragment, it could be
    // used instead. This depends on cdf demands though(Rob.)
    SVGElementImpl *farthest = 0;
    const SVGStyledElementImpl *e = dynamic_cast<const SVGStyledElementImpl *>(this);
    KDOM::NodeImpl *n = e->parentNode();
    while(n && n->nodeType() != KDOM::DOCUMENT_NODE)
    {
        if(n->nodeType() == KDOM::ELEMENT_NODE &&
            (n->id() == ID_SVG || n->id() == ID_SYMBOL ||
             n->id() == ID_FOREIGNOBJECT || n->id() == ID_IMAGE))
            farthest = static_cast<SVGElementImpl *>(n);

        n = n->parentNode();
    }

    return farthest;
}

SVGRectImpl *SVGLocatableImpl::getBBox() const
{
    SVGRectImpl *rect = 0;

    const SVGStyledElementImpl *e = dynamic_cast<const SVGStyledElementImpl *>(this);
    if(e && e->canvasItem())
    {
        QRect bboxRect = e->canvasItem()->bbox(false);
        rect = new SVGRectImpl(0);
        rect->setX(bboxRect.x());
        rect->setY(bboxRect.y());
        rect->setWidth(bboxRect.width() - 1);
        rect->setHeight(bboxRect.height() - 1);
    }

    return rect;
}

SVGMatrixImpl *SVGLocatableImpl::getCTM() const
{
    const SVGElementImpl *element = dynamic_cast<const SVGElementImpl *>(this);
    if(!element)
        return 0;

    SVGMatrixImpl *ctm = SVGSVGElementImpl::createSVGMatrix();

    KDOM::NodeImpl *parent = element->parentNode();
    if(parent && parent->nodeType() == KDOM::ELEMENT_NODE)
    {
        SVGElementImpl *parentElement = static_cast<SVGElementImpl *>(parent);
        SVGLocatableImpl *parentLocatable = dynamic_cast<SVGLocatableImpl *>(parentElement);
        if(parentLocatable)
        {
            SVGMatrixImpl *parentCTM = parentLocatable->getCTM();
            parentCTM->ref();
            ctm->multiply(parentCTM);
            parentCTM->deref();
        }
    }

    return ctm;
}

SVGMatrixImpl *SVGLocatableImpl::getScreenCTM() const
{
    const SVGElementImpl *element = dynamic_cast<const SVGElementImpl *>(this);
    if(!element)
        return 0;

    SVGMatrixImpl *ctm = SVGSVGElementImpl::createSVGMatrix();

    KDOM::NodeImpl *parent = element->parentNode();
    if(parent && parent->nodeType() == KDOM::ELEMENT_NODE)
    {
        SVGElementImpl *parentElement = static_cast<SVGElementImpl *>(parent);
        SVGLocatableImpl *parentLocatable = dynamic_cast<SVGLocatableImpl *>(parentElement);
        if(parentLocatable)
        {
            SVGMatrixImpl *parentCTM = parentLocatable->getScreenCTM();
            parentCTM->ref();
            ctm->multiply(parentCTM);
            parentCTM->deref();
        }
    }

    return ctm;
}

SVGMatrixImpl *SVGLocatableImpl::getTransformToElement(SVGElementImpl *) const
{
    // TODO!
    return 0;
}

// vim:ts=4:noet
