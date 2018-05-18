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
#include "SVGCircleElement.h"

#include "FloatPoint.h"
#include "RenderSVGEllipse.h"
#include "RenderSVGPath.h"
#include "RenderSVGResource.h"
#include "SVGLengthValue.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGCircleElement);

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGCircleElement, SVGNames::cxAttr, Cx, cx)
DEFINE_ANIMATED_LENGTH(SVGCircleElement, SVGNames::cyAttr, Cy, cy)
DEFINE_ANIMATED_LENGTH(SVGCircleElement, SVGNames::rAttr, R, r)
DEFINE_ANIMATED_BOOLEAN(SVGCircleElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGCircleElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(cx)
    REGISTER_LOCAL_ANIMATED_PROPERTY(cy)
    REGISTER_LOCAL_ANIMATED_PROPERTY(r)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGeometryElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGCircleElement::SVGCircleElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document)
    , m_cx(LengthModeWidth)
    , m_cy(LengthModeHeight)
    , m_r(LengthModeOther)
{
    ASSERT(hasTagName(SVGNames::circleTag));
    registerAnimatedPropertiesForSVGCircleElement();
}

Ref<SVGCircleElement> SVGCircleElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGCircleElement(tagName, document));
}

void SVGCircleElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::cxAttr)
        setCxBaseValue(SVGLengthValue::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::cyAttr)
        setCyBaseValue(SVGLengthValue::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::rAttr)
        setRBaseValue(SVGLengthValue::construct(LengthModeOther, value, parseError, ForbidNegativeLengths));

    reportAttributeParsingError(parseError, name, value);

    SVGGraphicsElement::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGCircleElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGGraphicsElement::svgAttributeChanged(attrName);

    if (attrName == SVGNames::cxAttr || attrName == SVGNames::cyAttr || attrName == SVGNames::rAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidateSVGPresentationAttributeStyle();
        return;
    }

    auto* renderer = downcast<RenderSVGShape>(this->renderer());
    if (!renderer)
        return;

    if (SVGLangSpace::isKnownAttribute(attrName) || SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
    }
}

RenderPtr<RenderElement> SVGCircleElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGEllipse>(*this, WTFMove(style));
}

}
