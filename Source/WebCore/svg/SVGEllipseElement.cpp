/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "SVGEllipseElement.h"

#include "FloatPoint.h"
#include "RenderSVGEllipse.h"
#include "RenderSVGPath.h"
#include "RenderSVGResource.h"
#include "SVGLength.h"
#include "SVGNames.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGEllipseElement, SVGNames::cxAttr, Cx, cx)
DEFINE_ANIMATED_LENGTH(SVGEllipseElement, SVGNames::cyAttr, Cy, cy)
DEFINE_ANIMATED_LENGTH(SVGEllipseElement, SVGNames::rxAttr, Rx, rx)
DEFINE_ANIMATED_LENGTH(SVGEllipseElement, SVGNames::ryAttr, Ry, ry)
DEFINE_ANIMATED_BOOLEAN(SVGEllipseElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGEllipseElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(cx)
    REGISTER_LOCAL_ANIMATED_PROPERTY(cy)
    REGISTER_LOCAL_ANIMATED_PROPERTY(rx)
    REGISTER_LOCAL_ANIMATED_PROPERTY(ry)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGEllipseElement::SVGEllipseElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
    , m_cx(LengthModeWidth)
    , m_cy(LengthModeHeight)
    , m_rx(LengthModeWidth)
    , m_ry(LengthModeHeight)
{
    ASSERT(hasTagName(SVGNames::ellipseTag));
    registerAnimatedPropertiesForSVGEllipseElement();
}    

Ref<SVGEllipseElement> SVGEllipseElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGEllipseElement(tagName, document));
}

void SVGEllipseElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::cxAttr)
        setCxBaseValue(SVGLength::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::cyAttr)
        setCyBaseValue(SVGLength::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::rxAttr)
        setRxBaseValue(SVGLength::construct(LengthModeWidth, value, parseError, ForbidNegativeLengths));
    else if (name == SVGNames::ryAttr)
        setRyBaseValue(SVGLength::construct(LengthModeHeight, value, parseError, ForbidNegativeLengths));

    reportAttributeParsingError(parseError, name, value);

    SVGGraphicsElement::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGEllipseElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::cxAttr || attrName == SVGNames::cyAttr || attrName == SVGNames::rxAttr || attrName == SVGNames::ryAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidateSVGPresentationAttributeStyle();
        return;
    }

    if (SVGLangSpace::isKnownAttribute(attrName) || SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        if (auto* renderer = downcast<RenderSVGShape>(this->renderer())) {
            InstanceInvalidationGuard guard(*this);
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGEllipseElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGEllipse>(*this, WTFMove(style));
}

}
