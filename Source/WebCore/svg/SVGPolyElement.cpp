/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "SVGPolyElement.h"

#include "Document.h"
#include "RenderSVGPath.h"
#include "RenderSVGResource.h"
#include "SVGDocumentExtensions.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGPolyElement);

SVGPolyElement::SVGPolyElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document)
    , SVGExternalResourcesRequired(this)
{
    registerAttributes();
}

void SVGPolyElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::pointsAttr, &SVGPolyElement::m_points>();
}

void SVGPolyElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::pointsAttr) {
        SVGPointListValues newList;
        if (!pointsListFromSVGData(newList, value))
            document().accessSVGExtensions().reportError("Problem parsing points=\"" + value + "\"");

        if (auto wrapper = static_pointer_cast<SVGAnimatedPointList>(lookupAnimatedProperty(m_points)))
            wrapper->detachListWrappers(newList.size());

        m_points.setValue(WTFMove(newList));
        return;
    }

    SVGGeometryElement::parseAttribute(name, value);
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

    SVGGeometryElement::svgAttributeChanged(attrName);
    SVGExternalResourcesRequired::svgAttributeChanged(attrName);
}

Ref<SVGPointList> SVGPolyElement::points()
{
    m_points.setShouldSynchronize(true);
    return static_pointer_cast<SVGAnimatedPointList>(lookupOrCreateAnimatedProperty(m_points))->baseVal();
}

Ref<SVGPointList> SVGPolyElement::animatedPoints()
{
    m_points.setShouldSynchronize(true);
    return static_pointer_cast<SVGAnimatedPointList>(lookupOrCreateAnimatedProperty(m_points))->animVal();
}

size_t SVGPolyElement::approximateMemoryCost() const
{
    size_t pointsCost = pointList().size() * sizeof(FloatPoint);
    // We need to account for the memory which is allocated by the RenderSVGPath::m_path.
    return sizeof(*this) + (renderer() ? pointsCost * 2 + sizeof(RenderSVGPath) : pointsCost);
}

}
