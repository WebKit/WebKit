/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "SVGFilterElement.h"

#include "Attr.h"
#include "RenderSVGResourceFilter.h"
#include "SVGElementInstance.h"
#include "SVGFilterBuilder.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_ENUMERATION(SVGFilterElement, SVGNames::filterUnitsAttr, FilterUnits, filterUnits, SVGUnitTypes::SVGUnitType)
DEFINE_ANIMATED_ENUMERATION(SVGFilterElement, SVGNames::primitiveUnitsAttr, PrimitiveUnits, primitiveUnits, SVGUnitTypes::SVGUnitType)
DEFINE_ANIMATED_LENGTH(SVGFilterElement, SVGNames::xAttr, X, x)
DEFINE_ANIMATED_LENGTH(SVGFilterElement, SVGNames::yAttr, Y, y)
DEFINE_ANIMATED_LENGTH(SVGFilterElement, SVGNames::widthAttr, Width, width)
DEFINE_ANIMATED_LENGTH(SVGFilterElement, SVGNames::heightAttr, Height, height)
DEFINE_ANIMATED_INTEGER_MULTIPLE_WRAPPERS(SVGFilterElement, SVGNames::filterResAttr, filterResXIdentifier(), FilterResX, filterResX)
DEFINE_ANIMATED_INTEGER_MULTIPLE_WRAPPERS(SVGFilterElement, SVGNames::filterResAttr, filterResYIdentifier(), FilterResY, filterResY)
DEFINE_ANIMATED_STRING(SVGFilterElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGFilterElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

inline SVGFilterElement::SVGFilterElement(const QualifiedName& tagName, Document* document)
    : SVGStyledElement(tagName, document)
    , SVGURIReference()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_filterUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_primitiveUnits(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
    , m_x(LengthModeWidth, "-10%")
    , m_y(LengthModeHeight, "-10%")
    , m_width(LengthModeWidth, "120%")
    , m_height(LengthModeHeight, "120%")
{
    // Spec: If the x/y attribute is not specified, the effect is as if a value of "-10%" were specified.
    // Spec: If the width/height attribute is not specified, the effect is as if a value of "120%" were specified.
    ASSERT(hasTagName(SVGNames::filterTag));
}

PassRefPtr<SVGFilterElement> SVGFilterElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGFilterElement(tagName, document));
}

const AtomicString& SVGFilterElement::filterResXIdentifier()
{
    DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGFilterResX"));
    return s_identifier;
}

const AtomicString& SVGFilterElement::filterResYIdentifier()
{
    DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGFilterResY"));
    return s_identifier;
}

void SVGFilterElement::setFilterRes(unsigned long filterResX, unsigned long filterResY)
{
    setFilterResXBaseValue(filterResX);
    setFilterResYBaseValue(filterResY);

    if (RenderObject* object = renderer())
        object->setNeedsLayout(true);
}

bool SVGFilterElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        SVGLangSpace::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::filterUnitsAttr);
        supportedAttributes.add(SVGNames::primitiveUnitsAttr);
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
        supportedAttributes.add(SVGNames::widthAttr);
        supportedAttributes.add(SVGNames::heightAttr);
        supportedAttributes.add(SVGNames::filterResAttr);
    }
    return supportedAttributes.contains(attrName);
}

void SVGFilterElement::parseMappedAttribute(Attribute* attr)
{
    if (!isSupportedAttribute(attr->name())) {
        SVGStyledElement::parseMappedAttribute(attr);
        return;
    }

    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::filterUnitsAttr) {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setFilterUnitsBaseValue(propertyValue);
        return;
    }

    if (attr->name() == SVGNames::primitiveUnitsAttr) {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setPrimitiveUnitsBaseValue(propertyValue);
        return;
    }

    if (attr->name() == SVGNames::xAttr) {
        setXBaseValue(SVGLength(LengthModeWidth, value));
        return;
    }

    if (attr->name() == SVGNames::yAttr) {
        setYBaseValue(SVGLength(LengthModeHeight, value));
        return;
    }

    if (attr->name() == SVGNames::widthAttr) {
        setWidthBaseValue(SVGLength(LengthModeWidth, value));
        return;
    }

    if (attr->name() == SVGNames::heightAttr) {
        setHeightBaseValue(SVGLength(LengthModeHeight, value));
        return;
    }

    if (attr->name() == SVGNames::filterResAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setFilterResXBaseValue(x);
            setFilterResYBaseValue(y);
        }
        return;
    }

    if (SVGURIReference::parseMappedAttribute(attr))
        return;
    if (SVGLangSpace::parseMappedAttribute(attr))
        return;
    if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
        return;

    ASSERT_NOT_REACHED();
}

void SVGFilterElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGStyledElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);
    
    if (attrName == SVGNames::xAttr
        || attrName == SVGNames::yAttr
        || attrName == SVGNames::widthAttr
        || attrName == SVGNames::heightAttr)
        updateRelativeLengthsInformation();

    if (RenderObject* object = renderer())
        object->setNeedsLayout(true);
}

void SVGFilterElement::synchronizeProperty(const QualifiedName& attrName)
{
    if (attrName == anyQName()) {
        synchronizeX();
        synchronizeY();
        synchronizeWidth();
        synchronizeHeight();
        synchronizeFilterUnits();
        synchronizePrimitiveUnits();
        synchronizeFilterResX();
        synchronizeFilterResY();
        synchronizeExternalResourcesRequired();
        synchronizeHref();
        SVGStyledElement::synchronizeProperty(attrName);
        return;
    }

    if (!isSupportedAttribute(attrName)) {
        SVGStyledElement::synchronizeProperty(attrName);
        return;
    }

    if (attrName == SVGNames::xAttr) {
        synchronizeX();
        return;
    }

    if (attrName == SVGNames::yAttr) {
        synchronizeY();
        return;
    }

    if (attrName == SVGNames::widthAttr) {
        synchronizeWidth();
        return;
    }

    if (attrName == SVGNames::heightAttr) {
        synchronizeHeight();
        return;
    }

    if (attrName == SVGNames::filterUnitsAttr) {
        synchronizeFilterUnits();
        return;
    }

    if (attrName == SVGNames::primitiveUnitsAttr) {
        synchronizePrimitiveUnits();
        return;
    }

    if (attrName == SVGNames::filterResAttr) {
        synchronizeFilterResX();
        synchronizeFilterResY();
        return;
    }

    if (SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        synchronizeExternalResourcesRequired();
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        synchronizeHref();
        return;
    }

    ASSERT_NOT_REACHED();
}

AttributeToPropertyTypeMap& SVGFilterElement::attributeToPropertyTypeMap()
{
    DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, s_attributeToPropertyTypeMap, ());
    return s_attributeToPropertyTypeMap;
}

void SVGFilterElement::fillAttributeToPropertyTypeMap()
{
    AttributeToPropertyTypeMap& attributeToPropertyTypeMap = this->attributeToPropertyTypeMap();

    SVGStyledElement::fillPassedAttributeToPropertyTypeMap(attributeToPropertyTypeMap);
    attributeToPropertyTypeMap.set(SVGNames::filterUnitsAttr, AnimatedEnumeration);
    attributeToPropertyTypeMap.set(SVGNames::primitiveUnitsAttr, AnimatedEnumeration);
    attributeToPropertyTypeMap.set(SVGNames::xAttr, AnimatedLength);
    attributeToPropertyTypeMap.set(SVGNames::yAttr, AnimatedLength);
    attributeToPropertyTypeMap.set(SVGNames::widthAttr, AnimatedLength);
    attributeToPropertyTypeMap.set(SVGNames::heightAttr, AnimatedLength);
    attributeToPropertyTypeMap.set(SVGNames::filterResAttr, AnimatedNumberOptionalNumber);
    attributeToPropertyTypeMap.set(XLinkNames::hrefAttr, AnimatedString);
}

void SVGFilterElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGStyledElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (changedByParser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayout(true);
}

FloatRect SVGFilterElement::filterBoundingBox(const FloatRect& objectBoundingBox) const
{
    FloatRect filterBBox;
    if (filterUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        filterBBox = FloatRect(x().valueAsPercentage() * objectBoundingBox.width() + objectBoundingBox.x(),
                               y().valueAsPercentage() * objectBoundingBox.height() + objectBoundingBox.y(),
                               width().valueAsPercentage() * objectBoundingBox.width(),
                               height().valueAsPercentage() * objectBoundingBox.height());
    else
        filterBBox = FloatRect(x().value(this),
                               y().value(this),
                               width().value(this),
                               height().value(this));

    return filterBBox;
}

RenderObject* SVGFilterElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGResourceFilter(this);
}

bool SVGFilterElement::selfHasRelativeLengths() const
{
    return x().isRelative()
        || y().isRelative()
        || width().isRelative()
        || height().isRelative();
}

}

#endif
