/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "RenderLayerBacking.h"

#include "GraphicsLayer.h"
#include "RenderElementInlines.h"
#include "RenderLayerInlines.h"
#include "RenderLayerSVGAdditions.h"
#include "RenderObjectInlines.h"
#include "RenderSVGBlock.h"
#include "RenderSVGModelObject.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

bool RenderLayerBacking::updateSVGSegmentLayers()
{
    // Recompute the segment boundaries from the current compositing state. A composited child splits the
    // container's DOM-order paint list (childrenInDOMOrderForSVG) at its index. The segments cover that
    // list in order with no gaps, so non-composited content after a composited child can paint above it.
    // Only SVG container layers have such a flat list to split.
    // FIXME: we make an overlay for every composited child with non-composited content after it, even when
    // that content overlaps no earlier composited child and could just stay in the primary segment. It is
    // only really needed when the content overlaps one (the visible paint-order inversion). Checking each
    // candidate against the earlier composited children's combined bounds would skip those overlays, at the
    // cost of tracking and re-testing bounds.
    auto buildSegments = [&]() -> Vector<SVGPaintOrderSegment> {
        Vector<SVGPaintOrderSegment> segments;
        // RenderSVGForeignObject is an SVG layer but paints its HTML subtree with HTML-style z-order and
        // keeps the foreground-layer path, so it must not also build segments (the two are mutually
        // exclusive, see the assert in updatePaintingPhases).
        if (!m_owningLayer.isSVGLayer() || m_owningLayer.renderer().isRenderSVGForeignObject())
            return segments;

        auto isCompositedChild = [](const SVGPaintOrderLayerItem& item) {
            CheckedPtr layer = item.layer.get();
            return layer && layer->isCompositedSVGPaintOrderChild();
        };

        // A composited child that composites only because it hosts a composited descendant is both a
        // segment boundary and painted inline, so its segment must be emitted even when the run before it
        // is empty, otherwise the child's own content is never painted.
        auto paintsInline = [](const SVGPaintOrderLayerItem& item) {
            CheckedPtr layer = item.layer.get();
            return layer && layer->paintsInlineInSVGContainer();
        };

        auto& items = m_owningLayer.childrenInDOMOrderForSVG();
        unsigned segBegin = 0;
        RenderLayer* precedingCompositedChild = nullptr;
        for (unsigned i = 0; i < items.size(); ++i) {
            if (!isCompositedChild(items[i]))
                continue;
            // The primary segment (the primary layer) is always emitted, covering any non-composited
            // content before the first composited child plus that child. A later composited child gets its
            // own segment when the run ending at it paints something inline: non-composited siblings in the
            // gap before it, or a composited child that also paints inline. Two own-backing children next to
            // each other leave an empty run and get no overlay, since each paints through its own
            // GraphicsLayer. Together the ranges cover every inline-painted index of the flat list.
            if (segments.isEmpty() || segBegin < i || paintsInline(items[i]))
                segments.append({ { segBegin, i + 1 }, precedingCompositedChild, nullptr, { } });
            precedingCompositedChild = items[i].layer.get();
            segBegin = i + 1;
        }
        // Trailing segment after the last composited child, dropped when it is the last item.
        if (!segments.isEmpty() && segBegin < items.size())
            segments.append({ { segBegin, static_cast<unsigned>(items.size()) }, precedingCompositedChild, nullptr, { } });
        return segments;
    };

    auto newSegments = buildSegments();

    // Check for a structural change (count, ranges, or composited-child associations) before changing the existing
    // segments. Callers rebuild the layer hierarchy based on the returned flag.
    bool segmentsChanged = newSegments.size() != m_svgPaintOrderSegments.size();
    if (!segmentsChanged) {
        for (size_t i = 0; i < newSegments.size(); ++i) {
            if (newSegments[i].childIndexRange != m_svgPaintOrderSegments[i].childIndexRange
                || newSegments[i].precedingCompositedChild.get() != m_svgPaintOrderSegments[i].precedingCompositedChild.get()) {
                segmentsChanged = true;
                break;
            }
        }
    }

    // Update the overlay GraphicsLayers to match the segments: the primary segment paints into the primary graphics layer
    // (null graphicsLayer), while every later segment needs its own "(svg segment N)" layer. Reuse existing
    // overlays by position so their backing stores survive a recompute (same as updateForegroundLayer).
    Vector<RefPtr<GraphicsLayer>> reusableLayers;
    for (auto& segment : m_svgPaintOrderSegments) {
        if (segment.graphicsLayer)
            reusableLayers.append(WTF::move(segment.graphicsLayer));
    }

    bool layerChanged = false;
    size_t nextReusable = 0;
    for (size_t i = 1; i < newSegments.size(); ++i) {
        RefPtr<GraphicsLayer> layer;
        if (nextReusable < reusableLayers.size())
            layer = WTF::move(reusableLayers[nextReusable++]);
        else {
            layer = createGraphicsLayer(makeString(m_owningLayer.name(), " (svg segment "_s, i, ')'));
            layer->setDrawsContent(true);
            layerChanged = true;
        }
        newSegments[i].graphicsLayer = WTF::move(layer);
    }

    for (; nextReusable < reusableLayers.size(); ++nextReusable) {
        if (RefPtr<GraphicsLayer> leftover = WTF::move(reusableLayers[nextReusable])) {
            willDestroyLayer(leftover.get());
            GraphicsLayer::unparentAndClear(leftover);
            layerChanged = true;
        }
    }

    m_svgPaintOrderSegments = WTF::move(newSegments);

    // Even when only the composited-child associations change (overlay count unchanged, but a segment now
    // follows a different composited child), the hierarchy still needs rebuilding. We run inside updateConfiguration, part of
    // the backing pass, so we ask for a layer connection here (the same flag the caller sets when
    // updateConfiguration returns true). setNeedsCompositingPaintOrderChildrenUpdate would set a
    // requirements-pass flag that this update has already passed.
    if (segmentsChanged && !layerChanged)
        m_owningLayer.setNeedsCompositingLayerConnection();

    return layerChanged;
}

