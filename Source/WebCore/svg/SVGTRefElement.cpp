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
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "Text.h"
#include "XLinkNames.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGTRefElement, XLinkNames::hrefAttr, Href, href)

inline SVGTRefElement::SVGTRefElement(const QualifiedName& tagName, Document* document)
    : SVGTextPositioningElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::trefTag));
}

PassRefPtr<SVGTRefElement> SVGTRefElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGTRefElement(tagName, document));
}

void SVGTRefElement::updateReferencedText()
{
    Element* target = treeScope()->getElementById(SVGURIReference::getTarget(href()));
    String textContent;
    if (target && target->isSVGElement())
        textContent = static_cast<SVGElement*>(target)->textContent();
    ExceptionCode ignore = 0;
    setTextContent(textContent, ignore);
}

bool SVGTRefElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty())
        SVGURIReference::addSupportedAttributes(supportedAttributes);
    return supportedAttributes.contains(attrName);
}

void SVGTRefElement::parseMappedAttribute(Attribute* attr)
{
    if (!isSupportedAttribute(attr->name())) {
        SVGTextPositioningElement::parseMappedAttribute(attr);
        return;
    }

    if (SVGURIReference::parseMappedAttribute(attr)) {
        updateReferencedText();
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGTRefElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGTextPositioningElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (!renderer())
        return;

    if (SVGURIReference::isKnownAttribute(attrName)) {
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer());
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGTRefElement::synchronizeProperty(const QualifiedName& attrName)
{
    if (attrName == anyQName()) {
        synchronizeHref();
        SVGTextPositioningElement::synchronizeProperty(attrName);
        return;
    }

    if (!isSupportedAttribute(attrName)) {
        SVGTextPositioningElement::synchronizeProperty(attrName);
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        synchronizeHref();
        return;
    }

    ASSERT_NOT_REACHED();
}

AttributeToPropertyTypeMap& SVGTRefElement::attributeToPropertyTypeMap()
{
    DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, s_attributeToPropertyTypeMap, ());
    return s_attributeToPropertyTypeMap;
}

void SVGTRefElement::fillAttributeToPropertyTypeMap()
{
    AttributeToPropertyTypeMap& attributeToPropertyTypeMap = this->attributeToPropertyTypeMap();

    SVGTextPositioningElement::fillPassedAttributeToPropertyTypeMap(attributeToPropertyTypeMap);
    attributeToPropertyTypeMap.set(XLinkNames::hrefAttr, AnimatedString);
}

RenderObject* SVGTRefElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGInline(this);
}

bool SVGTRefElement::childShouldCreateRenderer(Node* child) const
{
    if (child->isTextNode())
        return true;

    return false;
}

bool SVGTRefElement::rendererIsNeeded(const NodeRenderingContext& context)
{
    if (parentNode()
        && (parentNode()->hasTagName(SVGNames::aTag)
#if ENABLE(SVG_FONTS)
            || parentNode()->hasTagName(SVGNames::altGlyphTag)
#endif
            || parentNode()->hasTagName(SVGNames::textTag)
            || parentNode()->hasTagName(SVGNames::textPathTag)
            || parentNode()->hasTagName(SVGNames::tspanTag)))
        return StyledElement::rendererIsNeeded(context);

    return false;
}

}

#endif // ENABLE(SVG)
