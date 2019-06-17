/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Adobe Systems Incorporated. All rights reserved.
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
#include "SVGGeometryElement.h"

#include "DOMPoint.h"
#include "RenderSVGResource.h"
#include "RenderSVGShape.h"
#include "SVGDocumentExtensions.h"
#include "SVGPathUtilities.h"
#include "SVGPoint.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGGeometryElement);

SVGGeometryElement::SVGGeometryElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::pathLengthAttr, &SVGGeometryElement::m_pathLength>();
    });
}

float SVGGeometryElement::getTotalLength() const
{
    document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = downcast<RenderSVGShape>(this->renderer());
    if (!renderer)
        return 0;

    return renderer->getTotalLength();
}

Ref<SVGPoint> SVGGeometryElement::getPointAtLength(float distance) const
{
    FloatPoint point { };

    document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = downcast<RenderSVGShape>(this->renderer());
    if (renderer)
        renderer->getPointAtLength(point, distance);

    return SVGPoint::create(point);
}

bool SVGGeometryElement::isPointInFill(DOMPointInit&& pointInit)
{
    document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = downcast<RenderSVGShape>(this->renderer());
    if (!renderer)
        return false;

    FloatPoint point {static_cast<float>(pointInit.x), static_cast<float>(pointInit.y)};
    return renderer->isPointInFill(point);
}

bool SVGGeometryElement::isPointInStroke(DOMPointInit&& pointInit)
{
    document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = downcast<RenderSVGShape>(this->renderer());
    if (!renderer)
        return false;

    FloatPoint point {static_cast<float>(pointInit.x), static_cast<float>(pointInit.y)};
    return renderer->isPointInStroke(point);
}

void SVGGeometryElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::pathLengthAttr) {
        m_pathLength->setBaseValInternal(value.toFloat());
        if (m_pathLength->baseVal() < 0)
            document().accessSVGExtensions().reportError("A negative value for path attribute <pathLength> is not allowed");
        return;
    }

    SVGGraphicsElement::parseAttribute(name, value);
}

void SVGGeometryElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::pathLengthAttr) {
        InstanceInvalidationGuard guard(*this);
        if (auto* renderer = this->renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

}
