/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#if ENABLE(SVG)
#include "SVGPatternElement.h"

#include "AffineTransform.h"
#include "Attribute.h"
#include "Document.h"
#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PatternAttributes.h"
#include "RenderSVGContainer.h"
#include "RenderSVGResourcePattern.h"
#include "SVGElementInstance.h"
#include "SVGFitToViewBox.h"
#include "SVGGraphicsElement.h"
#include "SVGNames.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "SVGTransformable.h"
#include "XLinkNames.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGPatternElement, SVGNames::xAttr, X, x)
DEFINE_ANIMATED_LENGTH(SVGPatternElement, SVGNames::yAttr, Y, y)
DEFINE_ANIMATED_LENGTH(SVGPatternElement, SVGNames::widthAttr, Width, width)
DEFINE_ANIMATED_LENGTH(SVGPatternElement, SVGNames::heightAttr, Height, height)
DEFINE_ANIMATED_ENUMERATION(SVGPatternElement, SVGNames::patternUnitsAttr, PatternUnits, patternUnits, SVGUnitTypes::SVGUnitType)
DEFINE_ANIMATED_ENUMERATION(SVGPatternElement, SVGNames::patternContentUnitsAttr, PatternContentUnits, patternContentUnits, SVGUnitTypes::SVGUnitType)
DEFINE_ANIMATED_TRANSFORM_LIST(SVGPatternElement, SVGNames::patternTransformAttr, PatternTransform, patternTransform) 
DEFINE_ANIMATED_STRING(SVGPatternElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGPatternElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)
DEFINE_ANIMATED_RECT(SVGPatternElement, SVGNames::viewBoxAttr, ViewBox, viewBox)
DEFINE_ANIMATED_PRESERVEASPECTRATIO(SVGPatternElement, SVGNames::preserveAspectRatioAttr, PreserveAspectRatio, preserveAspectRatio) 

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGPatternElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(x)
    REGISTER_LOCAL_ANIMATED_PROPERTY(y)
    REGISTER_LOCAL_ANIMATED_PROPERTY(width)
    REGISTER_LOCAL_ANIMATED_PROPERTY(height)
    REGISTER_LOCAL_ANIMATED_PROPERTY(patternUnits)
    REGISTER_LOCAL_ANIMATED_PROPERTY(patternContentUnits)
    REGISTER_LOCAL_ANIMATED_PROPERTY(patternTransform)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_LOCAL_ANIMATED_PROPERTY(viewBox)
    REGISTER_LOCAL_ANIMATED_PROPERTY(preserveAspectRatio) 
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGElement)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTests)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGPatternElement::SVGPatternElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , m_x(LengthModeWidth)
    , m_y(LengthModeHeight)
    , m_width(LengthModeWidth)
    , m_height(LengthModeHeight)
    , m_patternUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_patternContentUnits(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
{
    ASSERT(hasTagName(SVGNames::patternTag));
    registerAnimatedPropertiesForSVGPatternElement();
}

PassRefPtr<SVGPatternElement> SVGPatternElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new SVGPatternElement(tagName, document));
}

