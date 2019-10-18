/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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
#include "SVGExternalResourcesRequired.h"

#include "RenderSVGResource.h"
#include "RenderSVGShape.h"
#include "SVGElement.h"
#include "SVGNames.h"

namespace WebCore {

SVGExternalResourcesRequired::SVGExternalResourcesRequired(SVGElement* contextElement)
    : m_contextElement(*contextElement)
    , m_externalResourcesRequired(SVGAnimatedBoolean::create(contextElement))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::externalResourcesRequiredAttr, &SVGExternalResourcesRequired::m_externalResourcesRequired>();
    });
}

void SVGExternalResourcesRequired::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::externalResourcesRequiredAttr)
        m_externalResourcesRequired->setBaseValInternal(value == "true");
}

void SVGExternalResourcesRequired::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isKnownAttribute(attrName))
        return;
    if (!m_contextElement.isConnected())
        return;

    auto* renderer = m_contextElement.renderer();
    if (renderer && is<RenderSVGShape>(renderer)) {
        SVGElement::InstanceInvalidationGuard guard(m_contextElement);
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
    }
}

void SVGExternalResourcesRequired::addSupportedAttributes(HashSet<QualifiedName>& supportedAttributes)
{
    supportedAttributes.add(SVGNames::externalResourcesRequiredAttr);
}

}
