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
#include "LegacyRenderSVGShape.h"
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

    auto* renderer = this->renderer();
    if (!renderer)
        return 0;

    if (is<LegacyRenderSVGShape>(renderer))
        return downcast<LegacyRenderSVGShape>(renderer)->getTotalLength();

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGShape>(renderer))
        return downcast<RenderSVGShape>(renderer)->getTotalLength();
#endif

    ASSERT_NOT_REACHED();
    return 0;
}

ExceptionOr<Ref<SVGPoint>> SVGGeometryElement::getPointAtLength(float distance) const
{
    document().updateLayoutIgnorePendingStylesheets();

    // Spec: Clamp distance to [0, length].
    distance = clampTo<float>(distance, 0, getTotalLength());

    auto* renderer = this->renderer();

    // Spec: If current element is a non-rendered element, throw an InvalidStateError.
    if (!renderer)
        return Exception { InvalidStateError };

    // Spec: Return a newly created, detached SVGPoint object.
    if (is<LegacyRenderSVGShape>(renderer))
        return SVGPoint::create(downcast<LegacyRenderSVGShape>(renderer)->getPointAtLength(distance));

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGShape>(renderer))
        return SVGPoint::create(downcast<RenderSVGShape>(renderer)->getPointAtLength(distance));
#endif

    ASSERT_NOT_REACHED();
    return Exception { InvalidStateError };
}

bool SVGGeometryElement::isPointInFill(DOMPointInit&& pointInit)
{
    document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = this->renderer();
    if (!renderer)
        return false;

    FloatPoint point {static_cast<float>(pointInit.x), static_cast<float>(pointInit.y)};
    if (is<LegacyRenderSVGShape>(renderer))
        return downcast<LegacyRenderSVGShape>(renderer)->isPointInFill(point);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGShape>(renderer))
        return downcast<RenderSVGShape>(renderer)->isPointInFill(point);
#endif

    ASSERT_NOT_REACHED();
    return false;
}

bool SVGGeometryElement::isPointInStroke(DOMPointInit&& pointInit)
{
    document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = this->renderer();
    if (!renderer)
        return false;

    FloatPoint point {static_cast<float>(pointInit.x), static_cast<float>(pointInit.y)};
    if (is<LegacyRenderSVGShape>(renderer))
        return downcast<LegacyRenderSVGShape>(renderer)->isPointInStroke(point);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGShape>(renderer))
        return downcast<RenderSVGShape>(renderer)->isPointInStroke(point);
#endif

    ASSERT_NOT_REACHED();
    return false;
}

void SVGGeometryElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::pathLengthAttr) {
        m_pathLength->setBaseValInternal(value.toFloat());
        if (m_pathLength->baseVal() < 0)
            document().accessSVGExtensions().reportError("A negative value for path attribute <pathLength> is not allowed"_s);
        return;
    }

    SVGGraphicsElement::parseAttribute(name, value);
}

void SVGGeometryElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        ASSERT(attrName == SVGNames::pathLengthAttr);
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

}
