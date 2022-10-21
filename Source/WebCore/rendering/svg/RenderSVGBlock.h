/*
 * Copyright (C) 2006 Apple Inc.
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

#include "RenderBlockFlow.h"

namespace WebCore {

class SVGElement;
class SVGGraphicsElement;

class RenderSVGBlock : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGBlock);
public:
    inline SVGGraphicsElement& graphicsElement() const;

protected:
    RenderSVGBlock(SVGGraphicsElement&, RenderStyle&&);
    void willBeDestroyed() override;

    void computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats = false) override;
    void updateFromStyle() override;

private:
    void element() const = delete;
    bool isRenderSVGBlock() const final { return true; }

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    LayoutPoint currentSVGLayoutLocation() const final { return location(); }
    void setCurrentSVGLayoutLocation(const LayoutPoint& location) final { setLocation(location); }

    FloatRect referenceBoxRect(CSSBoxType) const final;
#endif

    LayoutRect clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext) const final;
    std::optional<FloatRect> computeFloatVisibleRectInContainer(const FloatRect&, const RenderLayerModelObject* container, VisibleRectContext) const final;
    std::optional<LayoutRect> computeVisibleRectInContainer(const LayoutRect&, const RenderLayerModelObject* container, VisibleRectContext) const final;

    void mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState&, OptionSet<MapCoordinatesMode>, bool* wasFixed) const final;
    const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const final;
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    LayoutSize offsetFromContainer(RenderElement&, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const override;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGBlock, isRenderSVGBlock())
