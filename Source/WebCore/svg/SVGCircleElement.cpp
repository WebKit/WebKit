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

#if ENABLE(SVG)
#include "SVGCircleElement.h"

#include "Attribute.h"
#include "ExceptionCode.h"
#include "FloatPoint.h"
#include "RenderSVGEllipse.h"
#include "RenderSVGPath.h"
#include "RenderSVGResource.h"
#include "SVGElementInstance.h"
#include "SVGException.h"
#include "SVGLength.h"
#include "SVGNames.h"

namespace WebCore {

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
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGStyledTransformableElement)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTests)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGCircleElement::SVGCircleElement(const QualifiedName& tagName, Document* document)
    : SVGStyledTransformableElement(tagName, document)
    , m_cx(LengthModeWidth)
    , m_cy(LengthModeHeight)
    , m_r(LengthModeOther)
{
    ASSERT(hasTagName(SVGNames::circleTag));
    registerAnimatedPropertiesForSVGCircleElement();
}

PassRefPtr<SVGCircleElement> SVGCircleElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGCircleElement(tagName, document));
}

bool SVGCircleElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGTests::addSupportedAttributes(supportedAttributes);
        SVGLangSpace::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::cxAttr);
        supportedAttributes.add(SVGNames::cyAttr);
        supportedAttributes.add(SVGNames::rAttr);
    }
    return supportedAttributes.contains<QualifiedName, SVGAttributeHashTranslator>(attrName);
}

void SVGCircleElement::parseAttribute(const Attribute& attribute)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(attribute.name()))
        SVGStyledTransformableElement::parseAttribute(attribute);
    else if (attribute.name() == SVGNames::cxAttr)
        setCxBaseValue(SVGLength::construct(LengthModeWidth, attribute.value(), parseError));
    else if (attribute.name() == SVGNames::cyAttr)
        setCyBaseValue(SVGLength::construct(LengthModeHeight, attribute.value(), parseError));
    else if (attribute.name() == SVGNames::rAttr)
        setRBaseValue(SVGLength::construct(LengthModeOther, attribute.value(), parseError, ForbidNegativeLengths));
    else if (SVGTests::parseAttribute(attribute)
             || SVGLangSpace::parseAttribute(attribute)
             || SVGExternalResourcesRequired::parseAttribute(attribute)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, attribute);
}

void SVGCircleElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGStyledTransformableElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    bool isLengthAttribute = attrName == SVGNames::cxAttr
                          || attrName == SVGNames::cyAttr
                          || attrName == SVGNames::rAttr;

    if (isLengthAttribute)
        updateRelativeLengthsInformation();

    if (SVGTests::handleAttributeChange(this, attrName))
        return;

    RenderSVGEllipse* renderer = static_cast<RenderSVGEllipse*>(this->renderer());
    if (!renderer)
        return;

    if (isLengthAttribute) {
        renderer->setNeedsShapeUpdate();
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    if (SVGLangSpace::isKnownAttribute(attrName) || SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    ASSERT_NOT_REACHED();
}

bool SVGCircleElement::selfHasRelativeLengths() const
{
    return cx().isRelative()
        || cy().isRelative()
        || r().isRelative();
}

RenderObject* SVGCircleElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGEllipse(this);
}

}

#endif // ENABLE(SVG)
