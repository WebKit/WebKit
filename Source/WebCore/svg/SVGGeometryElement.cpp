/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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
#include "DocumentInlines.h"
#include "LegacyRenderSVGResource.h"
#include "LegacyRenderSVGShape.h"
#include "RenderSVGShape.h"
#include "SVGDocumentExtensions.h"
#include "SVGPathUtilities.h"
#include "SVGPoint.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGGeometryElement);

SVGGeometryElement::SVGGeometryElement(const QualifiedName& tagName, Document& document, UniqueRef<SVGPropertyRegistry>&& propertyRegistry)
    : SVGGraphicsElement(tagName, document, WTFMove(propertyRegistry))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::pathLengthAttr, &SVGGeometryElement::m_pathLength>();
    });
}

float SVGGeometryElement::getTotalLength() const
{
    document().updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    auto* renderer = this->renderer();
    if (!renderer)
        return 0;

    if (auto* renderSVGShape = dynamicDowncast<LegacyRenderSVGShape>(renderer))
        return renderSVGShape->getTotalLength();

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (auto* renderSVGShape = dynamicDowncast<RenderSVGShape>(renderer))
        return renderSVGShape->getTotalLength();
#endif

    ASSERT_NOT_REACHED();
    return 0;
}

ExceptionOr<Ref<SVGPoint>> SVGGeometryElement::getPointAtLength(float distance) const
{
    document().updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    // Spec: Clamp distance to [0, length].
    distance = clampTo<float>(distance, 0, getTotalLength());

    auto* renderer = this->renderer();
    // Spec: If current element is a non-rendered element, throw an InvalidStateError.
    if (!renderer)
        return Exception { ExceptionCode::InvalidStateError };

    // Spec: Return a newly created, detached SVGPoint object.
    if (auto* renderSVGShape = dynamicDowncast<LegacyRenderSVGShape>(renderer))
        return SVGPoint::create(renderSVGShape->getPointAtLength(distance));

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (auto* renderSVGShape = dynamicDowncast<RenderSVGShape>(renderer))
        return SVGPoint::create(renderSVGShape->getPointAtLength(distance));
#endif

    ASSERT_NOT_REACHED();
    return Exception { ExceptionCode::InvalidStateError };
}

bool SVGGeometryElement::isPointInFill(DOMPointInit&& pointInit)
{
    document().updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    auto* renderer = this->renderer();
    if (!renderer)
        return false;

    FloatPoint point {static_cast<float>(pointInit.x), static_cast<float>(pointInit.y)};
    if (auto* renderSVGShape = dynamicDowncast<LegacyRenderSVGShape>(renderer))
        return renderSVGShape->isPointInFill(point);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (auto* renderSVGShape = dynamicDowncast<RenderSVGShape>(renderer))
        return renderSVGShape->isPointInFill(point);
#endif

    ASSERT_NOT_REACHED();
    return false;
}

bool SVGGeometryElement::isPointInStroke(DOMPointInit&& pointInit)
{
    document().updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    auto* renderer = this->renderer();
    if (!renderer)
        return false;

    FloatPoint point {static_cast<float>(pointInit.x), static_cast<float>(pointInit.y)};
    if (auto* renderSVGShape = dynamicDowncast<LegacyRenderSVGShape>(renderer))
        return renderSVGShape->isPointInStroke(point);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (auto* renderSVGShape = dynamicDowncast<RenderSVGShape>(renderer))
        return renderSVGShape->isPointInStroke(point);
#endif

    ASSERT_NOT_REACHED();
    return false;
}

void SVGGeometryElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::pathLengthAttr) {
        m_pathLength->setBaseValInternal(newValue.toFloat());
        if (m_pathLength->baseVal() < 0)
            document().accessSVGExtensions().reportError("A negative value for path attribute <pathLength> is not allowed"_s);
    }

    SVGGraphicsElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
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
