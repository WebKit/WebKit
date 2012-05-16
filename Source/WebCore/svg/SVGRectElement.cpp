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
#include "SVGRectElement.h"

#include "Attribute.h"
#include "RenderSVGPath.h"
#include "RenderSVGRect.h"
#include "RenderSVGResource.h"
#include "SVGElementInstance.h"
#include "SVGLength.h"
#include "SVGNames.h"

namespace WebCore {

class RenderSVGRect;

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGRectElement, SVGNames::xAttr, X, x)
DEFINE_ANIMATED_LENGTH(SVGRectElement, SVGNames::yAttr, Y, y)
DEFINE_ANIMATED_LENGTH(SVGRectElement, SVGNames::widthAttr, Width, width)
DEFINE_ANIMATED_LENGTH(SVGRectElement, SVGNames::heightAttr, Height, height)
DEFINE_ANIMATED_LENGTH(SVGRectElement, SVGNames::rxAttr, Rx, rx)
DEFINE_ANIMATED_LENGTH(SVGRectElement, SVGNames::ryAttr, Ry, ry)
DEFINE_ANIMATED_BOOLEAN(SVGRectElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGRectElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(x)
    REGISTER_LOCAL_ANIMATED_PROPERTY(y)
    REGISTER_LOCAL_ANIMATED_PROPERTY(width)
    REGISTER_LOCAL_ANIMATED_PROPERTY(height)
    REGISTER_LOCAL_ANIMATED_PROPERTY(rx)
    REGISTER_LOCAL_ANIMATED_PROPERTY(ry)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGStyledTransformableElement)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTests)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGRectElement::SVGRectElement(const QualifiedName& tagName, Document* document)
    : SVGStyledTransformableElement(tagName, document)
    , m_x(LengthModeWidth)
    , m_y(LengthModeHeight)
    , m_width(LengthModeWidth)
    , m_height(LengthModeHeight)
    , m_rx(LengthModeWidth)
    , m_ry(LengthModeHeight)
{
    ASSERT(hasTagName(SVGNames::rectTag));
    registerAnimatedPropertiesForSVGRectElement();
}

PassRefPtr<SVGRectElement> SVGRectElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGRectElement(tagName, document));
}

bool SVGRectElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGTests::addSupportedAttributes(supportedAttributes);
        SVGLangSpace::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
        supportedAttributes.add(SVGNames::widthAttr);
        supportedAttributes.add(SVGNames::heightAttr);
        supportedAttributes.add(SVGNames::rxAttr);
        supportedAttributes.add(SVGNames::ryAttr);
    }
    return supportedAttributes.contains<QualifiedName, SVGAttributeHashTranslator>(attrName);
}

void SVGRectElement::parseAttribute(const Attribute& attribute)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(attribute.name()))
        SVGStyledTransformableElement::parseAttribute(attribute);
    else if (attribute.name() == SVGNames::xAttr)
        setXBaseValue(SVGLength::construct(LengthModeWidth, attribute.value(), parseError));
    else if (attribute.name() == SVGNames::yAttr)
        setYBaseValue(SVGLength::construct(LengthModeHeight, attribute.value(), parseError));
    else if (attribute.name() == SVGNames::rxAttr)
        setRxBaseValue(SVGLength::construct(LengthModeWidth, attribute.value(), parseError, ForbidNegativeLengths));
    else if (attribute.name() == SVGNames::ryAttr)
        setRyBaseValue(SVGLength::construct(LengthModeHeight, attribute.value(), parseError, ForbidNegativeLengths));
    else if (attribute.name() == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength::construct(LengthModeWidth, attribute.value(), parseError, ForbidNegativeLengths));
    else if (attribute.name() == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength::construct(LengthModeHeight, attribute.value(), parseError, ForbidNegativeLengths));
    else if (SVGTests::parseAttribute(attribute)
             || SVGLangSpace::parseAttribute(attribute)
             || SVGExternalResourcesRequired::parseAttribute(attribute)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, attribute);
}

void SVGRectElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGStyledTransformableElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);
    
    bool isLengthAttribute = attrName == SVGNames::xAttr
                          || attrName == SVGNames::yAttr
                          || attrName == SVGNames::widthAttr
                          || attrName == SVGNames::heightAttr
                          || attrName == SVGNames::rxAttr
                          || attrName == SVGNames::ryAttr;

    if (isLengthAttribute)
        updateRelativeLengthsInformation();

    if (SVGTests::handleAttributeChange(this, attrName))
        return;

    RenderSVGRect* renderer = static_cast<RenderSVGRect*>(this->renderer());
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

bool SVGRectElement::selfHasRelativeLengths() const
{
    return x().isRelative()
        || y().isRelative()
        || width().isRelative()
        || height().isRelative()
        || rx().isRelative()
        || ry().isRelative();
}

RenderObject* SVGRectElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGRect(this);
}

}

#endif // ENABLE(SVG)
