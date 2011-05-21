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
#include "SVGPolyElement.h"

#include "Attribute.h"
#include "Document.h"
#include "FloatPoint.h"
#include "RenderSVGPath.h"
#include "RenderSVGResource.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGPointList.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_BOOLEAN(SVGPolyElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

SVGPolyElement::SVGPolyElement(const QualifiedName& tagName, Document* document)
    : SVGStyledTransformableElement(tagName, document)
{
}

bool SVGPolyElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGTests::addSupportedAttributes(supportedAttributes);
        SVGLangSpace::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::pointsAttr);
    }
    return supportedAttributes.contains(attrName);
}

void SVGPolyElement::parseMappedAttribute(Attribute* attr)
{
    if (!isSupportedAttribute(attr->name())) {
        SVGStyledTransformableElement::parseMappedAttribute(attr);
        return;
    }

    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::pointsAttr) {
        SVGPointList newList;
        if (!pointsListFromSVGData(newList, value))
            document()->accessSVGExtensions()->reportError("Problem parsing points=\"" + value + "\"");

        if (SVGAnimatedListPropertyTearOff<SVGPointList>* list = m_animatablePointsList.get())
            list->detachListWrappers(newList.size());

        m_points.value = newList;
        return;
    }

    if (SVGTests::parseMappedAttribute(attr))
        return;
    if (SVGLangSpace::parseMappedAttribute(attr))
        return;
    if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
        return;

    ASSERT_NOT_REACHED();
}

void SVGPolyElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGStyledTransformableElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);
    
    if (SVGTests::handleAttributeChange(this, attrName))
        return;

    RenderSVGPath* renderer = static_cast<RenderSVGPath*>(this->renderer());
    if (!renderer)
        return;

    if (attrName == SVGNames::pointsAttr) {
        renderer->setNeedsPathUpdate();
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    if (SVGLangSpace::isKnownAttribute(attrName) || SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGPolyElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeExternalResourcesRequired();
        synchronizePoints();
        SVGTests::synchronizeProperties(this, attrName);
        return;
    }

    if (SVGExternalResourcesRequired::isKnownAttribute(attrName))
        synchronizeExternalResourcesRequired();
    else if (attrName == SVGNames::pointsAttr)
        synchronizePoints();
    else if (SVGTests::isKnownAttribute(attrName))
        SVGTests::synchronizeProperties(this, attrName);
}

AttributeToPropertyTypeMap& SVGPolyElement::attributeToPropertyTypeMap()
{
    DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, s_attributeToPropertyTypeMap, ());
    return s_attributeToPropertyTypeMap;
}

void SVGPolyElement::fillAttributeToPropertyTypeMap()
{
    AttributeToPropertyTypeMap& attributeToPropertyTypeMap = this->attributeToPropertyTypeMap();

    SVGStyledTransformableElement::fillPassedAttributeToPropertyTypeMap(attributeToPropertyTypeMap);
    attributeToPropertyTypeMap.set(SVGNames::pointsAttr, AnimatedPoints);
}

void SVGPolyElement::synchronizePoints()
{
    if (!m_points.shouldSynchronize)
        return;

    SVGAnimatedPropertySynchronizer<true>::synchronize(this, SVGNames::pointsAttr, m_points.value.valueAsString());
}

SVGListPropertyTearOff<SVGPointList>* SVGPolyElement::points()
{
    if (!m_animatablePointsList) {
        m_points.shouldSynchronize = true;
        m_animatablePointsList = SVGAnimatedProperty::lookupOrCreateWrapper<SVGAnimatedListPropertyTearOff<SVGPointList> , SVGPointList>
                                 (this, SVGNames::pointsAttr, SVGNames::pointsAttr.localName(), m_points.value);
    }

    return static_cast<SVGListPropertyTearOff<SVGPointList>*>(m_animatablePointsList->baseVal());
}

SVGListPropertyTearOff<SVGPointList>* SVGPolyElement::animatedPoints()
{
    if (!m_animatablePointsList) {
        m_points.shouldSynchronize = true;
        m_animatablePointsList = SVGAnimatedProperty::lookupOrCreateWrapper<SVGAnimatedListPropertyTearOff<SVGPointList> , SVGPointList>
                                 (this, SVGNames::pointsAttr, SVGNames::pointsAttr.localName(), m_points.value);
    }

    return static_cast<SVGListPropertyTearOff<SVGPointList>*>(m_animatablePointsList->animVal());
}

}

#endif // ENABLE(SVG)
