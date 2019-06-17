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

#include "RenderSVGResourceMarker.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGMarkerElement);

inline SVGMarkerElement::SVGMarkerElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , SVGExternalResourcesRequired(this)
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

void SVGMarkerElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::markerUnitsAttr) {
        auto propertyValue = SVGPropertyTraits<SVGMarkerUnitsType>::fromString(value);
        if (propertyValue > 0)
            m_markerUnits->setBaseValInternal<SVGMarkerUnitsType>(propertyValue);
        return;
    }

    if (name == SVGNames::orientAttr) {
        auto pair = SVGPropertyTraits<std::pair<SVGAngleValue, SVGMarkerOrientType>>::fromString(value);
        m_orientAngle->setBaseValInternal(pair.first);
        m_orientType->setBaseValInternal(pair.second);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::refXAttr)
        m_refX->setBaseValInternal(SVGLengthValue::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::refYAttr)
        m_refY->setBaseValInternal(SVGLengthValue::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::markerWidthAttr)
        m_markerWidth->setBaseValInternal(SVGLengthValue::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::markerHeightAttr)
        m_markerHeight->setBaseValInternal(SVGLengthValue::construct(LengthModeHeight, value, parseError));

    reportAttributeParsingError(parseError, name, value);

    SVGElement::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
    SVGFitToViewBox::parseAttribute(name, value);
}

void SVGMarkerElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        if (PropertyRegistry::isAnimatedLengthAttribute(attrName))
            updateRelativeLengthsInformation();
        if (RenderObject* object = renderer())
            object->setNeedsLayout();
        return;
    }
    
    if (SVGFitToViewBox::isKnownAttribute(attrName)) {
        if (RenderObject* object = renderer())
            object->setNeedsLayout();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
    SVGExternalResourcesRequired::svgAttributeChanged(attrName);
}

void SVGMarkerElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChangeSource::Parser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

void SVGMarkerElement::setOrient(SVGMarkerOrientType orientType, const SVGAngleValue& angle)
{
    m_orientType->setBaseValInternal(orientType);
    m_orientAngle->setBaseValInternal(angle);
    m_orientAngle->baseVal()->commitChange();
}

void SVGMarkerElement::setOrientToAuto()
{
    setOrient(SVGMarkerOrientAuto, { });
}

void SVGMarkerElement::setOrientToAngle(SVGAngle& angle)
{
    setOrient(SVGMarkerOrientAngle, angle.value());
}

RenderPtr<RenderElement> SVGMarkerElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGResourceMarker>(*this, WTFMove(style));
}

bool SVGMarkerElement::selfHasRelativeLengths() const
{
    return refX().isRelative()
        || refY().isRelative()
        || markerWidth().isRelative()
        || markerHeight().isRelative();
}

}
