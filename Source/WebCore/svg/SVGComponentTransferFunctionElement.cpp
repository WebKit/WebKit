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

#include "SVGComponentTransferFunctionElementInlines.h"
#include "SVGFEComponentTransferElement.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGComponentTransferFunctionElement);

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::typeAttr, ComponentTransferType, &SVGComponentTransferFunctionElement::m_type>();
        PropertyRegistry::registerProperty<SVGNames::tableValuesAttr, &SVGComponentTransferFunctionElement::m_tableValues>();
        PropertyRegistry::registerProperty<SVGNames::slopeAttr, &SVGComponentTransferFunctionElement::m_slope>();
        PropertyRegistry::registerProperty<SVGNames::interceptAttr, &SVGComponentTransferFunctionElement::m_intercept>();
        PropertyRegistry::registerProperty<SVGNames::amplitudeAttr, &SVGComponentTransferFunctionElement::m_amplitude>();
        PropertyRegistry::registerProperty<SVGNames::exponentAttr, &SVGComponentTransferFunctionElement::m_exponent>();
        PropertyRegistry::registerProperty<SVGNames::offsetAttr, &SVGComponentTransferFunctionElement::m_offset>();
    });
}

void SVGComponentTransferFunctionElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::typeAttr) {
        ComponentTransferType propertyValue = SVGPropertyTraits<ComponentTransferType>::fromString(value);
        if (propertyValue > 0)
            m_type->setBaseValInternal<ComponentTransferType>(propertyValue);
        return;
    }

    if (name == SVGNames::tableValuesAttr) {
        m_tableValues->baseVal()->parse(value);
        return;
    }

    if (name == SVGNames::slopeAttr) {
        m_slope->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::interceptAttr) {
        m_intercept->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::amplitudeAttr) {
        m_amplitude->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::exponentAttr) {
        m_exponent->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::offsetAttr) {
        m_offset->setBaseValInternal(value.toFloat());
        return;
    }

    SVGElement::parseAttribute(name, value);
}

void SVGComponentTransferFunctionElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        RefPtr parent = parentElement();

        if (parent && is<SVGFEComponentTransferElement>(*parent)) {
            InstanceInvalidationGuard guard(*this);
            downcast<SVGFEComponentTransferElement>(*parent).transferFunctionAttributeChanged(*this, attrName);
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
