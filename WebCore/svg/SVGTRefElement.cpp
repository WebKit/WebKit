/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG)
#include "SVGTRefElement.h"

#include "RenderSVGInline.h"
#include "RenderSVGResource.h"
#include "SVGDocument.h"
#include "SVGNames.h"
#include "Text.h"
#include "XLinkNames.h"

namespace WebCore {

inline SVGTRefElement::SVGTRefElement(const QualifiedName& tagName, Document* document)
    : SVGTextPositioningElement(tagName, document)
{
}

PassRefPtr<SVGTRefElement> SVGTRefElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGTRefElement(tagName, document));
}

void SVGTRefElement::updateReferencedText()
{
    Element* target = document()->getElementById(SVGURIReference::getTarget(href()));
    String textContent;
    if (target && target->isSVGElement())
        textContent = static_cast<SVGElement*>(target)->textContent();
    ExceptionCode ignore = 0;
    setTextContent(textContent, ignore);
}

void SVGTRefElement::parseMappedAttribute(Attribute* attr)
{
    if (SVGURIReference::parseMappedAttribute(attr)) {
        updateReferencedText();
        return;
    }

    SVGTextPositioningElement::parseMappedAttribute(attr);
}

void SVGTRefElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGTextPositioningElement::svgAttributeChanged(attrName);

    if (!renderer())
        return;

    if (SVGURIReference::isKnownAttribute(attrName))
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer());
}

void SVGTRefElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGTextPositioningElement::synchronizeProperty(attrName);

    if (attrName == anyQName() || SVGURIReference::isKnownAttribute(attrName))
        synchronizeHref();
}

bool SVGTRefElement::childShouldCreateRenderer(Node* child) const
{
    if (child->isTextNode() || child->hasTagName(SVGNames::tspanTag) ||
        child->hasTagName(SVGNames::trefTag))
        return true;
    return false;
}

RenderObject* SVGTRefElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGInline(this);
}

}

#endif // ENABLE(SVG)
