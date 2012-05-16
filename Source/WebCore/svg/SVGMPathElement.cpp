/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
#include "SVGMPathElement.h"

#include "Document.h"
#include "SVGNames.h"
#include "SVGPathElement.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGMPathElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGMPathElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGMPathElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGMPathElement::SVGMPathElement(const QualifiedName& tagName, Document* document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::mpathTag));
    registerAnimatedPropertiesForSVGMPathElement();
}

PassRefPtr<SVGMPathElement> SVGMPathElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGMPathElement(tagName, document));
}

bool SVGMPathElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
    }
    return supportedAttributes.contains<QualifiedName, SVGAttributeHashTranslator>(attrName);
}

void SVGMPathElement::parseAttribute(const Attribute& attribute)
{
    if (!isSupportedAttribute(attribute.name())) {
        SVGElement::parseAttribute(attribute);
        return;
    }

    if (SVGURIReference::parseAttribute(attribute))
        return;
    if (SVGExternalResourcesRequired::parseAttribute(attribute))
        return;

    ASSERT_NOT_REACHED();
}

SVGPathElement* SVGMPathElement::pathElement()
{
    Element* target = targetElementFromIRIString(href(), document());
    if (target && target->hasTagName(SVGNames::pathTag))
        return static_cast<SVGPathElement*>(target);
    return 0;
}

} // namespace WebCore

#endif // ENABLE(SVG)