void RenderLayerBacking::clearSVGSegmentLayers()
{
    for (auto& segment : m_svgPaintOrderSegments) {
        if (segment.graphicsLayer) {
            willDestroyLayer(segment.graphicsLayer.get());
            GraphicsLayer::unparentAndClear(segment.graphicsLayer);
        }
    }
    m_svgPaintOrderSegments.clear();
}

GraphicsLayer* RenderLayerBacking::svgSegmentLayerAfterCompositedChild(const RenderLayer& compositedChild) const
{
    for (auto& segment : m_svgPaintOrderSegments) {
        if (segment.graphicsLayer && segment.precedingCompositedChild.get() == &compositedChild)
            return segment.graphicsLayer.get();
    }
    return nullptr;
}

std::optional<WTF::Range<unsigned>> RenderLayerBacking::svgSegmentRangeForGraphicsLayer(const GraphicsLayer& graphicsLayer) const
{
    if (m_svgPaintOrderSegments.isEmpty())
        return std::nullopt;
    for (auto& segment : m_svgPaintOrderSegments) {
        bool isPrimarySegment = !segment.graphicsLayer && &graphicsLayer == m_graphicsLayer.get();
        if (isPrimarySegment || segment.graphicsLayer.get() == &graphicsLayer)
            return segment.childIndexRange;
    }
    return std::nullopt;
}

