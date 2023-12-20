/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Oliver Hunt <oliver@nerget.com>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "ElementChildIteratorInlines.h"
#include "LegacyRenderSVGResource.h"
#include "NodeName.h"
#include "RenderObject.h"
#include "SVGElementTypeHelpers.h"
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
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::azimuthAttr, &SVGFELightElement::m_azimuth>();
        PropertyRegistry::registerProperty<SVGNames::elevationAttr, &SVGFELightElement::m_elevation>();
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGFELightElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGFELightElement::m_y>();
        PropertyRegistry::registerProperty<SVGNames::zAttr, &SVGFELightElement::m_z>();
        PropertyRegistry::registerProperty<SVGNames::pointsAtXAttr, &SVGFELightElement::m_pointsAtX>();
        PropertyRegistry::registerProperty<SVGNames::pointsAtYAttr, &SVGFELightElement::m_pointsAtY>();
        PropertyRegistry::registerProperty<SVGNames::pointsAtZAttr, &SVGFELightElement::m_pointsAtZ>();
        PropertyRegistry::registerProperty<SVGNames::specularExponentAttr, &SVGFELightElement::m_specularExponent>();
        PropertyRegistry::registerProperty<SVGNames::limitingConeAngleAttr, &SVGFELightElement::m_limitingConeAngle>();
    });
}

SVGFELightElement* SVGFELightElement::findLightElement(const SVGElement* svgElement)
{
    for (auto& child : childrenOfType<SVGElement>(*svgElement)) {
        if (is<SVGFEDistantLightElement>(child) || is<SVGFEPointLightElement>(child) || is<SVGFESpotLightElement>(child))
            return static_cast<SVGFELightElement*>(const_cast<SVGElement*>(&child));
    }
    return nullptr;
}

void SVGFELightElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::azimuthAttr:
        m_azimuth->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::elevationAttr:
        m_elevation->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::xAttr:
        m_x->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::yAttr:
        m_y->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::zAttr:
        m_z->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::pointsAtXAttr:
        m_pointsAtX->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::pointsAtYAttr:
        m_pointsAtY->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::pointsAtZAttr:
        m_pointsAtZ->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::specularExponentAttr:
        m_specularExponent->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::limitingConeAngleAttr:
        m_limitingConeAngle->setBaseValInternal(newValue.toFloat());
        break;
    default:
        break;
    }

    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGFELightElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        ASSERT(attrName == SVGNames::azimuthAttr || attrName == SVGNames::elevationAttr || attrName == SVGNames::xAttr || attrName == SVGNames::yAttr
            || attrName == SVGNames::zAttr || attrName == SVGNames::pointsAtXAttr || attrName == SVGNames::pointsAtYAttr || attrName == SVGNames::pointsAtZAttr
            || attrName == SVGNames::specularExponentAttr || attrName == SVGNames::limitingConeAngleAttr);

        RefPtr parent = parentElement();
        if (!parent)
            return;

        auto* renderer = parent->renderer();
        if (!renderer || !renderer->isRenderSVGResourceFilterPrimitive())
            return;

        if (auto* lightingElement = dynamicDowncast<SVGFEDiffuseLightingElement>(*parent)) {
            InstanceInvalidationGuard guard(*this);
            lightingElement->lightElementAttributeChanged(this, attrName);
        } else if (auto* lightingElement = dynamicDowncast<SVGFESpecularLightingElement>(*parent)) {
            InstanceInvalidationGuard guard(*this);
            lightingElement->lightElementAttributeChanged(this, attrName);
        }

        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

void SVGFELightElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChange::Source::Parser)
        return;

    SVGFilterPrimitiveStandardAttributes::invalidateFilterPrimitiveParent(this);
}

}
