/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2022, 2023, 2024 Igalia S.L.
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

#include "RenderSVGResourceContainer.h"
#include "SVGMarkerTypes.h"

namespace WebCore {

class GraphicsContext;
class SVGMarkerElement;

class RenderSVGResourceMarker final : public RenderSVGResourceContainer {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderSVGResourceMarker);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSVGResourceMarker);
public:
    RenderSVGResourceMarker(SVGMarkerElement&, RenderStyle&&);
    virtual ~RenderSVGResourceMarker();

    inline bool hasReverseStart() const;

    void invalidateMarker();

    // Calculates marker boundaries, mapped to the target element's coordinate space
    FloatRect computeMarkerBoundingBox(const SVGBoundingBoxComputation::DecorationOptions&, const AffineTransform& markerTransformation) const;

    AffineTransform markerTransformation(const FloatPoint& origin, float angle, float strokeWidth) const;

private:
    ASCIILiteral renderName() const final { return "RenderSVGResourceMarker"_s; }

    inline SVGMarkerElement& markerElement() const;
    inline Ref<SVGMarkerElement> protectedMarkerElement() const;
    inline FloatPoint referencePoint() const;
    inline std::optional<float> angle() const;
    inline SVGMarkerUnitsType markerUnits() const;

    FloatRect viewport() const { return m_viewport; }
    FloatSize viewportSize() const { return m_viewport.size(); }

    void element() const = delete;
    bool updateLayoutSizeIfNeeded() final;
    std::optional<FloatRect> overridenObjectBoundingBoxWithoutTransformations() const final { return std::make_optional(viewport()); }

    FloatRect computeViewport() const;

    void applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption>) const final;
    LayoutRect overflowClipRect(const LayoutPoint& location, RenderFragmentContainer* = nullptr, OverlayScrollbarSizeRelevancy = OverlayScrollbarSizeRelevancy::IgnoreOverlayScrollbarSize, PaintPhase = PaintPhase::BlockBackground) const final;
    void updateLayerTransform() final;
    bool needsHasSVGTransformFlags() const final { return true; }

    void layout() final;
    void updateFromStyle() final;

private:
    AffineTransform m_supplementalLayerTransform;
    FloatRect m_viewport;
};

}

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGResourceMarker, isRenderSVGResourceMarker())
