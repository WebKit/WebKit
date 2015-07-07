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

#if ENABLE(FILTERS)
#include "SVGFilterElement.h"

#include "Attr.h"
#include "RenderSVGResourceFilter.h"
#include "SVGElementInstance.h"
#include "SVGFilterBuilder.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "XLinkNames.h"
#include <wtf/NeverDestroyed.h>

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

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFilterElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(filterUnits)
    REGISTER_LOCAL_ANIMATED_PROPERTY(primitiveUnits)
    REGISTER_LOCAL_ANIMATED_PROPERTY(x)
    REGISTER_LOCAL_ANIMATED_PROPERTY(y)
    REGISTER_LOCAL_ANIMATED_PROPERTY(width)
    REGISTER_LOCAL_ANIMATED_PROPERTY(height)
    REGISTER_LOCAL_ANIMATED_PROPERTY(filterResX)
    REGISTER_LOCAL_ANIMATED_PROPERTY(filterResY)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFilterElement::SVGFilterElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
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
    registerAnimatedPropertiesForSVGFilterElement();
}

PassRefPtr<SVGFilterElement> SVGFilterElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new SVGFilterElement(tagName, document));
}

const AtomicString& SVGFilterElement::filterResXIdentifier()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGFilterResX", AtomicString::ConstructFromLiteral));
    return s_identifier;
}

const AtomicString& SVGFilterElement::filterResYIdentifier()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGFilterResY", AtomicString::ConstructFromLiteral));
    return s_identifier;
}

void SVGFilterElement::setFilterRes(unsigned filterResX, unsigned filterResY)
{
    setFilterResXBaseValue(filterResX);
    setFilterResYBaseValue(filterResY);

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

bool SVGFilterElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static NeverDestroyed<HashSet<QualifiedName>> supportedAttributes;
    if (supportedAttributes.get().isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        SVGLangSpace::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.get().add(SVGNames::filterUnitsAttr);
        supportedAttributes.get().add(SVGNames::primitiveUnitsAttr);
        supportedAttributes.get().add(SVGNames::xAttr);
        supportedAttributes.get().add(SVGNames::yAttr);
        supportedAttributes.get().add(SVGNames::widthAttr);
        supportedAttributes.get().add(SVGNames::heightAttr);
        supportedAttributes.get().add(SVGNames::filterResAttr);
    }
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFilterElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGElement::parseAttribute(name, value);
    else if (name == SVGNames::filterUnitsAttr) {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setFilterUnitsBaseValue(propertyValue);
    } else if (name == SVGNames::primitiveUnitsAttr) {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setPrimitiveUnitsBaseValue(propertyValue);
    } else if (name == SVGNames::xAttr)
        setXBaseValue(SVGLength::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::yAttr)
        setYBaseValue(SVGLength::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::filterResAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setFilterResXBaseValue(x);
            setFilterResYBaseValue(y);
        }
    } else if (SVGURIReference::parseAttribute(name, value)
             || SVGLangSpace::parseAttribute(name, value)
             || SVGExternalResourcesRequired::parseAttribute(name, value)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGFilterElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);
    
    if (attrName == SVGNames::xAttr
        || attrName == SVGNames::yAttr
        || attrName == SVGNames::widthAttr
        || attrName == SVGNames::heightAttr)
        updateRelativeLengthsInformation();

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

void SVGFilterElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChangeSourceParser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

RenderPtr<RenderElement> SVGFilterElement::createElementRenderer(PassRef<RenderStyle> style)
{
    return createRenderer<RenderSVGResourceFilter>(*this, WTF::move(style));
}

bool SVGFilterElement::childShouldCreateRenderer(const Node& child) const
{
    if (!child.isSVGElement())
        return false;

    const SVGElement& svgElement = toSVGElement(child);

    static NeverDestroyed<HashSet<QualifiedName>> allowedChildElementTags;
    if (allowedChildElementTags.get().isEmpty()) {
        allowedChildElementTags.get().add(SVGNames::feBlendTag);
        allowedChildElementTags.get().add(SVGNames::feColorMatrixTag);
        allowedChildElementTags.get().add(SVGNames::feComponentTransferTag);
        allowedChildElementTags.get().add(SVGNames::feCompositeTag);
        allowedChildElementTags.get().add(SVGNames::feConvolveMatrixTag);
        allowedChildElementTags.get().add(SVGNames::feDiffuseLightingTag);
        allowedChildElementTags.get().add(SVGNames::feDisplacementMapTag);
        allowedChildElementTags.get().add(SVGNames::feDistantLightTag);
        allowedChildElementTags.get().add(SVGNames::feDropShadowTag);
        allowedChildElementTags.get().add(SVGNames::feFloodTag);
        allowedChildElementTags.get().add(SVGNames::feFuncATag);
        allowedChildElementTags.get().add(SVGNames::feFuncBTag);
        allowedChildElementTags.get().add(SVGNames::feFuncGTag);
        allowedChildElementTags.get().add(SVGNames::feFuncRTag);
        allowedChildElementTags.get().add(SVGNames::feGaussianBlurTag);
        allowedChildElementTags.get().add(SVGNames::feImageTag);
        allowedChildElementTags.get().add(SVGNames::feMergeTag);
        allowedChildElementTags.get().add(SVGNames::feMergeNodeTag);
        allowedChildElementTags.get().add(SVGNames::feMorphologyTag);
        allowedChildElementTags.get().add(SVGNames::feOffsetTag);
        allowedChildElementTags.get().add(SVGNames::fePointLightTag);
        allowedChildElementTags.get().add(SVGNames::feSpecularLightingTag);
        allowedChildElementTags.get().add(SVGNames::feSpotLightTag);
        allowedChildElementTags.get().add(SVGNames::feTileTag);
        allowedChildElementTags.get().add(SVGNames::feTurbulenceTag);
    }

    return allowedChildElementTags.get().contains<SVGAttributeHashTranslator>(svgElement.tagQName());
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
