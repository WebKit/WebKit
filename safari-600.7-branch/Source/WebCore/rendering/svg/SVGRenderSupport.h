/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
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

#ifndef SVGRenderSupport_h
#define SVGRenderSupport_h

#include "PaintInfo.h"

namespace WebCore {

class FloatPoint;
class FloatRect;
class ImageBuffer;
class LayoutRect;
class RenderBoxModelObject;
class RenderElement;
class RenderGeometryMap;
class RenderLayerModelObject;
class RenderObject;
class RenderStyle;
class RenderSVGRoot;
class TransformState;

// SVGRendererSupport is a helper class sharing code between all SVG renderers.
class SVGRenderSupport {
public:
    // Shares child layouting code between RenderSVGRoot/RenderSVG(Hidden)Container
    static void layoutChildren(RenderElement&, bool selfNeedsLayout);

    // Helper function determining wheter overflow is hidden
    static bool isOverflowHidden(const RenderElement&);

    static void intersectRepaintRectWithShadows(const RenderElement&, FloatRect&);

    // Calculates the repaintRect in combination with filter, clipper and masker in local coordinates.
    static void intersectRepaintRectWithResources(const RenderElement&, FloatRect&);

    // Determines whether a container needs to be laid out because it's filtered and a child is being laid out.
    static bool filtersForceContainerLayout(const RenderElement&);

    // Determines whether the passed point lies in a clipping area
    static bool pointInClippingArea(const RenderElement&, const FloatPoint&);

    static void computeContainerBoundingBoxes(const RenderElement& container, FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, FloatRect& strokeBoundingBox, FloatRect& repaintBoundingBox);
    static bool paintInfoIntersectsRepaintRect(const FloatRect& localRepaintRect, const AffineTransform& localTransform, const PaintInfo&);

    // Important functions used by nearly all SVG renderers centralizing coordinate transformations / repaint rect calculations
    static FloatRect repaintRectForRendererInLocalCoordinatesExcludingSVGShadow(const RenderElement&);
    static LayoutRect clippedOverflowRectForRepaint(const RenderElement&, const RenderLayerModelObject* repaintContainer);
    static void computeFloatRectForRepaint(const RenderElement&, const RenderLayerModelObject* repaintContainer, FloatRect&, bool fixed);
    static void mapLocalToContainer(const RenderElement&, const RenderLayerModelObject* repaintContainer, TransformState&, bool* wasFixed = 0);
    static const RenderElement* pushMappingToContainer(const RenderElement&, const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&);
    static bool checkForSVGRepaintDuringLayout(const RenderElement&);

    // Shared between SVG renderers and resources.
    static void applyStrokeStyleToContext(GraphicsContext*, const RenderStyle&, const RenderElement&);

    // Determines if any ancestor's transform has changed.
    static bool transformToRootChanged(RenderElement*);

    // Helper functions to keep track of whether a renderer has an SVG shadow applied.
    static bool rendererHasSVGShadow(const RenderObject&);
    static void setRendererHasSVGShadow(RenderObject&, bool hasShadow);

    static void childAdded(RenderElement& parent, RenderObject& child);
    static void styleChanged(RenderElement&, const RenderStyle*);

#if ENABLE(CSS_COMPOSITING)
    static bool isolatesBlending(const RenderStyle&);
    static void updateMaskedAncestorShouldIsolateBlending(const RenderElement&);
#endif

    // FIXME: These methods do not belong here.
    static const RenderSVGRoot& findTreeRootObject(const RenderElement&);

private:
    // This class is not constructable.
    SVGRenderSupport();
    ~SVGRenderSupport();
};

} // namespace WebCore

#endif // SVGRenderSupport_h
