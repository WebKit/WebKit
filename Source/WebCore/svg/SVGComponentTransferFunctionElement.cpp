/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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
#include "SVGComponentTransferFunctionElement.h"

#include "NodeName.h"
#include "SVGComponentTransferFunctionElementInlines.h"
#include "SVGFEComponentTransferElement.h"
#include "SVGNames.h"
#include "SVGPropertyOwnerRegistry.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGComponentTransferFunctionElement);

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::typeAttr, ComponentTransferType, &SVGComponentTransferFunctionElement::m_type>();
        PropertyRegistry::registerProperty<SVGNames::tableValuesAttr, &SVGComponentTransferFunctionElement::m_tableValues>();
        PropertyRegistry::registerProperty<SVGNames::slopeAttr, &SVGComponentTransferFunctionElement::m_slope>();
        PropertyRegistry::registerProperty<SVGNames::interceptAttr, &SVGComponentTransferFunctionElement::m_intercept>();
        PropertyRegistry::registerProperty<SVGNames::amplitudeAttr, &SVGComponentTransferFunctionElement::m_amplitude>();
        PropertyRegistry::registerProperty<SVGNames::exponentAttr, &SVGComponentTransferFunctionElement::m_exponent>();
        PropertyRegistry::registerProperty<SVGNames::offsetAttr, &SVGComponentTransferFunctionElement::m_offset>();
    };
}

void SVGComponentTransferFunctionElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::typeAttr: {
        ComponentTransferType propertyValue = SVGPropertyTraits<ComponentTransferType>::fromString(*this, newValue);
        if (enumToUnderlyingType(propertyValue))
            m_type->setBaseValInternal<ComponentTransferType>(propertyValue);
        break;
    }
    case AttributeNames::tableValuesAttr:
        m_tableValues->baseVal()->parse(newValue);
        break;
    case AttributeNames::slopeAttr:
        m_slope->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::interceptAttr:
        m_intercept->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::amplitudeAttr:
        m_amplitude->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::exponentAttr:
        m_exponent->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::offsetAttr:
        m_offset->setBaseValInternal(newValue.toFloat());
        break;
    default:
        break;
    }

    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGComponentTransferFunctionElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        if (RefPtr transferElement = dynamicDowncast<SVGFEComponentTransferElement>(parentElement())) {
            InstanceInvalidationGuard guard(*this);
            transferElement->transferFunctionAttributeChanged(*this, attrName);
        }
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

ComponentTransferFunction SVGComponentTransferFunctionElement::transferFunction() const
{
    return {
        type(),
        slope(),
        intercept(),
        amplitude(),
        exponent(),
        offset(),
        tableValues()
    };
}

}
