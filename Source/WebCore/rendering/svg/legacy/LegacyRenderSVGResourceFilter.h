/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "FilterResults.h"
#include "FilterTargetSwitcher.h"
#include "LegacyRenderSVGResourceContainer.h"
#include "SVGFilter.h"
#include "SVGUnitTypes.h"
#include <wtf/IsoMalloc.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class GraphicsContext;
class SVGFilterElement;

struct FilterData {
    WTF_MAKE_ISO_ALLOCATED(FilterData);
    WTF_MAKE_NONCOPYABLE(FilterData);
public:
    enum FilterDataState { PaintingSource, Applying, Built, CycleDetected, MarkedForRemoval };

    FilterData() = default;

    RefPtr<SVGFilter> filter;

    std::unique_ptr<FilterTargetSwitcher> targetSwitcher;
    FloatRect sourceImageRect;

    GraphicsContext* savedContext { nullptr };
    FilterDataState state { PaintingSource };
};

class LegacyRenderSVGResourceFilter final : public LegacyRenderSVGResourceContainer {
    WTF_MAKE_ISO_ALLOCATED(LegacyRenderSVGResourceFilter);
public:
    LegacyRenderSVGResourceFilter(SVGFilterElement&, RenderStyle&&);
    virtual ~LegacyRenderSVGResourceFilter();

    inline SVGFilterElement& filterElement() const;
    bool isIdentity() const;

    void removeAllClientsFromCacheIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers) override;
    void removeClientFromCache(RenderElement&, bool markForInvalidation = true) override;

    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) override;
    void postApplyResource(RenderElement&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement*) override;

    FloatRect resourceBoundingBox(const RenderObject&, RepaintRectCalculation) override;

    inline SVGUnitTypes::SVGUnitType filterUnits() const;
    inline SVGUnitTypes::SVGUnitType primitiveUnits() const;

    void markFilterForRepaint(FilterEffect&);
    void markFilterForRebuild();

    RenderSVGResourceType resourceType() const override { return FilterResourceType; }

    FloatRect drawingRegion(RenderObject&) const;

private:
    void element() const = delete;

    ASCIILiteral renderName() const override { return "RenderSVGResourceFilter"_s; }

    HashMap<SingleThreadWeakRef<RenderObject>, std::unique_ptr<FilterData>> m_rendererFilterDataMap;
};

WTF::TextStream& operator<<(WTF::TextStream&, FilterData::FilterDataState);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LegacyRenderSVGResourceFilter)
    static bool isType(const WebCore::RenderObject& renderer) { return renderer.isRenderSVGResourceFilter(); }
    static bool isType(const WebCore::LegacyRenderSVGResource& resource) { return resource.resourceType() == WebCore::FilterResourceType; }
SPECIALIZE_TYPE_TRAITS_END()
