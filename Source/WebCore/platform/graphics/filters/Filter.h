/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include <WebCore/FilterEffectVector.h>
#include <WebCore/FilterFunction.h>
#include <WebCore/FloatPoint3D.h>
#include <WebCore/FloatRect.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/RenderingMode.h>

namespace WebCore {

class FilterEffect;
class FilterImage;
class FilterResults;

struct FilterGeometry {
    FloatRect referenceBox;
    FloatRect filterRegion;
    FloatSize scale;
};

class Filter : public FilterFunction {
    using FilterFunction::apply;
    using FilterFunction::createFilterStyles;

public:
    RenderingMode renderingMode() const;

    OptionSet<FilterRenderingMode> filterRenderingModes() const { return m_filterRenderingModes; }
    WEBCORE_EXPORT void setFilterRenderingModes(OptionSet<FilterRenderingMode> preferredFilterRenderingModes);

    void setIsShowingDebugOverlay(bool showOverlay) { m_isShowingDebugOverlay = showOverlay; }
    bool isShowingDebugOverlay() const { return m_isShowingDebugOverlay; }

    const FilterGeometry& geometry() const { return m_geometry; }

    FloatSize filterScale() const { return m_geometry.scale; }
    void setFilterScale(const FloatSize& filterScale) { m_geometry.scale = filterScale; }

    FloatRect filterRegion() const { return m_geometry.filterRegion; }
    void setFilterRegion(const FloatRect& filterRegion) { m_geometry.filterRegion = filterRegion; }

    FloatRect referenceBox() const { return m_geometry.referenceBox; }

    virtual FloatSize resolvedSize(const FloatSize& size) const { return size; }
    virtual FloatPoint3D resolvedPoint3D(const FloatPoint3D& point) const { return point; }

    FloatPoint scaledByFilterScale(const FloatPoint&) const;
    FloatSize scaledByFilterScale(const FloatSize&) const;
    FloatRect scaledByFilterScale(const FloatRect&) const;

    FloatRect maxEffectRect(const FloatRect& primitiveSubregion) const;
    FloatRect clipToMaxEffectRect(const FloatRect& imageRect, const FloatRect& primitiveSubregion) const;

#if USE(CORE_IMAGE)
    // When CSS filter has a mixture of CSS and SVG filters, we need to compute CI filter geometry
    // against a consistent enclosing rect, which we compute as the union of the filter regions
    // of all the filters in the chain.
    FloatRect enclosingFilterRegion() const { return m_enclosingFilterRegion; }
    void setEnclosingFilterRegion(const FloatRect& rect) { m_enclosingFilterRegion = rect; }

    FloatRect absoluteEnclosingFilterRegion() const;
    FloatRect flippedRectRelativeToAbsoluteEnclosingFilterRegion(const FloatRect&) const;
#endif

    virtual FilterEffectVector effectsOfType(FilterFunction::Type) const = 0;

    bool clampFilterRegionIfNeeded();

    WEBCORE_EXPORT RefPtr<FilterImage> apply(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, FilterResults&);
    WEBCORE_EXPORT FilterStyleVector createFilterStyles(GraphicsContext&, const FloatRect& sourceImageRect) const;

    ImageBuffer* filterResultBuffer(FilterImage&) const;

protected:
    Filter(Filter::Type, std::optional<RenderingResourceIdentifier> = std::nullopt);
    Filter(Filter::Type, const FilterGeometry&, std::optional<RenderingResourceIdentifier> = std::nullopt);

    virtual RefPtr<FilterImage> apply(FilterImage* sourceImage, FilterResults&) = 0;
    virtual FilterStyleVector createFilterStyles(GraphicsContext&, const FilterStyle& sourceStyle) const = 0;

private:
    FilterGeometry m_geometry;
#if USE(CORE_IMAGE)
    FloatRect m_enclosingFilterRegion;
#endif
    OptionSet<FilterRenderingMode> m_filterRenderingModes { FilterRenderingMode::Software };
    bool m_isShowingDebugOverlay { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Filter)
    static bool isType(const WebCore::RenderingResource& renderingResource) { return renderingResource.isFilter(); }
SPECIALIZE_TYPE_TRAITS_END()
