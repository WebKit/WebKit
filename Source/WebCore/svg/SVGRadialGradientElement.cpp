/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
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
#include "SVGRadialGradientElement.h"

#include "Attribute.h"
#include "FloatConversion.h"
#include "FloatPoint.h"
#include "RadialGradientAttributes.h"
#include "RenderSVGResourceRadialGradient.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGStopElement.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "SVGUnitTypes.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGRadialGradientElement, SVGNames::cxAttr, Cx, cx)
DEFINE_ANIMATED_LENGTH(SVGRadialGradientElement, SVGNames::cyAttr, Cy, cy)
DEFINE_ANIMATED_LENGTH(SVGRadialGradientElement, SVGNames::rAttr, R, r)
DEFINE_ANIMATED_LENGTH(SVGRadialGradientElement, SVGNames::fxAttr, Fx, fx)
DEFINE_ANIMATED_LENGTH(SVGRadialGradientElement, SVGNames::fyAttr, Fy, fy)
DEFINE_ANIMATED_LENGTH(SVGRadialGradientElement, SVGNames::frAttr, Fr, fr)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGRadialGradientElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(cx)
    REGISTER_LOCAL_ANIMATED_PROPERTY(cy)
    REGISTER_LOCAL_ANIMATED_PROPERTY(r)
    REGISTER_LOCAL_ANIMATED_PROPERTY(fx)
    REGISTER_LOCAL_ANIMATED_PROPERTY(fy)
    REGISTER_LOCAL_ANIMATED_PROPERTY(fr)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGradientElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGRadialGradientElement::SVGRadialGradientElement(const QualifiedName& tagName, Document& document)
    : SVGGradientElement(tagName, document)
    , m_cx(LengthModeWidth, "50%")
    , m_cy(LengthModeHeight, "50%")
    , m_r(LengthModeOther, "50%")
    , m_fx(LengthModeWidth)
    , m_fy(LengthModeHeight)
    , m_fr(LengthModeOther, "0%")
{
    // Spec: If the cx/cy/r/fr attribute is not specified, the effect is as if a value of "50%" were specified.
    ASSERT(hasTagName(SVGNames::radialGradientTag));
    registerAnimatedPropertiesForSVGRadialGradientElement();
}

PassRefPtr<SVGRadialGradientElement> SVGRadialGradientElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new SVGRadialGradientElement(tagName, document));
}

bool SVGRadialGradientElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static NeverDestroyed<HashSet<QualifiedName>> supportedAttributes;
    if (supportedAttributes.get().isEmpty()) {
        supportedAttributes.get().add(SVGNames::cxAttr);
        supportedAttributes.get().add(SVGNames::cyAttr);
        supportedAttributes.get().add(SVGNames::fxAttr);
        supportedAttributes.get().add(SVGNames::fyAttr);
        supportedAttributes.get().add(SVGNames::rAttr);
        supportedAttributes.get().add(SVGNames::frAttr);
    }
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGRadialGradientElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGGradientElement::parseAttribute(name, value);
    else if (name == SVGNames::cxAttr)
        setCxBaseValue(SVGLength::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::cyAttr)
        setCyBaseValue(SVGLength::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::rAttr)
        setRBaseValue(SVGLength::construct(LengthModeOther, value, parseError, ForbidNegativeLengths));
    else if (name == SVGNames::fxAttr)
        setFxBaseValue(SVGLength::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::fyAttr)
        setFyBaseValue(SVGLength::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::frAttr)
        setFrBaseValue(SVGLength::construct(LengthModeOther, value, parseError, ForbidNegativeLengths));
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGRadialGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGradientElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);
    
    updateRelativeLengthsInformation();
        
    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

RenderPtr<RenderElement> SVGRadialGradientElement::createElementRenderer(PassRef<RenderStyle> style)
{
    return createRenderer<RenderSVGResourceRadialGradient>(*this, WTF::move(style));
}

static void setGradientAttributes(SVGGradientElement& element, RadialGradientAttributes& attributes, bool isRadial = true)
{
    if (!attributes.hasSpreadMethod() && element.hasAttribute(SVGNames::spreadMethodAttr))
        attributes.setSpreadMethod(element.spreadMethod());

    if (!attributes.hasGradientUnits() && element.hasAttribute(SVGNames::gradientUnitsAttr))
        attributes.setGradientUnits(element.gradientUnits());

    if (!attributes.hasGradientTransform() && element.hasAttribute(SVGNames::gradientTransformAttr)) {
        AffineTransform transform;
        element.gradientTransform().concatenate(transform);
        attributes.setGradientTransform(transform);
    }

    if (!attributes.hasStops()) {
        const Vector<Gradient::ColorStop>& stops(element.buildStops());
        if (!stops.isEmpty())
            attributes.setStops(stops);
    }

    if (isRadial) {
        SVGRadialGradientElement* radial = toSVGRadialGradientElement(&element);

        if (!attributes.hasCx() && element.hasAttribute(SVGNames::cxAttr))
            attributes.setCx(radial->cx());

        if (!attributes.hasCy() && element.hasAttribute(SVGNames::cyAttr))
            attributes.setCy(radial->cy());

        if (!attributes.hasR() && element.hasAttribute(SVGNames::rAttr))
            attributes.setR(radial->r());

        if (!attributes.hasFx() && element.hasAttribute(SVGNames::fxAttr))
            attributes.setFx(radial->fx());

        if (!attributes.hasFy() && element.hasAttribute(SVGNames::fyAttr))
            attributes.setFy(radial->fy());

        if (!attributes.hasFr() && element.hasAttribute(SVGNames::frAttr))
            attributes.setFr(radial->fr());
    }
}

bool SVGRadialGradientElement::collectGradientAttributes(RadialGradientAttributes& attributes)
{
    if (!renderer())
        return false;

    HashSet<SVGGradientElement*> processedGradients;
    SVGGradientElement* current = this;

    setGradientAttributes(*current, attributes);
    processedGradients.add(current);

    while (true) {
        // Respect xlink:href, take attributes from referenced element
        Node* refNode = SVGURIReference::targetElementFromIRIString(current->href(), document());
        if (refNode && isSVGGradientElement(*refNode)) {
            current = toSVGGradientElement(refNode);

            // Cycle detection
            if (processedGradients.contains(current))
                break;

            if (!current->renderer())
                return false;

            setGradientAttributes(*current, attributes, current->hasTagName(SVGNames::radialGradientTag));
            processedGradients.add(current);
        } else
            break;
    }

    // Handle default values for fx/fy
    if (!attributes.hasFx())
        attributes.setFx(attributes.cx());

    if (!attributes.hasFy())
        attributes.setFy(attributes.cy());

    return true;
}

bool SVGRadialGradientElement::selfHasRelativeLengths() const
{
    return cx().isRelative()
        || cy().isRelative()
        || r().isRelative()
        || fx().isRelative()
        || fy().isRelative()
        || fr().isRelative();
}

}
