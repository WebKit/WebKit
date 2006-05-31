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
#include "SVGLocatable.h"

#include "SVGElement.h"
#include "SVGMatrix.h"
#include "SVGRect.h"
#include "SVGSVGElement.h"
#include <kcanvas/RenderPath.h>

using namespace WebCore;

SVGLocatable::SVGLocatable()
{
}

SVGLocatable::~SVGLocatable()
{
}

SVGElement *SVGLocatable::nearestViewportElement(const SVGStyledElement *e)
{
    Node *n = e->parentNode();
    while (n && !n->isDocumentNode()) {
        if (n->hasTagName(SVGNames::svgTag) || n->hasTagName(SVGNames::symbolTag) ||
            n->hasTagName(SVGNames::imageTag) || n->hasTagName(SVGNames::foreignObjectTag))
            return static_cast<SVGElement *>(n);

        n = n->parentNode();
    }

    return 0;
}

SVGElement *SVGLocatable::farthestViewportElement(const SVGStyledElement *e)
{
    // FIXME : likely this will be always the <svg> farthest away.
    // If we have a different implementation of documentElement(), one
    // that give the documentElement() of the svg fragment, it could be
    // used instead. This depends on cdf demands though(Rob.)
    SVGElement *farthest = 0;
    Node *n = e->parentNode();
    while (n && !n->isDocumentNode()) {
        if (n->hasTagName(SVGNames::svgTag) || n->hasTagName(SVGNames::symbolTag) ||
            n->hasTagName(SVGNames::imageTag) || n->hasTagName(SVGNames::foreignObjectTag))
            farthest = static_cast<SVGElement *>(n);

        n = n->parentNode();
    }

    return farthest;
}

// Spec:
// http://www.w3.org/TR/2005/WD-SVGMobile12-20050413/svgudom.html#svg::SVGLocatable
FloatRect SVGLocatable::getBBox(const SVGStyledElement *e)
{
    FloatRect bboxRect;
    
    if (e && e->renderer()) {
        bboxRect = e->renderer()->relativeBBox(false);
        bboxRect.setSize(bboxRect.size() - FloatSize(1, 1)); // FIXME: Why -1 here?
    }

    return bboxRect;
}

SVGMatrix *SVGLocatable::getCTM(const SVGElement *element)
{
    if(!element)
        return 0;

    SVGMatrix *ctm = SVGSVGElement::createSVGMatrix();

    Node *parent = element->parentNode();
    if (parent && parent->isElementNode()) {
        SVGElement *parentElement = svg_dynamic_cast(parent);
        if(parentElement && parentElement->isStyledLocatable())
        {
            RefPtr<SVGMatrix> parentCTM = static_cast<SVGStyledLocatableElement *>(parentElement)->getCTM();
            ctm->multiply(parentCTM.get());
        }
    }

    return ctm;
}

SVGMatrix *SVGLocatable::getScreenCTM(const SVGElement *element)
{
    if(!element)
        return 0;

    SVGMatrix *ctm = SVGSVGElement::createSVGMatrix();

    Node *parent = element->parentNode();
    if(parent && parent->isElementNode())
    {
        SVGElement *parentElement = static_cast<SVGElement *>(parent);
        if(parentElement->isStyledLocatable())
        {
            RefPtr<SVGMatrix> parentCTM = static_cast<SVGStyledLocatableElement *>(parentElement)->getScreenCTM();
            ctm->multiply(parentCTM.get());
        }
    }

    return ctm;
}
/*
SVGMatrix *SVGLocatable::getTransformToElement(SVGElement *) const
{
    // TODO!
    return 0;
}
*/
// vim:ts=4:noet
#endif // SVG_SUPPORT