bool SVGPatternElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        SVGTests::addSupportedAttributes(supportedAttributes);
        SVGLangSpace::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        SVGFitToViewBox::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::patternUnitsAttr);
        supportedAttributes.add(SVGNames::patternContentUnitsAttr);
        supportedAttributes.add(SVGNames::patternTransformAttr);
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
        supportedAttributes.add(SVGNames::widthAttr);
        supportedAttributes.add(SVGNames::heightAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGPatternElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGElement::parseAttribute(name, value);
    else if (name == SVGNames::patternUnitsAttr) {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setPatternUnitsBaseValue(propertyValue);
        return;
    } else if (name == SVGNames::patternContentUnitsAttr) {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setPatternContentUnitsBaseValue(propertyValue);
        return;
    } else if (name == SVGNames::patternTransformAttr) {
        SVGTransformList newList;
        newList.parse(value);
        detachAnimatedPatternTransformListWrappers(newList.size());
        setPatternTransformBaseValue(newList);
        return;
    } else if (name == SVGNames::xAttr)
        setXBaseValue(SVGLength::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::yAttr)
        setYBaseValue(SVGLength::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength::construct(LengthModeWidth, value, parseError, ForbidNegativeLengths));
    else if (name == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength::construct(LengthModeHeight, value, parseError, ForbidNegativeLengths));
    else if (SVGURIReference::parseAttribute(name, value)
             || SVGTests::parseAttribute(name, value)
             || SVGLangSpace::parseAttribute(name, value)
             || SVGExternalResourcesRequired::parseAttribute(name, value)
             || SVGFitToViewBox::parseAttribute(this, name, value)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGPatternElement::svgAttributeChanged(const QualifiedName& attrName)
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

void SVGPatternElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChangeSourceParser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

RenderPtr<RenderElement> SVGPatternElement::createElementRenderer(PassRef<RenderStyle> style)
{
    return createRenderer<RenderSVGResourcePattern>(*this, std::move(style));
}

static void setPatternAttributes(const SVGPatternElement& element, PatternAttributes& attributes)
{
    if (!attributes.hasX() && element.hasAttribute(SVGNames::xAttr))
        attributes.setX(element.x());

    if (!attributes.hasY() && element.hasAttribute(SVGNames::yAttr))
        attributes.setY(element.y());

    if (!attributes.hasWidth() && element.hasAttribute(SVGNames::widthAttr))
        attributes.setWidth(element.width());

    if (!attributes.hasHeight() && element.hasAttribute(SVGNames::heightAttr))
        attributes.setHeight(element.height());

    if (!attributes.hasViewBox() && element.hasAttribute(SVGNames::viewBoxAttr) && element.viewBoxIsValid())
        attributes.setViewBox(element.viewBox());

    if (!attributes.hasPreserveAspectRatio() && element.hasAttribute(SVGNames::preserveAspectRatioAttr))
        attributes.setPreserveAspectRatio(element.preserveAspectRatio());

    if (!attributes.hasPatternUnits() && element.hasAttribute(SVGNames::patternUnitsAttr))
        attributes.setPatternUnits(element.patternUnits());

    if (!attributes.hasPatternContentUnits() && element.hasAttribute(SVGNames::patternContentUnitsAttr))
        attributes.setPatternContentUnits(element.patternContentUnits());

    if (!attributes.hasPatternTransform() && element.hasAttribute(SVGNames::patternTransformAttr)) {
        AffineTransform transform;
        element.patternTransform().concatenate(transform);
        attributes.setPatternTransform(transform);
    }

    if (!attributes.hasPatternContentElement() && element.childElementCount())
        attributes.setPatternContentElement(&element);
}

void SVGPatternElement::collectPatternAttributes(PatternAttributes& attributes) const
{
    HashSet<const SVGPatternElement*> processedPatterns;
    const SVGPatternElement* current = this;

    while (true) {
        setPatternAttributes(*current, attributes);
        processedPatterns.add(current);

        // Respect xlink:href, take attributes from referenced element
        Element* refElement = SVGURIReference::targetElementFromIRIString(current->href(), document());
        if (refElement && isSVGPatternElement(refElement)) {
            current = toSVGPatternElement(refElement);

            // Cycle detection
            if (processedPatterns.contains(current))
                return;
        } else
            return;
    }
    ASSERT_NOT_REACHED();
}

AffineTransform SVGPatternElement::localCoordinateSpaceTransform(SVGLocatable::CTMScope) const
{
    AffineTransform matrix;
    patternTransform().concatenate(matrix);
    return matrix;
}

bool SVGPatternElement::selfHasRelativeLengths() const
{
    return x().isRelative()
        || y().isRelative()
        || width().isRelative()
        || height().isRelative();
}

}

#endif // ENABLE(SVG)
