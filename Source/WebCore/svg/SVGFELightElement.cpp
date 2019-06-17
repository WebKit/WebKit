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

void SVGFELightElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::azimuthAttr) {
        m_azimuth->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::elevationAttr) {
        m_elevation->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::xAttr) {
        m_x->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::yAttr) {
        m_y->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::zAttr) {
        m_z->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::pointsAtXAttr) {
        m_pointsAtX->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::pointsAtYAttr) {
        m_pointsAtY->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::pointsAtZAttr) {
        m_pointsAtZ->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::specularExponentAttr) {
        m_specularExponent->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::limitingConeAngleAttr) {
        m_limitingConeAngle->setBaseValInternal(value.toFloat());
        return;
    }

    SVGElement::parseAttribute(name, value);
}

void SVGFELightElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
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
