/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "SVGPatternElement.h"

#include "AffineTransform.h"
#include "Document.h"
#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "LegacyRenderSVGResourcePattern.h"
#include "NodeName.h"
#include "PatternAttributes.h"
#include "RenderSVGResourcePattern.h"
#include "SVGElementInlines.h"
#include "SVGFitToViewBox.h"
#include "SVGGraphicsElement.h"
#include "SVGNames.h"
#include "SVGRenderSupport.h"
#include "SVGStringList.h"
#include "SVGTransformable.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGPatternElement);

inline SVGPatternElement::SVGPatternElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGFitToViewBox(this)
    , SVGTests(this)
    , SVGURIReference(this)
{
    ASSERT(hasTagName(SVGNames::patternTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGPatternElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGPatternElement::m_y>();
        PropertyRegistry::registerProperty<SVGNames::widthAttr, &SVGPatternElement::m_width>();
        PropertyRegistry::registerProperty<SVGNames::heightAttr, &SVGPatternElement::m_height>();
        PropertyRegistry::registerProperty<SVGNames::patternUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGPatternElement::m_patternUnits>();
        PropertyRegistry::registerProperty<SVGNames::patternContentUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGPatternElement::m_patternContentUnits>();
        PropertyRegistry::registerProperty<SVGNames::patternTransformAttr, &SVGPatternElement::m_patternTransform>();
    });
}

Ref<SVGPatternElement> SVGPatternElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGPatternElement(tagName, document));
}

void SVGPatternElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGParsingError parseError = NoError;
    switch (name.nodeName()) {
    case AttributeNames::patternUnitsAttr: {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(newValue);
        if (propertyValue > 0)
            Ref { m_patternUnits }->setBaseValInternal<SVGUnitTypes::SVGUnitType>(propertyValue);
        break;
    }
    case AttributeNames::patternContentUnitsAttr: {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(newValue);
        if (propertyValue > 0)
            Ref { m_patternContentUnits }->setBaseValInternal<SVGUnitTypes::SVGUnitType>(propertyValue);
        break;
    }
    case AttributeNames::patternTransformAttr: {
        Ref { m_patternTransform }->baseVal()->parse(newValue);
        break;
    }
    case AttributeNames::xAttr:
        Ref { m_x }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::yAttr:
        Ref { m_y }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    case AttributeNames::widthAttr:
        Ref { m_width }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    case AttributeNames::heightAttr:
        Ref { m_height }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);

    SVGURIReference::parseAttribute(name, newValue);
    SVGTests::parseAttribute(name, newValue);
    SVGFitToViewBox::parseAttribute(name, newValue);
    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGPatternElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName) || SVGFitToViewBox::isKnownAttribute(attrName) || SVGURIReference::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        if (PropertyRegistry::isAnimatedLengthAttribute(attrName))
            setPresentationalHintStyleIsDirty();
        if (document().settings().layerBasedSVGEngineEnabled()) {
            if (CheckedPtr patternRenderer = dynamicDowncast<RenderSVGResourcePattern>(renderer()))
                patternRenderer->invalidatePattern();
            return;
        }

        updateSVGRendererForElementChange();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

void SVGPatternElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChange::Source::Parser)
        return;

    if (document().settings().layerBasedSVGEngineEnabled()) {
        if (CheckedPtr patternRenderer = dynamicDowncast<RenderSVGResourcePattern>(renderer()))
            patternRenderer->invalidatePattern();
    }

    updateSVGRendererForElementChange();
}

RenderPtr<RenderElement> SVGPatternElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGResourcePattern>(*this, WTFMove(style));
    return createRenderer<LegacyRenderSVGResourcePattern>(*this, WTFMove(style));
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

    if (!attributes.hasViewBox() && hasAttribute(SVGNames::viewBoxAttr) && hasValidViewBox())
        attributes.setViewBox(viewBox());

    if (!attributes.hasPreserveAspectRatio() && hasAttribute(SVGNames::preserveAspectRatioAttr))
        attributes.setPreserveAspectRatio(preserveAspectRatio());

    if (!attributes.hasPatternUnits() && hasAttribute(SVGNames::patternUnitsAttr))
        attributes.setPatternUnits(patternUnits());

    if (!attributes.hasPatternContentUnits() && hasAttribute(SVGNames::patternContentUnitsAttr))
        attributes.setPatternContentUnits(patternContentUnits());

    if (!attributes.hasPatternTransform() && hasAttribute(SVGNames::patternTransformAttr))
        attributes.setPatternTransform(patternTransform().concatenate());

    if (!attributes.hasPatternContentElement() && childElementCount())
        attributes.setPatternContentElement(this);
}

Ref<const SVGTransformList> SVGPatternElement::protectedPatternTransform() const
{
    return m_patternTransform->currentValue();
}

AffineTransform SVGPatternElement::localCoordinateSpaceTransform(SVGLocatable::CTMScope) const
{
    return protectedPatternTransform()->concatenate();
}

}