FloatRect RenderLayerBacking::computeSVGSegmentBounds(const SVGPaintOrderSegment& segment)
{
    // Bounds are collected in compositedBounds()'s coordinate space and used to crop a non-transformed
    // container's overlays. Only non-transformed containers are cropped, so we never reach here through
    // an intermediate transform.
    //
    // Layer children use the same calculateLayerBounds() machinery as compositedBounds(), so they land in
    // that space directly. Non-layer children come back in the owner renderer's local space. Since
    // compositedBounds() moves its content to its own origin, the two spaces differ by the content's
    // top-left when the content does not start at the container origin. Shift non-layer children by that
    // delta so every segment shares one space and the crop lines up.
    FloatRect bounds;
    auto rendererToCompositedBoundsSpace = m_owningLayer.renderer().repaintRectInLocalCoordinates(RepaintRectCalculation::Fast).location() - FloatPoint(compositedBounds().location());

    auto& items = m_owningLayer.childrenInDOMOrderForSVG();
    for (unsigned i = segment.childIndexRange.begin(); i < segment.childIndexRange.end() && i < items.size(); ++i) {
        CheckedPtr renderer = items[i].renderer.get();
        if (!renderer)
            continue;
        if (CheckedPtr layer = items[i].layer.get()) {
            // A composited child that owns a backing store paints through its own GraphicsLayer, not this
            // overlay, so it does not contribute to the segment's painted bounds.
            if (!layer->paintsInlineInSVGContainer())
                continue;

            // Same machinery compositedBounds() uses for descendant layers, so the result is in the
            // identical coordinate space.
            bounds.unite(FloatRect(layer->calculateLayerBounds(&m_owningLayer, layer->offsetFromAncestor(&m_owningLayer), RenderLayer::defaultCalculateLayerBoundsFlags())));
            continue;
        }

        // An SVG renderer's repaint rect is in its own local coordinates, which already include its layout
        // location. Move back to the renderer's origin first, so localToContainerQuad does not add it a
        // second time. Fast repaint bounds are a safe over-approximation. Shapes, images and containers
        // carry the location through RenderSVGModelObject, text and foreignObject through RenderSVGBlock.
        auto localRect = renderer->repaintRectInLocalCoordinates(RepaintRectCalculation::Fast);
        if (CheckedPtr svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer.get()))
            localRect.moveBy(-svgModelObject->currentSVGLayoutLocation());
        else if (CheckedPtr svgBlock = dynamicDowncast<RenderSVGBlock>(renderer.get()))
            localRect.moveBy(-svgBlock->currentSVGLayoutLocation());
        FloatRect mapped = renderer->localToContainerQuad(FloatQuad(localRect), &m_owningLayer.renderer()).boundingBox();
        mapped.move(-rendererToCompositedBoundsSpace);
        bounds.unite(mapped);
    }
    return bounds;
}

