/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#if ENABLE(SVG)

#include "SVGLocatable.h"

#include "TransformationMatrix.h"
#include "RenderPath.h"
#include "SVGException.h"
#include "SVGSVGElement.h"

namespace WebCore {

SVGLocatable::SVGLocatable()
{
}

SVGLocatable::~SVGLocatable()
{
}

SVGElement* SVGLocatable::nearestViewportElement(const SVGElement* e)
{
    Node* n = e->parentNode();
    while (n && !n->isDocumentNode()) {
        if (n->hasTagName(SVGNames::svgTag) || n->hasTagName(SVGNames::symbolTag) ||
            n->hasTagName(SVGNames::imageTag))
            return static_cast<SVGElement*>(n);
#if ENABLE(SVG_FOREIGN_OBJECT)
        if (n->hasTagName(SVGNames::foreignObjectTag))
            return static_cast<SVGElement*>(n);
#endif

        n = n->parentNode();
    }

    return 0;
}

SVGElement* SVGLocatable::farthestViewportElement(const SVGElement* e)
{
    // FIXME : likely this will be always the <svg> farthest away.
    // If we have a different implementation of documentElement(), one
    // that give the documentElement() of the svg fragment, it could be
    // used instead. This depends on cdf demands though(Rob.)
    SVGElement* farthest = 0;
    Node* n = e->parentNode();
    while (n && !n->isDocumentNode()) {
        if (n->hasTagName(SVGNames::svgTag) || n->hasTagName(SVGNames::symbolTag) ||
            n->hasTagName(SVGNames::imageTag))
            farthest = static_cast<SVGElement*>(n);
#if ENABLE(SVG_FOREIGN_OBJECT)
        if (n->hasTagName(SVGNames::foreignObjectTag))
            farthest = static_cast<SVGElement*>(n);
#endif

        n = n->parentNode();
    }

    return farthest;
}

// Spec:
// http://www.w3.org/TR/2005/WD-SVGMobile12-20050413/svgudom.html#svg::SVGLocatable
FloatRect SVGLocatable::getBBox(const SVGElement* e)
{
    FloatRect bboxRect;

    e->document()->updateLayoutIgnorePendingStylesheets();

    if (e && e->renderer()) {
        // Need this to make sure we have render object dimensions.
        // See bug 11686.
        bboxRect = e->renderer()->relativeBBox(false);
    }

    return bboxRect;
}

TransformationMatrix SVGLocatable::getCTM(const SVGElement* element)
{
    if (!element)
        return TransformationMatrix();

    TransformationMatrix ctm;

    Node* parent = element->parentNode();
    if (parent && parent->isSVGElement()) {
        SVGElement* parentElement = static_cast<SVGElement*>(parent);
        if (parentElement && parentElement->isStyledLocatable()) {
            TransformationMatrix parentCTM = static_cast<SVGStyledLocatableElement*>(parentElement)->getCTM();
            ctm = parentCTM * ctm;
        }
    }

    return ctm;
}

TransformationMatrix SVGLocatable::getScreenCTM(const SVGElement* element)
{
    if (!element)
        return TransformationMatrix();

    TransformationMatrix ctm;

    Node* parent = element->parentNode();
    if (parent && parent->isSVGElement()) {
        SVGElement* parentElement = static_cast<SVGElement*>(parent);
        if (parentElement && parentElement->isStyledLocatable()) {
            TransformationMatrix parentCTM = static_cast<SVGStyledLocatableElement*>(parentElement)->getScreenCTM();
            ctm = parentCTM * ctm;
        }
    }

    return ctm;
}

TransformationMatrix SVGLocatable::getTransformToElement(SVGElement* target, ExceptionCode& ec) const
{
    TransformationMatrix ctm = getCTM();

    if (target && target->isStyledLocatable()) {
        TransformationMatrix targetCTM = static_cast<SVGStyledLocatableElement*>(target)->getCTM();
        if (!targetCTM.isInvertible()) {
            ec = SVGException::SVG_MATRIX_NOT_INVERTABLE;
            return ctm;
        }
        ctm *= targetCTM.inverse();
    }

    return ctm;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
