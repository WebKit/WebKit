/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
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

#include "RenderInline.h"

namespace WebCore {

class SVGGraphicsElement;

class RenderSVGInline : public RenderInline {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGInline);
public:
    RenderSVGInline(Type, SVGGraphicsElement&, RenderStyle&&);

    inline SVGGraphicsElement& graphicsElement() const;

private:
    void element() const = delete;

    ASCIILiteral renderName() const override { return "RenderSVGInline"_s; }
    bool requiresLayer() const final { return false; }

    void updateFromStyle() final;

    // Chapter 10.4 of the SVG Specification say that we should use the
    // object bounding box of the parent text element.
    // We search for the root text element and take its bounding box.
    // It is also necessary to take the stroke and repaint rect of
    // this element, since we need it for filters.
    FloatRect objectBoundingBox() const final;
    FloatRect strokeBoundingBox() const final;
    FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const final;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    LayoutPoint currentSVGLayoutLocation() const final { return { }; }
    void setCurrentSVGLayoutLocation(const LayoutPoint&) final { ASSERT_NOT_REACHED(); }

    bool needsHasSVGTransformFlags() const final;
#endif

    LayoutRect clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext) const final;
    RepaintRects rectsForRepaintingAfterLayout(const RenderLayerModelObject* repaintContainer, RepaintOutlineBounds) const final;

    std::optional<FloatRect> computeFloatVisibleRectInContainer(const FloatRect&, const RenderLayerModelObject* container, VisibleRectContext) const final;

    void mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState&, OptionSet<MapCoordinatesMode>, bool* wasFixed) const final;
    const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const final;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const final;

    std::unique_ptr<LegacyInlineFlowBox> createInlineFlowBox() final;

    void willBeDestroyed() final;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGInline, isRenderSVGInline())
