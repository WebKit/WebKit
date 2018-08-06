/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Oliver Hunt <oliver@nerget.com>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGFELightElement.h"

#include "ElementIterator.h"
#include "RenderObject.h"
#include "RenderSVGResource.h"
#include "SVGFEDiffuseLightingElement.h"
#include "SVGFEDistantLightElement.h"
#include "SVGFEPointLightElement.h"
#include "SVGFESpecularLightingElement.h"
#include "SVGFESpotLightElement.h"
#include "SVGFilterElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFELightElement);

SVGFELightElement::SVGFELightElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    registerAttributes();
}

SVGFELightElement* SVGFELightElement::findLightElement(const SVGElement* svgElement)
{
    for (auto& child : childrenOfType<SVGElement>(*svgElement)) {
        if (is<SVGFEDistantLightElement>(child) || is<SVGFEPointLightElement>(child) || is<SVGFESpotLightElement>(child))
            return static_cast<SVGFELightElement*>(const_cast<SVGElement*>(&child));
    }
    return nullptr;
}

void SVGFELightElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::azimuthAttr, &SVGFELightElement::m_azimuth>();
    registry.registerAttribute<SVGNames::elevationAttr, &SVGFELightElement::m_elevation>();
    registry.registerAttribute<SVGNames::xAttr, &SVGFELightElement::m_x>();
    registry.registerAttribute<SVGNames::yAttr, &SVGFELightElement::m_y>();
    registry.registerAttribute<SVGNames::zAttr, &SVGFELightElement::m_z>();
    registry.registerAttribute<SVGNames::pointsAtXAttr, &SVGFELightElement::m_pointsAtX>();
    registry.registerAttribute<SVGNames::pointsAtYAttr, &SVGFELightElement::m_pointsAtY>();
    registry.registerAttribute<SVGNames::pointsAtZAttr, &SVGFELightElement::m_pointsAtZ>();
    registry.registerAttribute<SVGNames::specularExponentAttr, &SVGFELightElement::m_specularExponent>();
    registry.registerAttribute<SVGNames::limitingConeAngleAttr, &SVGFELightElement::m_limitingConeAngle>();
}

void SVGFELightElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::azimuthAttr) {
        m_azimuth.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::elevationAttr) {
        m_elevation.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::xAttr) {
        m_x.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::yAttr) {
        m_y.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::zAttr) {
        m_z.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::pointsAtXAttr) {
        m_pointsAtX.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::pointsAtYAttr) {
        m_pointsAtY.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::pointsAtZAttr) {
        m_pointsAtZ.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::specularExponentAttr) {
        m_specularExponent.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::limitingConeAngleAttr) {
        m_limitingConeAngle.setValue(value.toFloat());
        return;
    }

    SVGElement::parseAttribute(name, value);
}

void SVGFELightElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (isKnownAttribute(attrName)) {
        auto parent = makeRefPtr(parentElement());
        if (!parent)
            return;

        auto* renderer = parent->renderer();
        if (!renderer || !renderer->isSVGResourceFilterPrimitive())
            return;

        if (is<SVGFEDiffuseLightingElement>(*parent)) {
            InstanceInvalidationGuard guard(*this);
            downcast<SVGFEDiffuseLightingElement>(*parent).lightElementAttributeChanged(this, attrName);
        } else if (is<SVGFESpecularLightingElement>(*parent)) {
            InstanceInvalidationGuard guard(*this);
            downcast<SVGFESpecularLightingElement>(*parent).lightElementAttributeChanged(this, attrName);
        }

        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

void SVGFELightElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChangeSource::Parser)
        return;
    auto parent = makeRefPtr(parentNode());
    if (!parent)
        return;
    RenderElement* renderer = parent->renderer();
    if (renderer && renderer->isSVGResourceFilterPrimitive())
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
}

}
