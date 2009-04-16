/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
    Copyright (C) 2009 Google, Inc.  All rights reserved.

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

static bool isViewportElement(Node* node)
{
    return (node->hasTagName(SVGNames::svgTag)
        || node->hasTagName(SVGNames::symbolTag)
#if ENABLE(SVG_FOREIGN_OBJECT)
        || node->hasTagName(SVGNames::foreignObjectTag)
#endif
        || node->hasTagName(SVGNames::imageTag));
}

SVGElement* SVGLocatable::nearestViewportElement(const SVGElement* element)
{
    ASSERT(element);
    for (Node* n = element->parentNode(); n && !n->isDocumentNode(); n = n->parentNode()) {
        if (isViewportElement(n))
            return static_cast<SVGElement*>(n);
    }

    return 0;
}

SVGElement* SVGLocatable::farthestViewportElement(const SVGElement* element)
{
    ASSERT(element);
    SVGElement* farthest = 0;
    for (Node* n = element->parentNode(); n && !n->isDocumentNode(); n = n->parentNode()) {
        if (isViewportElement(n))
            farthest = static_cast<SVGElement*>(n);
    }
    return farthest;
}

FloatRect SVGLocatable::getBBox(const SVGElement* element)
{
    element->document()->updateLayoutIgnorePendingStylesheets();

    // FIXME: Eventually we should support getBBox for detached elements.
    if (!element->renderer())
        return FloatRect();

    return element->renderer()->objectBoundingBox();
}

TransformationMatrix SVGLocatable::getCTM(const SVGElement* element)
{
    ASSERT(element);
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
    ASSERT(element);
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
