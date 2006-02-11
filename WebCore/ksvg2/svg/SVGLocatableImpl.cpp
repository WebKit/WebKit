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

#include "config.h"
#if SVG_SUPPORT
#include <kdom/kdom.h>
#include "SVGRectImpl.h"
#include "SVGMatrixImpl.h"
#include "SVGElementImpl.h"
#include "SVGLocatableImpl.h"
#include "SVGSVGElementImpl.h"

#include <kcanvas/RenderPath.h>

using namespace WebCore;

SVGLocatableImpl::SVGLocatableImpl()
{
}

SVGLocatableImpl::~SVGLocatableImpl()
{
}

SVGElementImpl *SVGLocatableImpl::nearestViewportElement(const SVGStyledElementImpl *e)
{
    NodeImpl *n = e->parentNode();
    while (n && !n->isDocumentNode()) {
        if (n->hasTagName(SVGNames::svgTag) || n->hasTagName(SVGNames::symbolTag) ||
            n->hasTagName(SVGNames::imageTag) || n->hasTagName(SVGNames::foreignObjectTag))
            return static_cast<SVGElementImpl *>(n);

        n = n->parentNode();
    }

    return 0;
}

SVGElementImpl *SVGLocatableImpl::farthestViewportElement(const SVGStyledElementImpl *e)
{
    // FIXME : likely this will be always the <svg> farthest away.
    // If we have a different implementation of documentElement(), one
    // that give the documentElement() of the svg fragment, it could be
    // used instead. This depends on cdf demands though(Rob.)
    SVGElementImpl *farthest = 0;
    NodeImpl *n = e->parentNode();
    while (n && !n->isDocumentNode()) {
        if (n->hasTagName(SVGNames::svgTag) || n->hasTagName(SVGNames::symbolTag) ||
            n->hasTagName(SVGNames::imageTag) || n->hasTagName(SVGNames::foreignObjectTag))
            farthest = static_cast<SVGElementImpl *>(n);

        n = n->parentNode();
    }

    return farthest;
}

// Spec:
// http://www.w3.org/TR/2005/WD-SVGMobile12-20050413/svgudom.html#svg::SVGLocatable
SVGRectImpl *SVGLocatableImpl::getBBox(const SVGStyledElementImpl *e)
{
    SVGRectImpl *rect = 0;

    if(e && e->renderer())
    {
        FloatRect bboxRect = e->renderer()->relativeBBox(false);
        rect = new SVGRectImpl(0);
        rect->setX(bboxRect.x());
        rect->setY(bboxRect.y());
        rect->setWidth(bboxRect.width() - 1);
        rect->setHeight(bboxRect.height() - 1);
    }

    return rect;
}

SVGMatrixImpl *SVGLocatableImpl::getCTM(const SVGElementImpl *element)
{
    if(!element)
        return 0;

    SVGMatrixImpl *ctm = SVGSVGElementImpl::createSVGMatrix();

    NodeImpl *parent = element->parentNode();
    if(parent && parent->nodeType() == ELEMENT_NODE)
    {
        SVGElementImpl *parentElement = svg_dynamic_cast(parent);
        if(parentElement && parentElement->isStyledLocatable())
        {
            RefPtr<SVGMatrixImpl> parentCTM = static_cast<SVGStyledLocatableElementImpl *>(parentElement)->getCTM();
            ctm->multiply(parentCTM.get());
        }
    }

    return ctm;
}

SVGMatrixImpl *SVGLocatableImpl::getScreenCTM(const SVGElementImpl *element)
{
    if(!element)
        return 0;

    SVGMatrixImpl *ctm = SVGSVGElementImpl::createSVGMatrix();

    NodeImpl *parent = element->parentNode();
    if(parent && parent->isElementNode())
    {
        SVGElementImpl *parentElement = static_cast<SVGElementImpl *>(parent);
        if(parentElement->isStyledLocatable())
        {
            RefPtr<SVGMatrixImpl> parentCTM = static_cast<SVGStyledLocatableElementImpl *>(parentElement)->getScreenCTM();
            ctm->multiply(parentCTM.get());
        }
    }

    return ctm;
}
/*
SVGMatrixImpl *SVGLocatableImpl::getTransformToElement(SVGElementImpl *) const
{
    // TODO!
    return 0;
}
*/
// vim:ts=4:noet
#endif // SVG_SUPPORT

