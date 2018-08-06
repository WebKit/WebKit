/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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
#include "SVGComponentTransferFunctionElement.h"

#include "SVGFEComponentTransferElement.h"
#include "SVGNames.h"
#include "SVGNumberListValues.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGComponentTransferFunctionElement);

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    registerAttributes();
}

void SVGComponentTransferFunctionElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::typeAttr, ComponentTransferType, &SVGComponentTransferFunctionElement::m_type>();
    registry.registerAttribute<SVGNames::tableValuesAttr, &SVGComponentTransferFunctionElement::m_tableValues>();
    registry.registerAttribute<SVGNames::slopeAttr, &SVGComponentTransferFunctionElement::m_slope>();
    registry.registerAttribute<SVGNames::interceptAttr, &SVGComponentTransferFunctionElement::m_intercept>();
    registry.registerAttribute<SVGNames::amplitudeAttr, &SVGComponentTransferFunctionElement::m_amplitude>();
    registry.registerAttribute<SVGNames::exponentAttr, &SVGComponentTransferFunctionElement::m_exponent>();
    registry.registerAttribute<SVGNames::offsetAttr, &SVGComponentTransferFunctionElement::m_offset>();
}

void SVGComponentTransferFunctionElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::typeAttr) {
        ComponentTransferType propertyValue = SVGPropertyTraits<ComponentTransferType>::fromString(value);
        if (propertyValue > 0)
            m_type.setValue(propertyValue);
        return;
    }

    if (name == SVGNames::tableValuesAttr) {
        SVGNumberListValues newList;
        newList.parse(value);
        m_tableValues.detachAnimatedListWrappers(attributeOwnerProxy(), newList.size());
        m_tableValues.setValue(WTFMove(newList));
        return;
    }

    if (name == SVGNames::slopeAttr) {
        m_slope.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::interceptAttr) {
        m_intercept.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::amplitudeAttr) {
        m_amplitude.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::exponentAttr) {
        m_exponent.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::offsetAttr) {
        m_offset.setValue(value.toFloat());
        return;
    }

    SVGElement::parseAttribute(name, value);
}

void SVGComponentTransferFunctionElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        invalidateFilterPrimitiveParent(this);
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
