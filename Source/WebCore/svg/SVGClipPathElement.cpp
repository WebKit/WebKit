/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
#include "SVGClipPathElement.h"

#include "Document.h"
#include "RenderSVGResourceClipper.h"
#include "SVGNames.h"
#include "StyleResolver.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_ENUMERATION(SVGClipPathElement, SVGNames::clipPathUnitsAttr, ClipPathUnits, clipPathUnits, SVGUnitTypes::SVGUnitType)
DEFINE_ANIMATED_BOOLEAN(SVGClipPathElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGClipPathElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(clipPathUnits)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGClipPathElement::SVGClipPathElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
    , m_clipPathUnits(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
{
    ASSERT(hasTagName(SVGNames::clipPathTag));
    registerAnimatedPropertiesForSVGClipPathElement();
}

Ref<SVGClipPathElement> SVGClipPathElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGClipPathElement(tagName, document));
}

bool SVGClipPathElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static NeverDestroyed<HashSet<QualifiedName>> supportedAttributes;
    if (supportedAttributes.get().isEmpty()) {
        SVGLangSpace::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.get().add(SVGNames::clipPathUnitsAttr);
    }
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGClipPathElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::clipPathUnitsAttr) {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setClipPathUnitsBaseValue(propertyValue);
        return;
    }

    SVGGraphicsElement::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGClipPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGraphicsElement::svgAttributeChanged(attrName);
        return;
    }

    InstanceInvalidationGuard guard(*this);

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

void SVGClipPathElement::childrenChanged(const ChildChange& change)
{
    SVGGraphicsElement::childrenChanged(change);

    if (change.source == ChildChangeSourceParser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

RenderPtr<RenderElement> SVGClipPathElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGResourceClipper>(*this, WTFMove(style));
}

}
