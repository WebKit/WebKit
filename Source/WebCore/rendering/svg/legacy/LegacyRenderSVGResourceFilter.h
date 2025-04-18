/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#pragma once

#include "LegacyRenderSVGResourceContainer.h"
#include "RenderFilterResource.h"
#include "SVGUnitTypes.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class SVGFilterElement;

class LegacyRenderSVGResourceFilter final : public LegacyRenderSVGResourceContainer, public RenderFilterResource {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(LegacyRenderSVGResourceFilter);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LegacyRenderSVGResourceFilter);
public:
    LegacyRenderSVGResourceFilter(SVGFilterElement&, RenderStyle&&);

    inline SVGFilterElement& filterElement() const;
    inline Ref<SVGFilterElement> protectedFilterElement() const;

    inline SVGUnitTypes::SVGUnitType filterUnits() const;
    inline SVGUnitTypes::SVGUnitType primitiveUnits() const;

    RenderSVGResourceType resourceType() const final { return FilterResourceType; }

    bool isIdentity() const;

    FloatRect drawingRegion(RenderElement&) const;
    FloatRect resourceBoundingBox(const RenderObject&, RepaintRectCalculation) final;

    OptionSet<ApplyResult> applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) final;
    void postApplyResource(RenderElement&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement*) final;

    void markFilterForRepaint(FilterEffect&);
    void markFilterForRebuild();

    void removeAllClientsFromCache() final;
    void removeClientFromCache(RenderElement&) final;

private:
    void element() const = delete;

    std::optional<std::tuple<Ref<Filter>, FloatRect>> createFilter(RenderElement&, const FloatRect& targetBoundingBox, GraphicsContext&) final;
    DestinationColorSpace operatingColorSpace() const final;
    DestinationColorSpace destinationColorSpace() const final { return DestinationColorSpace::LinearSRGB(); }

    ASCIILiteral renderName() const final { return "RenderSVGResourceFilter"_s; }

    HashMap<SingleThreadWeakRef<RenderObject>, std::unique_ptr<FilterClient>> m_rendererFilterClientCache;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LegacyRenderSVGResourceFilter)
    static bool isType(const WebCore::RenderObject& renderer) { return renderer.isLegacyRenderSVGResourceFilter(); }
    static bool isType(const WebCore::LegacyRenderSVGResource& resource) { return resource.resourceType() == WebCore::FilterResourceType; }
SPECIALIZE_TYPE_TRAITS_END()
