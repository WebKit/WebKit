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

#ifndef RenderSVGResourceFilter_h
#define RenderSVGResourceFilter_h

#include "ImageBuffer.h"
#include "RenderSVGResourceContainer.h"
#include "SVGFilter.h"
#include "SVGFilterBuilder.h"
#include "SVGFilterElement.h"
#include "SVGUnitTypes.h"

#include <wtf/RefPtr.h>

namespace WebCore {

struct FilterData {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(FilterData);
public:
    enum FilterDataState { PaintingSource, Applying, Built, CycleDetected, MarkedForRemoval };

    FilterData()
        : savedContext(0)
        , state(PaintingSource)
    {
    }

    RefPtr<SVGFilter> filter;
    std::unique_ptr<SVGFilterBuilder> builder;
    std::unique_ptr<ImageBuffer> sourceGraphicBuffer;
    GraphicsContext* savedContext;
    AffineTransform shearFreeAbsoluteTransform;
    FloatRect boundaries;
    FloatRect drawingRegion;
    FloatSize scale;
    FilterDataState state;
};

class GraphicsContext;

class RenderSVGResourceFilter final : public RenderSVGResourceContainer {
public:
    RenderSVGResourceFilter(SVGFilterElement&, Ref<RenderStyle>&&);
    virtual ~RenderSVGResourceFilter();

    SVGFilterElement& filterElement() const { return downcast<SVGFilterElement>(RenderSVGResourceContainer::element()); }

    void removeAllClientsFromCache(bool markForInvalidation = true) override;
    void removeClientFromCache(RenderElement&, bool markForInvalidation = true) override;

    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, unsigned short resourceMode) override;
    void postApplyResource(RenderElement&, GraphicsContext*&, unsigned short resourceMode, const Path*, const RenderSVGShape*) override;

    FloatRect resourceBoundingBox(const RenderObject&) override;

    std::unique_ptr<SVGFilterBuilder> buildPrimitives(SVGFilter&) const;

    SVGUnitTypes::SVGUnitType filterUnits() const { return filterElement().filterUnits(); }
    SVGUnitTypes::SVGUnitType primitiveUnits() const { return filterElement().primitiveUnits(); }

    void primitiveAttributeChanged(RenderObject*, const QualifiedName&);

    RenderSVGResourceType resourceType() const override { return FilterResourceType; }

    FloatRect drawingRegion(RenderObject*) const;
private:
    void element() const = delete;

    const char* renderName() const override { return "RenderSVGResourceFilter"; }
    bool isSVGResourceFilter() const override { return true; }

    HashMap<RenderObject*, std::unique_ptr<FilterData>> m_filter;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::RenderSVGResourceFilter)
    static bool isType(const WebCore::RenderObject& renderer) { return renderer.isSVGResourceFilter(); }
    static bool isType(const WebCore::RenderSVGResource& resource) { return resource.resourceType() == WebCore::FilterResourceType; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // RenderSVGResourceFilter_h
