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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEMergeNodeElement.h"

#include "Attribute.h"
#include "RenderObject.h"
#include "RenderSVGResource.h"
#include "SVGElementInstance.h"
#include "SVGFilterElement.h"
#include "SVGNames.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEMergeNodeElement, SVGNames::inAttr, In1, in1)

inline SVGFEMergeNodeElement::SVGFEMergeNodeElement(const QualifiedName& tagName, Document* document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feMergeNodeTag));
}

PassRefPtr<SVGFEMergeNodeElement> SVGFEMergeNodeElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGFEMergeNodeElement(tagName, document));
}

bool SVGFEMergeNodeElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty())
        supportedAttributes.add(SVGNames::inAttr);
    return supportedAttributes.contains(attrName);
}

void SVGFEMergeNodeElement::parseMappedAttribute(Attribute* attr)
{
    if (!isSupportedAttribute(attr->name())) {
        SVGElement::parseMappedAttribute(attr);
        return;
    }

    if (attr->name() == SVGNames::inAttr) {
        setIn1BaseValue(attr->value());
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGFEMergeNodeElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);
    
    if (attrName == SVGNames::inAttr) {
        ContainerNode* parent = parentNode();
        if (!parent)
            return;

        RenderObject* renderer = parent->renderer();
        if (!renderer || !renderer->isSVGResourceFilterPrimitive())
            return;

        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    ASSERT_NOT_REACHED();
}

AttributeToPropertyTypeMap& SVGFEMergeNodeElement::attributeToPropertyTypeMap()
{
    DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, s_attributeToPropertyTypeMap, ());
    return s_attributeToPropertyTypeMap;
}

void SVGFEMergeNodeElement::fillAttributeToPropertyTypeMap()
{
    attributeToPropertyTypeMap().set(SVGNames::inAttr, AnimatedString);
}

void SVGFEMergeNodeElement::synchronizeProperty(const QualifiedName& attrName)
{
    if (attrName == anyQName()) {
        synchronizeIn1();
        SVGElement::synchronizeProperty(attrName);
        return;
    }

    if (!isSupportedAttribute(attrName)) {
        SVGElement::synchronizeProperty(attrName);
        return;
    }   

    if (attrName == SVGNames::inAttr) {
        synchronizeIn1();
        return;
    }

    ASSERT_NOT_REACHED();
}

}

#endif // ENABLE(SVG)
