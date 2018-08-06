/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
#include "SVGMarkerElement.h"

#include "RenderSVGResourceMarker.h"
#include "SVGFitToViewBox.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGMarkerElement);

inline SVGMarkerElement::SVGMarkerElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , SVGExternalResourcesRequired(this)
    , SVGFitToViewBox(this)
{
    // Spec: If the markerWidth/markerHeight attribute is not specified, the effect is as if a value of "3" were specified.
    ASSERT(hasTagName(SVGNames::markerTag));
    registerAttributes();
}

Ref<SVGMarkerElement> SVGMarkerElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGMarkerElement(tagName, document));
}

const AtomicString& SVGMarkerElement::orientTypeIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGOrientType", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

const AtomicString& SVGMarkerElement::orientAngleIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGOrientAngle", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

AffineTransform SVGMarkerElement::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    return SVGFitToViewBox::viewBoxToViewTransform(viewBox(), preserveAspectRatio(), viewWidth, viewHeight);
}

void SVGMarkerElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::refXAttr, &SVGMarkerElement::m_refX>();
    registry.registerAttribute<SVGNames::refYAttr, &SVGMarkerElement::m_refY>();
    registry.registerAttribute<SVGNames::markerWidthAttr, &SVGMarkerElement::m_markerWidth>();
    registry.registerAttribute<SVGNames::markerHeightAttr, &SVGMarkerElement::m_markerHeight>();
    registry.registerAttribute<SVGNames::markerUnitsAttr, SVGMarkerUnitsType, &SVGMarkerElement::m_markerUnits>();
    registry.registerAttribute(SVGAnimatedCustomAngleAttributeAccessor::singleton<SVGNames::orientAttr,
        &SVGMarkerElement::orientAngleIdentifier, &SVGMarkerElement::m_orientAngle,
        &SVGMarkerElement::orientTypeIdentifier, &SVGMarkerElement::m_orientType>());
}

void SVGMarkerElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::markerUnitsAttr) {
        auto propertyValue = SVGPropertyTraits<SVGMarkerUnitsType>::fromString(value);
        if (propertyValue > 0)
            m_markerUnits.setValue(propertyValue);
        return;
    }

    if (name == SVGNames::orientAttr) {
        SVGAngleValue angle;
        auto orientType = SVGPropertyTraits<SVGMarkerOrientType>::fromString(value, angle);
        if (orientType > 0)
            m_orientType.setValue(orientType);
        if (orientType == SVGMarkerOrientAngle)
            m_orientAngle.setValue(angle);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::refXAttr)
        m_refX.setValue(SVGLengthValue::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::refYAttr)
        m_refY.setValue(SVGLengthValue::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::markerWidthAttr)
        m_markerWidth.setValue(SVGLengthValue::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::markerHeightAttr)
        m_markerHeight.setValue(SVGLengthValue::construct(LengthModeHeight, value, parseError));

    reportAttributeParsingError(parseError, name, value);

    SVGElement::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
    SVGFitToViewBox::parseAttribute(name, value);
}

void SVGMarkerElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        if (isAnimatedLengthAttribute(attrName))
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
    m_orientType.setValue(orientType);
    m_orientAngle.setValue(angle);

    // Mark orientAttr dirty - the next XML DOM access of that attribute kicks in synchronization.
    m_orientAngle.setShouldSynchronize(true);
    m_orientType.setShouldSynchronize(true);
    invalidateSVGAttributes();
    svgAttributeChanged(SVGNames::orientAttr);
}

void SVGMarkerElement::setOrientToAuto()
{
    setOrient(SVGMarkerOrientAuto, { });
}

void SVGMarkerElement::setOrientToAngle(SVGAngle& angle)
{
    setOrient(SVGMarkerOrientAngle, angle.propertyReference());
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
