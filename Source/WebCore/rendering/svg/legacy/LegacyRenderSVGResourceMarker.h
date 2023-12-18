/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

namespace WebCore {

class AffineTransform;
class RenderObject;
class SVGMarkerElement;

class LegacyRenderSVGResourceMarker final : public LegacyRenderSVGResourceContainer {
    WTF_MAKE_ISO_ALLOCATED(LegacyRenderSVGResourceMarker);
public:
    LegacyRenderSVGResourceMarker(SVGMarkerElement&, RenderStyle&&);
    virtual ~LegacyRenderSVGResourceMarker();

    inline SVGMarkerElement& markerElement() const;

    void removeAllClientsFromCacheIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers) override;
    void removeClientFromCache(RenderElement&, bool markForInvalidation = true) override;

    void draw(PaintInfo&, const AffineTransform&);

    // Calculates marker boundaries, mapped to the target element's coordinate space
    FloatRect markerBoundaries(RepaintRectCalculation, const AffineTransform& markerTransformation) const;

    void applyViewportClip(PaintInfo&) override;
    void layout() override;
    void calcViewport() override;

    const AffineTransform& localToParentTransform() const override;
    AffineTransform markerTransformation(const FloatPoint& origin, float angle, float strokeWidth) const;

    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) override { return false; }
    FloatRect resourceBoundingBox(const RenderObject&, RepaintRectCalculation) override { return FloatRect(); }

    FloatPoint referencePoint() const;
    std::optional<float> angle() const;
    inline SVGMarkerUnitsType markerUnits() const;

    RenderSVGResourceType resourceType() const override { return MarkerResourceType; }

private:
    void element() const = delete;

    ASCIILiteral renderName() const override { return "RenderSVGResourceMarker"_s; }

    // Generates a transformation matrix usable to render marker content. Handles scaling the marker content
    // acording to SVGs markerUnits="strokeWidth" concept, when a strokeWidth value != -1 is passed in.
    AffineTransform markerContentTransformation(const AffineTransform& contentTransformation, const FloatPoint& origin, float strokeWidth = -1) const;

    AffineTransform viewportTransform() const;

    mutable AffineTransform m_localToParentTransform;
    FloatRect m_viewport;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LegacyRenderSVGResourceMarker)
static bool isType(const WebCore::RenderObject& renderer) { return renderer.isLegacyRenderSVGResourceMarker(); }
static bool isType(const WebCore::LegacyRenderSVGResource& resource) { return resource.resourceType() == WebCore::MarkerResourceType; }
SPECIALIZE_TYPE_TRAITS_END()
