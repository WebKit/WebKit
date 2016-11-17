/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "SVGPatternElement.h"

#include "AffineTransform.h"
#include "Document.h"
#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PatternAttributes.h"
#include "RenderSVGContainer.h"
#include "RenderSVGResourcePattern.h"
#include "SVGFitToViewBox.h"
#include "SVGGraphicsElement.h"
#include "SVGNames.h"
#include "SVGRenderSupport.h"
#include "SVGStringList.h"
#include "SVGTransformable.h"
#include "XLinkNames.h"
#include <wtf/NeverDestroyed.h>

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

Ref<SVGPatternElement> SVGPatternElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGPatternElement(tagName, document));
}

bool SVGPatternElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static NeverDestroyed<HashSet<QualifiedName>> supportedAttributes;
    if (supportedAttributes.get().isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        SVGTests::addSupportedAttributes(supportedAttributes);
        SVGLangSpace::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        SVGFitToViewBox::addSupportedAttributes(supportedAttributes);
        supportedAttributes.get().add(SVGNames::patternUnitsAttr);
        supportedAttributes.get().add(SVGNames::patternContentUnitsAttr);
        supportedAttributes.get().add(SVGNames::patternTransformAttr);
        supportedAttributes.get().add(SVGNames::xAttr);
        supportedAttributes.get().add(SVGNames::yAttr);
        supportedAttributes.get().add(SVGNames::widthAttr);
        supportedAttributes.get().add(SVGNames::heightAttr);
    }
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGPatternElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::patternUnitsAttr) {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setPatternUnitsBaseValue(propertyValue);
        return;
    }
    if (name == SVGNames::patternContentUnitsAttr) {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setPatternContentUnitsBaseValue(propertyValue);
        return;
    }
    if (name == SVGNames::patternTransformAttr) {
        SVGTransformListValues newList;
        newList.parse(value);
        detachAnimatedPatternTransformListWrappers(newList.size());
        setPatternTransformBaseValue(newList);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::xAttr)
        setXBaseValue(SVGLengthValue::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::yAttr)
        setYBaseValue(SVGLengthValue::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::widthAttr)
        setWidthBaseValue(SVGLengthValue::construct(LengthModeWidth, value, parseError, ForbidNegativeLengths));
    else if (name == SVGNames::heightAttr)
        setHeightBaseValue(SVGLengthValue::construct(LengthModeHeight, value, parseError, ForbidNegativeLengths));

    reportAttributeParsingError(parseError, name, value);

    SVGElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
    SVGTests::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
    SVGFitToViewBox::parseAttribute(this, name, value);
}

void SVGPatternElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    InstanceInvalidationGuard guard(*this);

    if (attrName == SVGNames::xAttr
        || attrName == SVGNames::yAttr
        || attrName == SVGNames::widthAttr
        || attrName == SVGNames::heightAttr) {
        invalidateSVGPresentationAttributeStyle();
        return;
    }

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

RenderPtr<RenderElement> SVGPatternElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGResourcePattern>(*this, WTFMove(style));
}

void SVGPatternElement::collectPatternAttributes(PatternAttributes& attributes) const
{
    if (!attributes.hasX() && hasAttribute(SVGNames::xAttr))
        attributes.setX(x());

    if (!attributes.hasY() && hasAttribute(SVGNames::yAttr))
        attributes.setY(y());

    if (!attributes.hasWidth() && hasAttribute(SVGNames::widthAttr))
        attributes.setWidth(width());

    if (!attributes.hasHeight() && hasAttribute(SVGNames::heightAttr))
        attributes.setHeight(height());

    if (!attributes.hasViewBox() && hasAttribute(SVGNames::viewBoxAttr) && viewBoxIsValid())
        attributes.setViewBox(viewBox());

    if (!attributes.hasPreserveAspectRatio() && hasAttribute(SVGNames::preserveAspectRatioAttr))
        attributes.setPreserveAspectRatio(preserveAspectRatio());

    if (!attributes.hasPatternUnits() && hasAttribute(SVGNames::patternUnitsAttr))
        attributes.setPatternUnits(patternUnits());

    if (!attributes.hasPatternContentUnits() && hasAttribute(SVGNames::patternContentUnitsAttr))
        attributes.setPatternContentUnits(patternContentUnits());

    if (!attributes.hasPatternTransform() && hasAttribute(SVGNames::patternTransformAttr)) {
        AffineTransform transform;
        patternTransform().concatenate(transform);
        attributes.setPatternTransform(transform);
    }

    if (!attributes.hasPatternContentElement() && childElementCount())
        attributes.setPatternContentElement(this);
}

AffineTransform SVGPatternElement::localCoordinateSpaceTransform(SVGLocatable::CTMScope) const
{
    AffineTransform matrix;
    patternTransform().concatenate(matrix);
    return matrix;
}

Ref<SVGStringList> SVGPatternElement::requiredFeatures()
{
    return SVGTests::requiredFeatures(*this);
}

Ref<SVGStringList> SVGPatternElement::requiredExtensions()
{ 
    return SVGTests::requiredExtensions(*this);
}

Ref<SVGStringList> SVGPatternElement::systemLanguage()
{
    return SVGTests::systemLanguage(*this);
}

}
