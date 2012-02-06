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
#include "SVGEllipseElement.h"

#include "Attribute.h"
#include "FloatPoint.h"
#include "RenderSVGPath.h"
#include "RenderSVGResource.h"
#include "SVGElementInstance.h"
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
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGStyledTransformableElement)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTests)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGEllipseElement::SVGEllipseElement(const QualifiedName& tagName, Document* document)
    : SVGStyledTransformableElement(tagName, document)
    , m_cx(LengthModeWidth)
    , m_cy(LengthModeHeight)
    , m_rx(LengthModeWidth)
    , m_ry(LengthModeHeight)
{
    ASSERT(hasTagName(SVGNames::ellipseTag));
    registerAnimatedPropertiesForSVGEllipseElement();
}    

PassRefPtr<SVGEllipseElement> SVGEllipseElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGEllipseElement(tagName, document));
}

bool SVGEllipseElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGTests::addSupportedAttributes(supportedAttributes);
        SVGLangSpace::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::cxAttr);
        supportedAttributes.add(SVGNames::cyAttr);
        supportedAttributes.add(SVGNames::rxAttr);
        supportedAttributes.add(SVGNames::ryAttr);
    }
    return supportedAttributes.contains<QualifiedName, SVGAttributeHashTranslator>(attrName);
}

void SVGEllipseElement::parseAttribute(Attribute* attr)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(attr->name()))
        SVGStyledTransformableElement::parseAttribute(attr);
    else if (attr->name() == SVGNames::cxAttr)
        setCxBaseValue(SVGLength::construct(LengthModeWidth, attr->value(), parseError));
    else if (attr->name() == SVGNames::cyAttr)
        setCyBaseValue(SVGLength::construct(LengthModeHeight, attr->value(), parseError));
    else if (attr->name() == SVGNames::rxAttr)
        setRxBaseValue(SVGLength::construct(LengthModeWidth, attr->value(), parseError, ForbidNegativeLengths));
    else if (attr->name() == SVGNames::ryAttr)
        setRyBaseValue(SVGLength::construct(LengthModeHeight, attr->value(), parseError, ForbidNegativeLengths));
    else if (SVGTests::parseAttribute(attr)
             || SVGLangSpace::parseAttribute(attr)
             || SVGExternalResourcesRequired::parseAttribute(attr)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, attr);
}

void SVGEllipseElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGStyledTransformableElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    bool isLengthAttribute = attrName == SVGNames::cxAttr
                          || attrName == SVGNames::cyAttr
                          || attrName == SVGNames::rxAttr
                          || attrName == SVGNames::ryAttr;

    if (isLengthAttribute)
        updateRelativeLengthsInformation();
 
    if (SVGTests::handleAttributeChange(this, attrName))
        return;

    RenderSVGPath* renderer = static_cast<RenderSVGPath*>(this->renderer());
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
 
bool SVGEllipseElement::selfHasRelativeLengths() const
{
    return cx().isRelative()
        || cy().isRelative()
        || rx().isRelative()
        || ry().isRelative();
}

}

#endif // ENABLE(SVG)