void RenderLayerBacking::updateSVGSegmentLayerGeometry(FloatSize foregroundLikeSize, FloatSize foregroundLikeOffset, GraphicsLayer::ShouldSetNeedsDisplay needsDisplayOnOffsetChange)
{
    // Crop the overlay to its segment's bounds only in the plain (non-scrolled, non-clipping)
    // non-transformed case: offsetFromRenderer and position shift by the segment's origin together,
    // landing the content at the backing's top-left while keeping its on-screen location fixed. A
    // transformed container keeps the full foreground-sized backing (no crop): shifting the origin
    // makes the overlay a positioned child of the transformed primary layer, which the compositor
    // misplaces because the layer's anchor point does not seem to be honoured under the transform.
    // That appears to be the underlying cause and needs a follow-up investigation, so for now the
    // origin is kept foreground-aligned to composite identically.
    bool canCrop = !m_scrolledContentsLayer && !hasClippingLayer() && !m_owningLayer.isTransformed();
    FloatPoint contentOrigin = FloatRect(compositedBounds()).location();

    for (auto& segment : m_svgPaintOrderSegments) {
        segment.segmentBoundsInLayerSpace = { };
        if (!segment.graphicsLayer)
            continue;

        FloatSize segmentSize = foregroundLikeSize;
        FloatSize segmentOffset = foregroundLikeOffset;
        FloatPoint segmentPosition;

        if (canCrop) {
            FloatRect segmentBounds = computeSVGSegmentBounds(segment);
            if (!segmentBounds.isEmpty()) {
                // The segment in foreground-local space, rounded out so the backing always covers its
                // content and clamped to the foreground area.
                FloatRect localBounds { segmentBounds.location() - toFloatSize(contentOrigin), segmentBounds.size() };
                localBounds = FloatRect(enclosingIntRect(localBounds));
                localBounds.intersect({ FloatPoint(), foregroundLikeSize });
                if (!localBounds.isEmpty()) {
                    segment.segmentBoundsInLayerSpace = localBounds;
                    // Shift offsetFromRenderer by the segment's origin so the paint lands at the
                    // backing's top-left. The matching position keeps the on-screen location fixed.
                    segmentSize = localBounds.size();
                    segmentOffset = foregroundLikeOffset + toFloatSize(localBounds.location());
                    segmentPosition = localBounds.location();
                }
            }
        }

        segment.graphicsLayer->setPosition(segmentPosition);
        segment.graphicsLayer->setSize(segmentSize);
        segment.graphicsLayer->setOffsetFromRenderer(segmentOffset, needsDisplayOnOffsetChange);
    }
}

bool RenderLayerBacking::svgSegmentHasInlineContent(const SVGPaintOrderSegment& segment)
{
    // An overlay paints only the children in its segment that paint inline, so it has nothing to draw
    // when its segment holds only composited children that paint through their own GraphicsLayer.
    auto& items = m_owningLayer.childrenInDOMOrderForSVG();
    for (unsigned i = segment.childIndexRange.begin(); i < segment.childIndexRange.end() && i < items.size(); ++i) {
        CheckedPtr layer = items[i].layer.get();
        if (layer ? layer->paintsInlineInSVGContainer() : !!items[i].renderer)
            return true;
    }
    return false;
}

void RenderLayerBacking::updateSVGSegmentLayersDrawsContent(bool hasPaintedContent)
{
    // Gate each overlay here so it never allocates a backing store for content it does not paint.
    for (auto& segment : m_svgPaintOrderSegments) {
        if (segment.graphicsLayer)
            segment.graphicsLayer->setDrawsContent(hasPaintedContent && svgSegmentHasInlineContent(segment));
    }
}

void RenderLayerBacking::setSVGSegmentLayersNeedDisplayInRect(const FloatRect& pixelSnappedRectForPainting, GraphicsLayer::ShouldClipToLayer shouldClip)
{
    // Repaint only the overlays whose segment overlaps the dirty rect. segmentBoundsInLayerSpace (set in
    // updateGeometry) holds the segment in foreground-local space, the same space the dirty rect lands in
    // once the foreground's offset is removed. An empty segmentBoundsInLayerSpace means we do not know the
    // bounds (for example the overlay was not cropped), so just repaint it.
    FloatRect foregroundLocalDirtyRect = pixelSnappedRectForPainting;
    foregroundLocalDirtyRect.move(-m_graphicsLayer->offsetFromRenderer() - m_subpixelOffsetFromRenderer);
    for (auto& segment : m_svgPaintOrderSegments) {
        if (!segment.graphicsLayer || !segment.graphicsLayer->drawsContent())
            continue;
        if (!segment.segmentBoundsInLayerSpace.isEmpty() && !segment.segmentBoundsInLayerSpace.intersects(foregroundLocalDirtyRect))
            continue;
        FloatRect layerDirtyRect = pixelSnappedRectForPainting;
        layerDirtyRect.move(-segment.graphicsLayer->offsetFromRenderer() - m_subpixelOffsetFromRenderer);
        segment.graphicsLayer->setNeedsDisplayInRect(layerDirtyRect, shouldClip);
    }
}

} // namespace WebCore
