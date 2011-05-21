/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "SVGComponentTransferFunctionElement.h"

#include "Attribute.h"
#include "SVGFEComponentTransferElement.h"
#include "SVGNames.h"
#include "SVGNumberList.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_ENUMERATION(SVGComponentTransferFunctionElement, SVGNames::typeAttr, Type, type, ComponentTransferType)
DEFINE_ANIMATED_NUMBER_LIST(SVGComponentTransferFunctionElement, SVGNames::tableValuesAttr, TableValues, tableValues)
DEFINE_ANIMATED_NUMBER(SVGComponentTransferFunctionElement, SVGNames::slopeAttr, Slope, slope)
DEFINE_ANIMATED_NUMBER(SVGComponentTransferFunctionElement, SVGNames::interceptAttr, Intercept, intercept)
DEFINE_ANIMATED_NUMBER(SVGComponentTransferFunctionElement, SVGNames::amplitudeAttr, Amplitude, amplitude)
DEFINE_ANIMATED_NUMBER(SVGComponentTransferFunctionElement, SVGNames::exponentAttr, Exponent, exponent)
DEFINE_ANIMATED_NUMBER(SVGComponentTransferFunctionElement, SVGNames::offsetAttr, Offset, offset)

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(const QualifiedName& tagName, Document* document)
    : SVGElement(tagName, document)
    , m_type(FECOMPONENTTRANSFER_TYPE_IDENTITY)
    , m_slope(1)
    , m_amplitude(1)
    , m_exponent(1)
{
}

bool SVGComponentTransferFunctionElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::typeAttr);
        supportedAttributes.add(SVGNames::tableValuesAttr);
        supportedAttributes.add(SVGNames::slopeAttr);
        supportedAttributes.add(SVGNames::interceptAttr);
        supportedAttributes.add(SVGNames::amplitudeAttr);
        supportedAttributes.add(SVGNames::exponentAttr);
        supportedAttributes.add(SVGNames::offsetAttr);
    }
    return supportedAttributes.contains(attrName);
}

void SVGComponentTransferFunctionElement::parseMappedAttribute(Attribute* attr)
{
    if (!isSupportedAttribute(attr->name())) {
        SVGElement::parseMappedAttribute(attr);
        return;
    }

    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::typeAttr) {
        ComponentTransferType propertyValue = SVGPropertyTraits<ComponentTransferType>::fromString(value);
        if (propertyValue > 0)
            setTypeBaseValue(propertyValue);
        return;
    }

    if (attr->name() == SVGNames::tableValuesAttr) {
        SVGNumberList newList;
        newList.parse(value);
        detachAnimatedTableValuesListWrappers(newList.size());
        setTableValuesBaseValue(newList);
        return;
    }

    if (attr->name() == SVGNames::slopeAttr) {
        setSlopeBaseValue(value.toFloat());
        return;
    }

    if (attr->name() == SVGNames::interceptAttr) {
        setInterceptBaseValue(value.toFloat());
        return;
    }

    if (attr->name() == SVGNames::amplitudeAttr) {
        setAmplitudeBaseValue(value.toFloat());
        return;
    }

    if (attr->name() == SVGNames::exponentAttr) {
        setExponentBaseValue(value.toFloat());
        return;
    }

    if (attr->name() == SVGNames::offsetAttr) {
        setOffsetBaseValue(value.toFloat());
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGComponentTransferFunctionElement::synchronizeProperty(const QualifiedName& attrName)
{
    if (attrName == anyQName()) {
        synchronizeType();
        synchronizeTableValues();
        synchronizeSlope();
        synchronizeIntercept();
        synchronizeAmplitude();
        synchronizeExponent();
        synchronizeOffset();
        SVGElement::synchronizeProperty(attrName);
        return;
    }

    if (!isSupportedAttribute(attrName)) {
        SVGElement::synchronizeProperty(attrName);
        return;
    }

    if (attrName == SVGNames::typeAttr) {
        synchronizeType();
        return;
    }

    if (attrName == SVGNames::tableValuesAttr) {
        synchronizeTableValues();
        return;
    }

    if (attrName == SVGNames::slopeAttr) {
        synchronizeSlope();
        return;
    }

    if (attrName == SVGNames::interceptAttr) {
        synchronizeIntercept();
        return;
    }

    if (attrName == SVGNames::amplitudeAttr) {
        synchronizeAmplitude();
        return;
    }

    if (attrName == SVGNames::exponentAttr) {
        synchronizeExponent();
        return;
    }

    if (attrName == SVGNames::offsetAttr) {
        synchronizeOffset();
        return;
    }

    ASSERT_NOT_REACHED();
}

AttributeToPropertyTypeMap& SVGComponentTransferFunctionElement::attributeToPropertyTypeMap()
{
    DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, s_attributeToPropertyTypeMap, ());
    return s_attributeToPropertyTypeMap;
}

void SVGComponentTransferFunctionElement::fillAttributeToPropertyTypeMap()
{
    AttributeToPropertyTypeMap& attributeToPropertyTypeMap = this->attributeToPropertyTypeMap();
    attributeToPropertyTypeMap.set(SVGNames::typeAttr, AnimatedEnumeration);
    attributeToPropertyTypeMap.set(SVGNames::tableValuesAttr, AnimatedNumberList);
    attributeToPropertyTypeMap.set(SVGNames::slopeAttr, AnimatedNumber);
    attributeToPropertyTypeMap.set(SVGNames::interceptAttr, AnimatedNumber);
    attributeToPropertyTypeMap.set(SVGNames::amplitudeAttr, AnimatedNumber);
    attributeToPropertyTypeMap.set(SVGNames::exponentAttr, AnimatedNumber);
    attributeToPropertyTypeMap.set(SVGNames::offsetAttr, AnimatedNumber);
}

ComponentTransferFunction SVGComponentTransferFunctionElement::transferFunction() const
{
    ComponentTransferFunction func;
    func.type = type();
    func.slope = slope();
    func.intercept = intercept();
    func.amplitude = amplitude();
    func.exponent = exponent();
    func.offset = offset();
    func.tableValues = tableValues();
    return func;
}

}

#endif // ENABLE(SVG)
