/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc. All rights reserved.
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

#include <WebCore/LayoutRepainter.h>
#include <WebCore/PaintInfo.h>
#include <WebCore/RenderObject.h>

namespace WebCore {

class FloatPoint;
class FloatRect;
class ImageBuffer;
class LayoutRect;
class RenderBoxModelObject;
class RenderElement;
class RenderGeometryMap;
class RenderLayerModelObject;
class RenderStyle;
class LegacyRenderSVGRoot;
class SVGElement;
class TransformState;

// SVGRendererSupport is a helper class sharing code between all SVG renderers.
class SVGRenderSupport {
public:
    static void layoutDifferentRootIfNeeded(const RenderElement&);

    // Shares child layouting code between LegacyRenderSVGRoot/RenderSVG(Hidden)Container
    static void layoutChildren(RenderElement&, bool selfNeedsLayout);

    // Helper function determining wheter overflow is hidden
    static bool NODELETE isOverflowHidden(const RenderElement&);

    // Applies filter/clipper/masker resource effects to a geometric bounding rect.
    // This is the preferred API for resource code (masks, gradients, clippers) that needs
    // to compute bounds for rendering. Unlike intersectRepaintRectWithResources(), this
    // function is semantically about geometric calculations, not repaint/invalidation.
    static void applyResourceEffectsToRect(const RenderElement&, FloatRect&);

    // Calculates the repaintRect in combination with filter, clipper and masker in local coordinates.
    // FIXME: After migrating all RepaintRectCalculation::Accurate callers to use applyResourceEffectsToRect
    // function, we can simplify this to only handle Fast mode.
    static void intersectRepaintRectWithResources(const RenderElement&, FloatRect&, RepaintRectCalculation = RepaintRectCalculation::Fast);

    static FloatRect computeContainerDecoratedBoundingBox(const RenderElement& container);

    // Determines whether a container needs to be laid out because it's filtered and a child is being laid out.
    static bool filtersForceContainerLayout(const RenderElement&);

    // Determines whether the passed point lies in a clipping area
    static bool pointInClippingArea(const RenderElement&, const FloatPoint&);

    struct ContainerBoundingBoxes {
        Markable<FloatRect> objectBoundingBox;
        FloatRect repaintBoundingBox;
    };

    static ContainerBoundingBoxes computeContainerBoundingBoxes(const RenderElement&, RepaintRectCalculation = RepaintRectCalculation::Fast);

    static FloatRect computeContainerStrokeBoundingBox(const RenderElement& container);
    static bool paintInfoIntersectsRepaintRect(const FloatRect& localRepaintRect, const AffineTransform& localTransform, const PaintInfo&);

    // Important functions used by nearly all SVG renderers centralizing coordinate transformations / repaint rect calculations
    static LayoutRect clippedOverflowRectForRepaint(const RenderElement&, const RenderLayerModelObject* container, VisibleRectContext);
    static std::optional<FloatRect> computeFloatVisibleRectInContainer(const RenderElement&, const FloatRect&, const RenderLayerModelObject* container, VisibleRectContext);
    static const RenderElement& localToParentTransform(const RenderElement&, AffineTransform&);
    static void mapLocalToContainer(const RenderElement&, const RenderLayerModelObject* ancestorContainer, TransformState&, bool* wasFixed);
    static const RenderElement* pushMappingToContainer(const RenderElement&, const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&);
    static LayoutRepainter::CheckForRepaint checkForSVGRepaintDuringLayout(const RenderElement&);

    static FloatRect calculateApproximateStrokeBoundingBox(const RenderElement&);

    static void NODELETE updateAncestorNonScalingStrokeCounts(RenderElement&, int delta);

    static void NODELETE elementInsertedIntoTree(RenderElement&);
    static void NODELETE elementWillBeRemovedFromTree(RenderElement&);

    // Shared between SVG renderers and resources.
    static void applyStrokeStyleToContext(GraphicsContext&, const RenderStyle&, const RenderElement&);

    // Determines if any ancestor's transform has changed.
    static bool transformToRootChanged(RenderElement*);

    static void clipContextToCSSClippingArea(GraphicsContext&, const RenderElement& renderer);

    static void styleChanged(RenderElement&, const RenderStyle*);

    static bool isolatesBlending(const RenderStyle&);
    static void updateMaskedAncestorShouldIsolateBlending(const RenderElement&);

    static LegacyRenderSVGRoot* NODELETE findTreeRootObject(RenderElement&);
    static const LegacyRenderSVGRoot* findTreeRootObject(const RenderElement&);

private:
    // This class is not constructable.
    SVGRenderSupport();
    ~SVGRenderSupport();
};

} // namespace WebCore
