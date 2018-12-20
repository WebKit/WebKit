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
#include "SVGGraphicsElement.h"

namespace WebCore {

class RenderSVGInline : public RenderInline {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGInline);
public:
    RenderSVGInline(SVGGraphicsElement&, RenderStyle&&);

    SVGGraphicsElement& graphicsElement() const { return downcast<SVGGraphicsElement>(nodeForNonAnonymous()); }

private:
    void element() const = delete;

    const char* renderName() const override { return "RenderSVGInline"; }
    bool requiresLayer() const final { return false; }
    bool isSVGInline() const final { return true; }

    void updateFromStyle() final;

    // Chapter 10.4 of the SVG Specification say that we should use the
    // object bounding box of the parent text element.
    // We search for the root text element and take its bounding box.
    // It is also necessary to take the stroke and repaint rect of
    // this element, since we need it for filters.
    FloatRect objectBoundingBox() const final;
    FloatRect strokeBoundingBox() const final;
    FloatRect repaintRectInLocalCoordinates() const final;

    LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const final;
    Optional<FloatRect> computeFloatVisibleRectInContainer(const FloatRect&, const RenderLayerModelObject* container, VisibleRectContext) const final;
    void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags, bool* wasFixed) const final;
    const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const final;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const final;

    std::unique_ptr<InlineFlowBox> createInlineFlowBox() final;

    void willBeDestroyed() final;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGInline, isSVGInline())
