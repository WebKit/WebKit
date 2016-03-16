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
#include "SVGPolyElement.h"

#include "Document.h"
#include "FloatPoint.h"
#include "RenderSVGPath.h"
#include "RenderSVGResource.h"
#include "SVGAnimatedPointList.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"

namespace WebCore {

// Define custom animated property 'points'.
const SVGPropertyInfo* SVGPolyElement::pointsPropertyInfo()
{
    static const SVGPropertyInfo* s_propertyInfo = nullptr;
    if (!s_propertyInfo) {
        s_propertyInfo = new SVGPropertyInfo(AnimatedPoints,
                                             PropertyIsReadWrite,
                                             SVGNames::pointsAttr,
                                             SVGNames::pointsAttr.localName(),
                                             &SVGPolyElement::synchronizePoints,
                                             &SVGPolyElement::lookupOrCreatePointsWrapper);
    }
    return s_propertyInfo;
}

// Animated property definitions
DEFINE_ANIMATED_BOOLEAN(SVGPolyElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGPolyElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(points)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

SVGPolyElement::SVGPolyElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
{
    registerAnimatedPropertiesForSVGPolyElement();    
}

void SVGPolyElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::pointsAttr) {
        SVGPointList newList;
        if (!pointsListFromSVGData(newList, value))
            document().accessSVGExtensions().reportError("Problem parsing points=\"" + value + "\"");

        if (auto wrapper = SVGAnimatedProperty::lookupWrapper<SVGPolyElement, SVGAnimatedPointList>(this, pointsPropertyInfo()))
            static_pointer_cast<SVGAnimatedPointList>(wrapper)->detachListWrappers(newList.size());

        m_points.value = newList;
        return;
    }

    SVGGraphicsElement::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGPolyElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::pointsAttr) {
        if (auto* renderer = downcast<RenderSVGPath>(this->renderer())) {
            InstanceInvalidationGuard guard(*this);
            renderer->setNeedsShapeUpdate();
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }
        return;
    }

    if (SVGLangSpace::isKnownAttribute(attrName) || SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        if (auto* renderer = downcast<RenderSVGPath>(this->renderer())) {
            InstanceInvalidationGuard guard(*this);
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

void SVGPolyElement::synchronizePoints(SVGElement* contextElement)
{
    ASSERT(contextElement);
    SVGPolyElement& ownerType = downcast<SVGPolyElement>(*contextElement);
    if (!ownerType.m_points.shouldSynchronize)
        return;
    ownerType.m_points.synchronize(&ownerType, pointsPropertyInfo()->attributeName, ownerType.m_points.value.valueAsString());
}

Ref<SVGAnimatedProperty> SVGPolyElement::lookupOrCreatePointsWrapper(SVGElement* contextElement)
{
    ASSERT(contextElement);
    SVGPolyElement& ownerType = downcast<SVGPolyElement>(*contextElement);
    return SVGAnimatedProperty::lookupOrCreateWrapper<SVGPolyElement, SVGAnimatedPointList, SVGPointList>
        (&ownerType, pointsPropertyInfo(), ownerType.m_points.value);
}

RefPtr<SVGListPropertyTearOff<SVGPointList>> SVGPolyElement::points()
{
    m_points.shouldSynchronize = true;
    return static_cast<SVGListPropertyTearOff<SVGPointList>*>(static_reference_cast<SVGAnimatedPointList>(lookupOrCreatePointsWrapper(this))->baseVal().get());
}

RefPtr<SVGListPropertyTearOff<SVGPointList>> SVGPolyElement::animatedPoints()
{
    m_points.shouldSynchronize = true;
    return static_cast<SVGListPropertyTearOff<SVGPointList>*>(static_reference_cast<SVGAnimatedPointList>(lookupOrCreatePointsWrapper(this))->animVal().get());
}

}
