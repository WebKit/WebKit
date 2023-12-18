/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
#include "SVGMarkerElement.h"

#include "LegacyRenderSVGResourceMarker.h"
#include "NodeName.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGMarkerElement);

inline SVGMarkerElement::SVGMarkerElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGFitToViewBox(this)
{
    // Spec: If the markerWidth/markerHeight attribute is not specified, the effect is as if a value of "3" were specified.
    ASSERT(hasTagName(SVGNames::markerTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::refXAttr, &SVGMarkerElement::m_refX>();
        PropertyRegistry::registerProperty<SVGNames::refYAttr, &SVGMarkerElement::m_refY>();
        PropertyRegistry::registerProperty<SVGNames::markerWidthAttr, &SVGMarkerElement::m_markerWidth>();
        PropertyRegistry::registerProperty<SVGNames::markerHeightAttr, &SVGMarkerElement::m_markerHeight>();
        PropertyRegistry::registerProperty<SVGNames::markerUnitsAttr, SVGMarkerUnitsType, &SVGMarkerElement::m_markerUnits>();
        PropertyRegistry::registerProperty<SVGNames::orientAttr, &SVGMarkerElement::m_orientAngle, &SVGMarkerElement::m_orientType>();
    });
}

Ref<SVGMarkerElement> SVGMarkerElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGMarkerElement(tagName, document));
}

AffineTransform SVGMarkerElement::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    return SVGFitToViewBox::viewBoxToViewTransform(viewBox(), preserveAspectRatio(), viewWidth, viewHeight);
}

void SVGMarkerElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGParsingError parseError = NoError;
    switch (name.nodeName()) {
    case AttributeNames::markerUnitsAttr: {
        auto propertyValue = SVGPropertyTraits<SVGMarkerUnitsType>::fromString(newValue);
        if (propertyValue > 0)
            m_markerUnits->setBaseValInternal<SVGMarkerUnitsType>(propertyValue);
        return;
    }
    case AttributeNames::orientAttr: {
        auto pair = SVGPropertyTraits<std::pair<SVGAngleValue, SVGMarkerOrientType>>::fromString(newValue);
        m_orientAngle->setBaseValInternal(pair.first);
        m_orientType->setBaseValInternal(pair.second);
        return;
    }
    case AttributeNames::refXAttr:
        m_refX->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::refYAttr:
        m_refY->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    case AttributeNames::markerWidthAttr:
        m_markerWidth->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::markerHeightAttr:
        m_markerHeight->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);

    SVGFitToViewBox::parseAttribute(name, newValue);
    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGMarkerElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        if (PropertyRegistry::isAnimatedLengthAttribute(attrName))
            updateRelativeLengthsInformation();
        updateSVGRendererForElementChange();
        return;
    }
    
    if (SVGFitToViewBox::isKnownAttribute(attrName)) {
        updateSVGRendererForElementChange();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

void SVGMarkerElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChange::Source::Parser)
        return;

    updateSVGRendererForElementChange();
}

AtomString SVGMarkerElement::orient() const
{
    return getAttribute(SVGNames::orientAttr);
}

void SVGMarkerElement::setOrient(const AtomString& orient)
{
    setAttribute(SVGNames::orientAttr, orient);
}

void SVGMarkerElement::setOrientToAuto()
{
    m_orientType->setBaseVal(SVGMarkerOrientAuto);
}

void SVGMarkerElement::setOrientToAngle(const SVGAngle& angle)
{
    m_orientAngle->baseVal()->newValueSpecifiedUnits(angle.unitType(), angle.valueInSpecifiedUnits());
}

RenderPtr<RenderElement> SVGMarkerElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<LegacyRenderSVGResourceMarker>(*this, WTFMove(style));
}

bool SVGMarkerElement::selfHasRelativeLengths() const
{
    return refX().isRelative()
        || refY().isRelative()
        || markerWidth().isRelative()
        || markerHeight().isRelative();
}

}
