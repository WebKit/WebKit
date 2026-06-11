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

#include "ContainerNodeInlines.h"
#include "DOMPoint.h"
#include "LegacyRenderSVGResource.h"
#include "LegacyRenderSVGShape.h"
#include "NodeDocument.h"
#include "Path.h"
#include "RenderSVGShape.h"
#include "SVGDocumentExtensions.h"
#include "SVGPathData.h"
#include "SVGPathUtilities.h"
#include "SVGPoint.h"
#include "SVGPropertyOwnerRegistry.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGGeometryElement);

SVGGeometryElement::SVGGeometryElement(const QualifiedName& tagName, Document& document, UniqueRef<SVGPropertyRegistry>&& propertyRegistry)
    : SVGGraphicsElement(tagName, document, WTF::move(propertyRegistry))
{
    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::pathLengthAttr, &SVGGeometryElement::m_pathLength>();
    }
}

float SVGGeometryElement::getTotalLength() const
{
    protect(document())->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, this);

    // The length is computed from the element's path geometry. This works for both rendered and
    // non-rendered (e.g. display:none) elements; the renderer-based path is derived from the same
    // geometry, so the result is equivalent. For a <path>, SVGPathElement overrides this method.
    if (const Path* path = cachedRendererPath())
        return path->length();
    return pathFromGraphicsElement(*this).length();
}

ExceptionOr<Ref<SVGPoint>> SVGGeometryElement::getPointAtLength(float distance) const
{
    protect(document())->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, this);

    // Reuse the renderer's cached path when it has one; only compute the geometry directly for
    // non-rendered (e.g. display:none) elements and uncached shape fast paths.
    Path computedPath;
    const Path* path = cachedRendererPath();
    if (!path) {
        computedPath = pathFromGraphicsElement(*this);
        path = &computedPath;
    }

    // Spec: If the user agent is not able to compute the total length of the path (i.e. the element
    // has no path geometry), throw an InvalidStateError. This covers empty rendered shapes as well
    // as non-rendered (e.g. display:none) elements, independent of the 'display' value.
    if (path->isEmpty())
        return Exception { ExceptionCode::InvalidStateError };

    // Spec: Clamp distance to [0, length], then return a newly created, detached SVGPoint.
    distance = clampTo<float>(distance, 0, path->length());
    return SVGPoint::create(path->pointAtLength(distance));
}

const Path* SVGGeometryElement::cachedRendererPath() const
{
    if (auto* renderSVGShape = dynamicDowncast<LegacyRenderSVGShape>(renderer()); renderSVGShape && renderSVGShape->hasPath())
        return &renderSVGShape->path();

    if (auto* renderSVGShape = dynamicDowncast<RenderSVGShape>(renderer()); renderSVGShape && renderSVGShape->hasPath())
        return &renderSVGShape->path();

    return nullptr;
}

bool SVGGeometryElement::isPointInFill(DOMPointInit&& pointInit)
{
    protect(document())->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, this);

    auto* renderer = this->renderer();
    if (!renderer)
        return false;

    FloatPoint point {static_cast<float>(pointInit.x), static_cast<float>(pointInit.y)};
    if (CheckedPtr renderSVGShape = dynamicDowncast<LegacyRenderSVGShape>(renderer))
        return renderSVGShape->isPointInFill(point);

    if (CheckedPtr renderSVGShape = dynamicDowncast<RenderSVGShape>(renderer))
        return renderSVGShape->isPointInFill(point);

    ASSERT_NOT_REACHED();
    return false;
}

bool SVGGeometryElement::isPointInStroke(DOMPointInit&& pointInit)
{
    protect(document())->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, this);

    auto* renderer = this->renderer();
    if (!renderer)
        return false;

    FloatPoint point {static_cast<float>(pointInit.x), static_cast<float>(pointInit.y)};
    if (CheckedPtr renderSVGShape = dynamicDowncast<LegacyRenderSVGShape>(renderer))
        return renderSVGShape->isPointInStroke(point);

    if (CheckedPtr renderSVGShape = dynamicDowncast<RenderSVGShape>(renderer))
        return renderSVGShape->isPointInStroke(point);

    ASSERT_NOT_REACHED();
    return false;
}

void SVGGeometryElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::pathLengthAttr) {
        Ref pathLength = m_pathLength;
        pathLength->setBaseValInternal(newValue.toFloat());
        if (pathLength->baseVal() < 0)
            protect(protect(document())->svgExtensions())->reportError("A negative value for path attribute <pathLength> is not allowed"_s);
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
