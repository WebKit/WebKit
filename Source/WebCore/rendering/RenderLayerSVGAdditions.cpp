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
#include "RenderLayer.h"

#include "CSSFilterRenderer.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "ReferencedSVGResources.h"
#include "RenderAncestorIterator.h"
#include "RenderBoxInlines.h"
#include "RenderDescendantIterator.h"
#include "RenderElementInlines.h"
#include "RenderIterator.h"
#include "RenderLayerBacking.h"
#include "RenderLayerFilters.h"
#include "RenderLayerInlines.h"
#include "RenderLayerModelObject.h"
#include "RenderLayerSVGAdditionsInlines.h"
#include "RenderObjectInlines.h"
#include "RenderSVGContainer.h"
#include "RenderSVGForeignObject.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGInline.h"
#include "RenderSVGModelObject.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceContainer.h"
#include "RenderSVGRoot.h"
#include "RenderSVGText.h"
#include "RenderSVGViewportContainer.h"
#include "SVGFilterElement.h"
#include "SVGRenderSupport.h"
#include "Settings.h"
#include "StyleTransformResolver.h"
#include "TransformPaintScope.h"
#include "TransformationMatrix.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RenderLayer::SVGData);

bool RenderLayer::hasVisibleContentForPaintingForSVG() const
{
    ASSERT(m_svgData);
    ASSERT(renderer().document().settings().layerBasedSVGEngineEnabled());

    // Layers belonging to a resource container (mask, clipPath, marker, pattern) — either as
    // the container itself or a descendant — are only visible when actively painting via
    // paintResourceLayerForSVG(). The flag lives on the resource container's own layer.
    CheckedPtr resourceContainer = dynamicDowncast<RenderSVGResourceContainer>(renderer());
    if (!resourceContainer)
        resourceContainer = dynamicDowncast<RenderSVGResourceContainer>(m_svgData->enclosingHiddenOrResourceContainer.get());
    if (resourceContainer) {
        ASSERT(resourceContainer->hasLayer());
        return resourceContainer->layer()->isPaintingResourceLayerForSVG();
    }

    // Hidden SVG containers (<defs> / <symbol> ...) and their children are never painted directly.
    if (m_svgData->enclosingHiddenOrResourceContainer)
        return false;

    return hasVisibleContent();
}

void RenderLayer::paintResourceLayerForSVG(GraphicsContext& context, const AffineTransform& layerContentTransform)
{
    ASSERT(m_svgData);
    ASSERT(renderer().document().settings().layerBasedSVGEngineEnabled());

    bool wasPaintingSVGResourceLayer = m_svgData->isPaintingResourceLayer;
    m_svgData->isPaintingResourceLayer = true;
    context.concatCTM(layerContentTransform);

    auto localPaintDirtyRect = LayoutRect::infiniteRect();

    LayerPaintingInfo paintingInfo(this, localPaintDirtyRect, PaintBehavior::Normal, LayoutSize());

    OptionSet<PaintLayerFlag> flags { PaintLayerFlag::TemporaryClipRects };
    if (!renderer().hasNonVisibleOverflow())
        flags.add({ PaintLayerFlag::PaintingOverflowContents, PaintLayerFlag::PaintingOverflowContentsRoot });

    paintLayer(context, paintingInfo, flags);

    m_svgData->isPaintingResourceLayer = wasPaintingSVGResourceLayer;
}

bool RenderLayer::setupClipPathIfNeededForSVG(OptionSet<PaintLayerFlag>& paintFlags)
{
    ASSERT(m_svgData);
    ASSERT(renderer().document().settings().layerBasedSVGEngineEnabled());

    // Applying clip-path on <clipPath> enforces us to use mask based clipping, so disable path based clipping.
    // If isPaintingResourceLayerForSVG() is true, this function was invoked via paintResourceLayerForSVG() -- clipping on <clipPath> is already
    // handled in RenderSVGResourceClipper::applyMaskClipping(), so do not set paintSVGClippingMask to true here.
    if (!is<RenderSVGResourceClipper>(m_svgData->enclosingHiddenOrResourceContainer.get()))
        return false;

    paintFlags.set(PaintLayerFlag::PaintingSVGClippingMask, !m_svgData->isPaintingResourceLayer);
    return true;
}

bool RenderLayer::paintForegroundForFragmentsForSVG(const LayerFragments& layerFragments, GraphicsContext& context, const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintBehavior> localPaintBehavior, RenderObject* subtreePaintRootForRenderer)
{
    ASSERT(m_svgData);
    ASSERT(renderer().document().settings().layerBasedSVGEngineEnabled());

    if (!is<RenderSVGModelObject>(renderer()) || is<RenderSVGContainer>(renderer()))
        return false;

    // SVG containers need to propagate paint phases. This could be saved if we remember somewhere if a SVG subtree
    // contains e.g. LegacyRenderSVGForeignObject objects that do need the individual paint phases. For SVG shapes & SVG images
    // we can avoid the multiple paintForegroundForFragmentsWithPhase() calls.
    if (localPaintingInfo.paintBehavior.containsAny({ PaintBehavior::SelectionOnly, PaintBehavior::SelectionAndBackgroundsOnly }))
        return true;

    paintForegroundForFragmentsWithPhase(PaintPhase::Foreground, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
    return true;
}

void RenderLayer::paintNegativeZOrderChildrenForSVG(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    ASSERT(m_svgData);

    // foreignObject uses HTML-style z-order painting.
    if (renderer().isRenderSVGForeignObject()) {
        paintList(negativeZOrderLayers(), context, paintingInfo, paintFlags);
        return;
    }

    // SVG: no-op. Negative z-order children are handled in DOM-order painting
    // (paintChildrenInDOMOrderForSVG handles all children including negative z-index).
}

void RenderLayer::paintForegroundChildrenForSVG(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintLayerFlag> paintFlags, const LayerFragments& layerFragments, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRoot, std::optional<WTF::Range<unsigned>> svgPaintOrderItemRange)
{
    ASSERT(m_svgData);

    // foreignObject uses HTML-style z-order painting.
    if (renderer().isRenderSVGForeignObject()) {
        // HTML layers and foreignObject use normal z-order list painting.
        paintList(normalFlowLayers(), context, paintingInfo, paintFlags);
        paintList(positiveZOrderLayers(), context, localPaintingInfo, paintFlags);
        return;
    }

    paintChildrenInDOMOrderForSVG(context, localPaintingInfo, paintFlags, layerFragments, paintBehavior, subtreePaintRoot, svgPaintOrderItemRange);
}

RenderLayer::HitLayer RenderLayer::hitTestChildrenForSVG(RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result, const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffsetForDescendants)
{
    ASSERT(m_svgData);
    ASSERT(!renderer().isRenderSVGForeignObject());

    // RenderSVGRoot: viewport-clip points outside the SVG content box.
    if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(renderer())) {
        if (svgRoot->shouldApplyViewportClip() && !hitTestLocation.intersects(svgRoot->contentBoxRect()))
            return { };
    }

    return hitTestChildrenInDOMOrderForSVG(rootLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffsetForDescendants);
}

bool RenderLayer::shouldSkipRepaintAfterLayoutForSVG() const
{
    ASSERT(m_svgData);
    ASSERT(renderer().document().settings().layerBasedSVGEngineEnabled());

    // The SVG containers themselves never trigger repaints, only their contents are allowed to.
    return is<RenderSVGContainer>(renderer()) && !shouldPaintWithFilters();
}

bool RenderLayer::isCompositedSVGPaintOrderChild() const
{
    // A child that paints outside the container's DOM-order walk: it owns a backing store, or hosts a
    // composited descendant parented under its layer. Either way, trailing non-composited siblings must
    // paint above it through an overlay segment layer.
    return compositedWithOwnBackingStore(*this) || hasCompositingDescendant();
}

bool RenderLayer::paintsInlineInSVGContainer() const
{
    // A layer without its own backing store is painted inline by the container's DOM-order walk.
    return !compositedWithOwnBackingStore(*this);
}

bool RenderLayer::isFlattenedByEnclosingSVGReferenceFilter() const
{
    // A non-compositable (reference) SVG filter renders its whole subtree into a single software buffer,
    // so a descendant must not composite independently, which would let it escape the filter. Walk the
    // SVG layer ancestry (below the SVG root, whose reference filter is handled separately) for such a
    // filter.
    for (CheckedPtr ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        auto& renderer = ancestor->renderer();
        if (!renderer.isSVGLayerAwareRenderer() || renderer.isRenderSVGRoot())
            return false;
        if (renderer.style().filter().hasReferenceFilter())
            return true;
    }
    return false;
}

bool RenderLayer::shouldSkipHitTestForSVG() const
{
    ASSERT(m_svgData);
    ASSERT(renderer().document().settings().layerBasedSVGEngineEnabled());

    // SVG resource layers and their children are never hit tested.
    if (is<RenderSVGResourceContainer>(m_svgData->enclosingHiddenOrResourceContainer.get()))
        return true;

    // Hidden SVG containers (<defs> / <symbol> ...) are never hit tested directly.
    if (is<RenderSVGHiddenContainer>(renderer()))
        return true;

    return false;
}

bool RenderLayer::hasFailedFilterForSVG() const
{
    ASSERT(m_svgData);
    ASSERT(renderer().document().settings().layerBasedSVGEngineEnabled());

    // Per the SVG spec, if a filter is referenced but cannot be applied (non-existent
    // reference, empty filter, etc.), the element must not be rendered — the filter
    // produces transparent black, making the element invisible. The CSS Filter Effects
    // spec differs — a failed filter means "no effect" (painted normally). Therefore
    // treat SVG renderers differently, obeying to the SVG rules.
    if (!m_filters || m_filters->filter() || !renderer().style().filter().isReferenceFilter())
        return false;
    return WTF::switchOn(renderer().style().filter().first(),
        [&](const Style::FilterReference& reference) {
            return ReferencedSVGResources::referencedFilterElement(protect(renderer().treeScopeForSVGReferences()), reference) != nullptr;
        },
        [&](const auto&) { return false; }
    );
}

void RenderLayer::updateAncestorDependentStateForSVG()
{
    ASSERT(m_svgData);
    ASSERT(renderer().document().settings().layerBasedSVGEngineEnabled());
    m_svgData->enclosingHiddenOrResourceContainer = ancestorsOfType<RenderSVGHiddenContainer>(renderer()).first();
}

void RenderLayer::dirtyChildrenInDOMOrderForSVG()
{
    ASSERT(m_svgData);
    m_svgData->childrenInDOMOrder.shrink(0); // Use shrink(0) instead of clear() to retain our capacity.
    m_svgData->childrenInDOMOrderDirty = true;

    // Segment ranges index into this flat list, so a rebuild invalidates them. Schedule a configuration
    // update so the segments are recomputed off the fresh list.
    if (isComposited() && backing()->hasSVGPaintOrderSegments())
        setNeedsCompositingConfigurationUpdate();
}

void RenderLayer::invalidateEnclosingSVGContainerSegmentation()
{
    if (!isSVGLayer())
        return;
    // This layer's composited-child status in its enclosing SVG container may have changed, so that
    // container has to recompute its paint-order segments.
    if (CheckedPtr enclosing = enclosingCompositingLayerForRepaint(ExcludeSelf).layer)
        enclosing->setNeedsCompositingConfigurationUpdate();
}

void RenderLayer::collectChildrenInDOMOrderForSVG()
{
    ASSERT(m_svgData);
    m_svgData->childrenInDOMOrderDirty = false;
    m_svgData->childrenInDOMOrder.shrink(0); // Use shrink(0) instead of clear() to retain our capacity.

    bool anyNonZeroZIndex = false;
    appendChildrenInDOMOrderForSVG(renderer(), { }, anyNonZeroZIndex);

    // Sort by z-index; for equal z-index, stable_sort preserves DOM order.
    // Skip entirely when no child uses z-index — collection order already matches DOM order.
    if (anyNonZeroZIndex) {
        std::stable_sort(m_svgData->childrenInDOMOrder.begin(), m_svgData->childrenInDOMOrder.end(),
            [](const SVGPaintOrderLayerItem& a, const SVGPaintOrderLayerItem& b) {
                return a.zIndex < b.zIndex;
            });
    }
}

// Recursively collect children, splitting non-layered containers that have
// layered descendants to ensure proper DOM-order interleaving. Returns true
// if any layered child was found in this subtree.
bool RenderLayer::appendChildrenInDOMOrderForSVG(RenderElement& parent, LayoutSize ancestorOffset, bool& anyNonZeroZIndex)
{
    auto& allChildren = m_svgData->childrenInDOMOrder;
    bool hasIndependentlyPaintedDescendant = false;
    for (CheckedRef child : childrenOfType<RenderElement>(parent)) {
        // Never directly paint children of <defs>, <linearGradient>, etc.
        if (child->isRenderSVGHiddenContainer())
            continue;

        if (child->hasSelfPaintingLayer()) {
            CheckedRef layerModelObject = downcast<RenderLayerModelObject>(child.get());
            CheckedRef childLayer = *layerModelObject->layer();
            int zIndex = childLayer->zIndex();
            if (zIndex)
                anyNonZeroZIndex = true;
            allChildren.append(SVGPaintOrderLayerItem::makeLayered(child.get(), childLayer.get(), zIndex));
            hasIndependentlyPaintedDescendant = true;
            continue;
        }

        // Transformed non-layer children are painted atomically by the consumer:
        // its transform is applied and children are painted recursively.
        if (child->isTransformed()) {
            allChildren.append(SVGPaintOrderLayerItem::makeAtomic(child.get(), ancestorOffset));
            hasIndependentlyPaintedDescendant = true;
            continue;
        }

        // Leaf nodes (no children) are always painted atomically.
        if (!child->firstChild()) {
            allChildren.append(SVGPaintOrderLayerItem::makeAtomic(child.get(), ancestorOffset));
            continue;
        }

        // Compute the offset that this child contributes to its descendants.
        LayoutSize childOffset = ancestorOffset;
        if (CheckedPtr svgModel = dynamicDowncast<RenderSVGModelObject>(child.get()))
            childOffset += toLayoutSize(svgModel->currentSVGLayoutLocation());

        // We don't yet know whether this non-layered container has any
        // independently painted descendants (layered or transformed non-layer
        // children), so recurse speculatively and decide what to keep based on
        // the result.
        //
        // Example:
        //   <g id="A">
        //     <rect id="r1"/>
        //     <g id="B" style="z-index: 5; opacity: 0.5;"/>   <!-- layered -->
        //     <rect id="r2"/>
        //   </g>
        //
        //   When recursing into A, the inner walk visits r1 (leaf, append), then B
        //   (layered, append, mark independently-painted), then r2 (leaf, append).
        //   The speculative segment is now [r1, B, r2] and
        //   subtreeHasIndependentlyPaintedDescendant=true, so we fall into Case B
        //   and additionally append A with the split flag. After z-sort the final
        //   list is [r1, r2, A(split), B] — B last because z=5. The consumer then
        //   paints r1 and r2 at A's offset, then A(split) which paints only A's
        //   own outline (no child recursion), then B on top. Without the split
        //   flag, A's normal paint would recurse into r1/B/r2 and all three would
        //   be painted twice — once via A's recursion, once via their own list
        //   entries.
        //
        // Case A — no independently painted descendants: the subtree paints
        // atomically as part of the container's own normal paint walk. Discard
        // the speculative entries (shrink to startIndex) and append a single
        // entry for the container. Without the shrink, A's normal recursive
        // paint would visit children that are still present in the list,
        // causing the same double-paint problem.
        //
        // Case B — independently painted descendants exist: keep their entries
        // (so they sort into the overall z-order) and append a "split" entry
        // for the container itself. The consumer paints only the container's
        // own non-content contribution (outlines) and skips child recursion,
        // since each independently painted descendant is painted from the
        // sorted list.
        size_t startIndex = allChildren.size();
        bool subtreeHasIndependentlyPaintedDescendant = appendChildrenInDOMOrderForSVG(child.get(), childOffset, anyNonZeroZIndex);
        if (subtreeHasIndependentlyPaintedDescendant) {
            allChildren.append(SVGPaintOrderLayerItem::makeOutlineOnly(child.get(), ancestorOffset));
            hasIndependentlyPaintedDescendant = true;
        } else {
            allChildren.shrink(startIndex);
            allChildren.append(SVGPaintOrderLayerItem::makeAtomic(child.get(), ancestorOffset));
        }
    }
    return hasIndependentlyPaintedDescendant;
}

const Vector<SVGPaintOrderLayerItem>& RenderLayer::childrenInDOMOrderForSVG()
{
    ASSERT(m_svgData);
    if (m_svgData->childrenInDOMOrderDirty)
        collectChildrenInDOMOrderForSVG();
    return m_svgData->childrenInDOMOrder;
}

void RenderLayer::paintNonLayerChildForFragmentsForSVG(RenderElement& childRenderer, const LayoutSize& accumulatedAncestorOffset,
    PaintPhase phase, const LayerFragments& layerFragments, GraphicsContext& context, const LayerPaintingInfo& paintingInfo,
    OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRootForRenderer, const LayoutPoint& containerBaseOffset, bool isSVGRoot)
{
    LayoutPoint svgRootScrollAdjustment;
    if (isSVGRoot)
        svgRootScrollAdjustment = LayoutPoint(-downcast<RenderSVGRoot>(renderer()).scrollPosition());

    for (const auto& fragment : layerFragments) {
        if (!fragment.shouldPaintContent || fragment.dirtyForegroundRect().isEmpty())
            continue;

        GraphicsContextStateSaver stateSaver(context, false);
        RegionContextStateSaver regionContextStateSaver(paintingInfo.regionContext);
        clipToRect(context, stateSaver, regionContextStateSaver, paintingInfo, paintBehavior, fragment.dirtyForegroundRect());

        PaintInfo paintInfo(context, fragment.dirtyForegroundRect().rect(),
            phase, paintBehavior, subtreePaintRootForRenderer,
            nullptr, nullptr, &paintingInfo.rootLayer->renderer(), this,
            paintingInfo.requireSecurityOriginAccessForWidgets);
        if (phase == PaintPhase::Foreground)
            paintInfo.overlapTestRequests = paintingInfo.overlapTestRequests;
        paintInfo.updateSubtreePaintRootForChildren(&renderer());

        auto containerPaintOffset = paintOffsetForRenderer(fragment, paintingInfo);
        auto childPaintOffset = containerBaseOffset.isZero() ? containerPaintOffset : containerPaintOffset + containerBaseOffset;
        if (isSVGRoot)
            childPaintOffset.moveBy(svgRootScrollAdjustment);

        auto finalOffset = childPaintOffset + accumulatedAncestorOffset;
        childRenderer.paint(paintInfo, finalOffset);
    }
}

void RenderLayer::paintChildrenInDOMOrderForSVG(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags,
    const LayerFragments& layerFragments, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRootForRenderer, std::optional<WTF::Range<unsigned>> svgPaintOrderItemRange)
{
    ASSERT(m_svgData);
    auto& allChildren = childrenInDOMOrderForSVG();
    if (allChildren.isEmpty())
        return;

    bool isSVGRoot = is<RenderSVGRoot>(renderer());
    LayoutPoint containerBaseOffset;
    LayoutPoint layerResourceOffset;

    if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer())) {
        containerBaseOffset = svgModelObject->currentSVGLayoutLocation();
        if (isPaintingResourceLayerForSVG()) {
            layerResourceOffset = svgModelObject->nominalSVGLayoutLocation();
            containerBaseOffset.moveBy(layerResourceOffset);
        }
    } else if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(renderer()))
        containerBaseOffset = svgRoot->location();

    // When this layer paints as a single paint-order segment (paintIntoLayer passes the range of the
    // GraphicsLayer being painted), walk only that segment's half-open range of the flat list. The
    // rootLayer == this check keeps the range tied to this layer: a child layer paints through its own
    // paintLayer call, which carries no range, so it cannot leak into the child's flat list.
    size_t beginIndex = 0;
    size_t endIndex = allChildren.size();
    if (svgPaintOrderItemRange && paintingInfo.rootLayer == this) {
        ASSERT(svgPaintOrderItemRange->end() <= allChildren.size());
        beginIndex = svgPaintOrderItemRange->begin();
        endIndex = std::min<size_t>(svgPaintOrderItemRange->end(), allChildren.size());
    }

    for (size_t index = beginIndex; index < endIndex; ++index) {
        auto& childToPaint = allChildren[index];
        if (CheckedPtr childLayer = childToPaint.layer.get()) {
            // Children that own a backing store paint through the compositor and an overlay segment layer
            // paints above them. A composited child that paints into a composited ancestor has no own
            // GraphicsLayer, so it must still paint inline here (see paintsInlineInSVGContainer()).
            if (!childLayer->paintsInlineInSVGContainer() && !paintBehavior.contains(PaintBehavior::FlattenCompositingLayers))
                continue;

            if (isPaintingResourceLayerForSVG() && !layerResourceOffset.isZero()) {
                GraphicsContextStateSaver stateSaver(context);
                context.translate(layerResourceOffset.x(), layerResourceOffset.y());
                childLayer->paintLayer(context, paintingInfo, paintFlags);
            } else
                childLayer->paintLayer(context, paintingInfo, paintFlags);
            continue;
        }

        CheckedPtr childRendererPtr = childToPaint.renderer.get();
        if (!childRendererPtr)
            continue;
        CheckedRef childRenderer = *childRendererPtr;

        // Transformed non-layer children: apply the transform and recursively paint the subtree.
        if (childRenderer->isTransformed()) {
            // When this container is itself a transformed SVG layer (e.g. a layered <use>), its CTM is
            // positioned at its own nominalSVGLayoutLocation (objectBoundingBox top-left). The transform
            // recursion paints the child additively in that CTM, so the child would inherit a spurious
            // shift. Cancel it by folding -nominalCorrection into the child's transform scope (see
            // paintRendererByApplyingTransformForSVG). No-op for SVG root / viewport (nominal == 0) and
            // when not painting as a layer-scope root. Since every transformed container is layered, the
            // only transformed renderers reaching this recursion are leaves and the anonymous outermost
            // viewport container, so no layer descendant ever needs the inverse correction.
            LayoutSize nominalCorrection;
            if (this == paintingInfo.rootLayer && !isPaintingResourceLayerForSVG() && renderer().isSVGLayerAwareRenderer() && !renderer().isRenderSVGRoot())
                nominalCorrection = toLayoutSize(renderer().nominalSVGLayoutLocation());

            for (const auto& fragment : layerFragments) {
                if (!fragment.shouldPaintContent || fragment.dirtyForegroundRect().isEmpty())
                    continue;

                GraphicsContextStateSaver stateSaver(context, false);
                RegionContextStateSaver regionContextStateSaver(paintingInfo.regionContext);
                clipToRect(context, stateSaver, regionContextStateSaver, paintingInfo, paintBehavior, fragment.dirtyForegroundRect());

                // nominalCorrection is applied inside the scope, so TransformPaintScope concatenates it
                // onto the CTM and inverse-maps paintDirtyRect through it together. The leaf
                // visual-overflow cull (RenderSVGShape::paint) then compares the shifted subtree against
                // a matching damage rect, so overhanging leaves survive incremental repaints
                // (svg/W3C-SVG-1.1/animate-elem-80-t.svg).
                paintRendererByApplyingTransformForSVG(context, childRenderer,
                    childToPaint.accumulatedAncestorOffset, paintingInfo, paintFlags,
                    paintBehavior, subtreePaintRootForRenderer, nominalCorrection);
            }
            continue;
        }

        for (auto phase : childToPaint.phasesToPaint) {
            paintNonLayerChildForFragmentsForSVG(childRenderer.get(), childToPaint.accumulatedAncestorOffset,
                phase, layerFragments, context, paintingInfo, paintBehavior, subtreePaintRootForRenderer, containerBaseOffset, isSVGRoot);
        }
    }
}

std::optional<RenderLayer::SVGRendererTransform> RenderLayer::computeRendererTransformForSVG(
    CheckedRef<RenderElement> rendererRef, const LayoutSize& positionOffset) const
{
    CheckedRef layerModelObject = downcast<RenderLayerModelObject>(rendererRef.get());
    TransformationMatrix transform;
    CheckedRef style = layerModelObject->style();
    auto referenceBoxRect = layerModelObject->transformReferenceBoxRect(style);

    // For non-layer renderers, undo the alignReferenceBox shift applied in transformReferenceBoxRect().
    if (!rendererRef->hasSelfPaintingLayer() && rendererRef->isSVGLayerAwareRenderer())
        referenceBoxRect.moveBy(layerModelObject->nominalSVGLayoutLocation());

    layerModelObject->applyTransform(transform, style, referenceBoxRect, Style::TransformResolver::allTransformOperations);

    // For the outermost viewport container (anonymous child of RenderSVGRoot), apply the
    // content-box origin offset (border+padding).
    if (auto* viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(rendererRef.get()); viewportContainer && viewportContainer->isAnonymous()) {
        if (CheckedPtr svgRoot = dynamicDowncast<RenderSVGRoot>(viewportContainer->parent())) {
            auto contentBoxLocation = svgRoot->contentBoxLocation();
            if (!contentBoxLocation.isZero())
                transform.translateRight(contentBoxLocation.x(), contentBoxLocation.y());
        }
    }

    if (!transform.isInvertible())
        return std::nullopt;

    bool isOutermostViewportContainer = is<RenderSVGViewportContainer>(rendererRef.get()) && rendererRef->isAnonymous();
    LayoutSize containerOffset;
    if (rendererRef->hasSelfPaintingLayer() && !isOutermostViewportContainer) {
        containerOffset = positionOffset;
        if (auto* box = dynamicDowncast<RenderBox>(rendererRef.get()))
            containerOffset += toLayoutSize(box->location());
        else if (auto* svgModel = dynamicDowncast<RenderSVGModelObject>(rendererRef.get()))
            containerOffset += toLayoutSize(svgModel->currentSVGLayoutLocation());
    }

    transform.translateRight(containerOffset.width().toFloat(), containerOffset.height().toFloat());
    return SVGRendererTransform { transform, containerOffset };
}

void RenderLayer::paintRendererByApplyingTransformForSVG(GraphicsContext& context, CheckedRef<RenderElement> rendererToPaint,
    const LayoutSize& positionOffset, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags,
    OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRoot, const LayoutSize& nominalPreTranslation)
{
    ASSERT(rendererToPaint->isTransformed());
    auto rendererTransform = computeRendererTransformForSVG(rendererToPaint, positionOffset);
    if (!rendererTransform)
        return;

    Ref document = rendererToPaint->document();
    float deviceScaleFactor = document->deviceScaleFactor();

    ASSERT(paintingInfo.subpixelOffset.isZero());
    LayoutSize adjustedSubpixelOffset;

    // Fold the container's nominal pre-translation into the scope transform rather than a separate
    // context.translate, so TransformPaintScope concatenates it onto the CTM and inverse-maps
    // paintDirtyRect through it together, keeping content and the leaf visual-overflow cull in one
    // coordinate space. translateRight post-multiplies, placing the offset in this container's own
    // space, where nominal lives.
    auto scopeTransform = rendererTransform->transform;
    if (!nominalPreTranslation.isZero())
        scopeTransform.translateRight(-nominalPreTranslation.width().toFloat(), -nominalPreTranslation.height().toFloat());

    TransformPaintScope scope(context, paintingInfo, scopeTransform, deviceScaleFactor, adjustedSubpixelOffset);

    auto adjustedPaintFlags = paintFlags;
    adjustedPaintFlags.remove(PaintLayerFlag::PaintingOverflowContents);

    if (rendererToPaint->hasSelfPaintingLayer()) {
        CheckedRef layerModelObject = downcast<RenderLayerModelObject>(rendererToPaint.get());
        CheckedPtr childLayer = layerModelObject->layer();

        LayerPaintingInfo childPaintingInfo(childLayer.get(), scope.transformedPaintingInfo().paintDirtyRect, paintBehavior, LayoutSize());
        childLayer->paintLayer(context, childPaintingInfo, adjustedPaintFlags | PaintLayerFlag::AppliedTransform);
    } else {
        // Non-layer renderer painted directly within the transform scope.
        auto& transformedPaintingInfo = scope.transformedPaintingInfo();
        auto paintInScope = [&](PaintPhase phase, const LayoutPoint& offset) {
            PaintInfo paintInfo(context, transformedPaintingInfo.paintDirtyRect, phase, paintBehavior, subtreePaintRoot,
                nullptr, nullptr, &transformedPaintingInfo.rootLayer->renderer(), this,
                transformedPaintingInfo.requireSecurityOriginAccessForWidgets);
            rendererToPaint->paint(paintInfo, offset);
        };

        // A renderer's own paint() positions content at paintOffset + currentSVGLayoutLocation() - at a
        // transform-scope root we want it at the visual top-left, so pass (nominal - current). This is
        // uniform for shapes, images, text and a container's own outline.
        LayoutPoint selfPaintOffset;
        if (rendererToPaint->isSVGLayerAwareRenderer()) {
            CheckedRef layerModelObject = downcast<RenderLayerModelObject>(rendererToPaint.get());
            selfPaintOffset = layerModelObject->nominalSVGLayoutLocation();
            selfPaintOffset.moveBy(-layerModelObject->currentSVGLayoutLocation());
        }

        if (rendererToPaint->isRenderSVGContainer()) {
            // Children recurse from the container's nominal origin (= selfPaintOffset + current) and
            // re-add their own currentSVGLayoutLocation; the anonymous outermost viewport starts at (0, 0).
            LayoutPoint recursionBase;
            if (auto* viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(rendererToPaint.get()); viewportContainer && viewportContainer->isAnonymous()) {
                // Outermost viewport container: coordinate system starts at (0, 0).
            } else if (auto* svgModel = dynamicDowncast<RenderSVGModelObject>(rendererToPaint.get()))
                recursionBase = svgModel->nominalSVGLayoutLocation();
            paintSubtreeWithinTransformScopeForSVG(context, rendererToPaint.get(), recursionBase, transformedPaintingInfo, adjustedPaintFlags, paintBehavior, subtreePaintRoot);
            paintInScope(PaintPhase::SelfOutline, selfPaintOffset);
        } else {
            paintInScope(PaintPhase::Foreground, selfPaintOffset);
            paintInScope(PaintPhase::Outline, selfPaintOffset);
        }
    }
}

void RenderLayer::paintSubtreeWithinTransformScopeForSVG(GraphicsContext& context, RenderElement& container,
    const LayoutPoint& paintOffset, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags,
    OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRoot)
{
    // Apply viewport clipping for nested <svg> elements without layers.
    GraphicsContextStateSaver clipSaver(context, false);
    if (CheckedPtr viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(container)) {
        if (!viewportContainer->hasSelfPaintingLayer() && SVGRenderSupport::isOverflowHidden(*viewportContainer)) {
            clipSaver.save();
            auto clipOffset = viewportContainer->isTransformed() ? LayoutPoint() : paintOffset;
            context.clip(FloatRect(static_cast<const RenderSVGModelObject&>(*viewportContainer).overflowClipRect(clipOffset)));
        }
    }

    Vector<SVGPaintOrderLayerItem> sortedChildren;
    for (CheckedRef child : childrenOfType<RenderElement>(container)) {
        if (child->isRenderSVGHiddenContainer())
            continue;
        if (child->style().display() == Style::DisplayType::None)
            continue;

        if (child->hasSelfPaintingLayer()) {
            CheckedRef layerModelObject = downcast<RenderLayerModelObject>(child.get());
            if (CheckedPtr childLayer = layerModelObject->layer()) {
                sortedChildren.append(SVGPaintOrderLayerItem::makeLayered(child.get(), *childLayer, childLayer->zIndex()));
                continue;
            }
        }
        sortedChildren.append(SVGPaintOrderLayerItem::makeAtomic(child.get(), { }));
    }

    std::stable_sort(sortedChildren.begin(), sortedChildren.end(),
        [](const SVGPaintOrderLayerItem& a, const SVGPaintOrderLayerItem& b) {
            return a.zIndex < b.zIndex;
        });

    for (auto& entry : sortedChildren) {
        CheckedRef child = *entry.renderer;

        if (child->isTransformed()) {
            paintRendererByApplyingTransformForSVG(context, child, toLayoutSize(paintOffset), paintingInfo, paintFlags,
                paintBehavior, subtreePaintRoot);
            continue;
        }

        if (child->hasSelfPaintingLayer()) {
            CheckedRef layerModelObject = downcast<RenderLayerModelObject>(child.get());
            CheckedPtr childLayer = layerModelObject->layer();

            auto adjustedFlags = paintFlags;
            adjustedFlags.remove(PaintLayerFlag::PaintingOverflowContents);

            RenderLayer::LayerPaintingInfo childPaintingInfo(paintingInfo);

            if (isPaintingResourceLayerForSVG() && !paintOffset.isZero()) {
                GraphicsContextStateSaver stateSaver(context);
                context.translate(paintOffset.x(), paintOffset.y());
                childLayer->paintLayer(context, childPaintingInfo, adjustedFlags);
            } else
                childLayer->paintLayer(context, childPaintingInfo, adjustedFlags);
            continue;
        }

        // Non-layer, non-transformed child.
        auto adjustedPaintOffset = paintOffset;
        if (CheckedPtr childSvgModel = dynamicDowncast<RenderSVGModelObject>(child.get()))
            adjustedPaintOffset.moveBy(childSvgModel->currentSVGLayoutLocation());

        if (child->isRenderSVGContainer()) {
            paintSubtreeWithinTransformScopeForSVG(context, child.get(), adjustedPaintOffset, paintingInfo, paintFlags, paintBehavior, subtreePaintRoot);

            PaintInfo outlinePaintInfo(context, paintingInfo.paintDirtyRect, PaintPhase::SelfOutline, paintBehavior, subtreePaintRoot,
                nullptr, nullptr, &paintingInfo.rootLayer->renderer(), this,
                paintingInfo.requireSecurityOriginAccessForWidgets);
            child->paint(outlinePaintInfo, paintOffset);
        } else {
            LayoutRect dirtyRect = paintingInfo.paintDirtyRect;
            PaintInfo paintInfo(context, dirtyRect, PaintPhase::Foreground, paintBehavior, subtreePaintRoot,
                nullptr, nullptr, &paintingInfo.rootLayer->renderer(), this,
                paintingInfo.requireSecurityOriginAccessForWidgets);
            child->paint(paintInfo, paintOffset);

            PaintInfo outlinePaintInfo(paintInfo);
            outlinePaintInfo.phase = PaintPhase::Outline;
            child->paint(outlinePaintInfo, paintOffset);
        }
    }
}

RenderLayer::HitLayer RenderLayer::hitTestChildrenInDOMOrderForSVG(RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffsetForDescendants)
{
    ASSERT(m_svgData);
    auto& allChildren = childrenInDOMOrderForSVG();
    if (allChildren.isEmpty())
        return { };

    CheckedPtr svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer());
    LayoutSize svgOffset = svgModelObject ? toLayoutSize(svgModelObject->nominalSVGLayoutLocation()) : LayoutSize();

    for (int i = allChildren.size() - 1; i >= 0; --i) {
        auto& childToPaint = allChildren[i];

        if (CheckedPtr childLayer = childToPaint.layer.get()) {
            auto hitLayer = childLayer->hitTestLayer(rootLayer, this, request, result, hitTestRect, hitTestLocation, false, transformState, zOffsetForDescendants);
            if (hitLayer.layer)
                return hitLayer;
            continue;
        }

        if (!childToPaint.phasesToPaint.contains(PaintPhase::Foreground))
            continue;

        CheckedPtr childRendererPtr = childToPaint.renderer.get();
        if (!childRendererPtr)
            continue;
        CheckedRef childRenderer = *childRendererPtr;

        if (childRenderer->isTransformed()) {
            auto hitLayer = hitTestRendererByInversingTransformForSVG(childRenderer.get(), childToPaint.accumulatedAncestorOffset, request, result, hitTestRect, hitTestLocation);
            if (hitLayer.layer)
                return hitLayer;
            continue;
        }

        LayoutPoint accumulatedOffset(svgOffset);
        accumulatedOffset += childToPaint.accumulatedAncestorOffset;

        if (childRenderer->nodeAtPoint(request, result, hitTestLocation, accumulatedOffset, HitTestAction::Foreground))
            return { this, 0 };
    }

    return { };
}

RenderLayer::HitLayer RenderLayer::hitTestRendererByInversingTransformForSVG(RenderElement& rendererToTest,
    const LayoutSize& positionOffset, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation)
{
    auto transformResult = computeRendererTransformForSVG(rendererToTest, positionOffset);
    if (!transformResult)
        return { };

    auto inverse = transformResult->transform.inverse();
    if (!inverse)
        return { };

    auto localPoint = inverse->mapPoint(hitTestLocation.point());
    auto localRect = enclosingLayoutRect(inverse->mapRect(FloatRect(hitTestRect)));

    HitTestLocation localLocation { LayoutPoint { localPoint } };

    if (rendererToTest.isRenderSVGContainer()) {
        LayoutPoint accumulatedOffset;
        if (auto* viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(rendererToTest); !viewportContainer || !viewportContainer->isAnonymous()) {
            if (auto* svgModel = dynamicDowncast<RenderSVGModelObject>(rendererToTest))
                accumulatedOffset = svgModel->nominalSVGLayoutLocation();
        }
        return hitTestSubtreeWithinTransformScopeForSVG(rendererToTest, accumulatedOffset, request, result, localRect, localLocation);
    }

    LayoutPoint accumulatedOffset;
    if (CheckedPtr parentRenderer = rendererToTest.parent()) {
        if (CheckedPtr parentSvgModel = dynamicDowncast<RenderSVGModelObject>(*parentRenderer))
            accumulatedOffset = parentSvgModel->nominalSVGLayoutLocation();
    }
    if (rendererToTest.nodeAtPoint(request, result, localLocation, accumulatedOffset, HitTestAction::Foreground))
        return { this, 0 };
    return { };
}

RenderLayer::HitLayer RenderLayer::hitTestSubtreeWithinTransformScopeForSVG(RenderElement& container,
    const LayoutPoint& accumulatedOffset, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation)
{
    for (CheckedPtr child = container.lastChild(); child; child = child->previousSibling()) {
        CheckedPtr childElement = dynamicDowncast<RenderElement>(child.get());
        if (!childElement)
            continue;

        if (childElement->isRenderSVGHiddenContainer() || childElement->style().display() == Style::DisplayType::None)
            continue;

        if (childElement->isTransformed()) {
            if (auto hitLayer = hitTestRendererByInversingTransformForSVG(*childElement, toLayoutSize(accumulatedOffset), request, result, hitTestRect, hitTestLocation); hitLayer.layer)
                return hitLayer;
            continue;
        }

        if (childElement->hasSelfPaintingLayer()) {
            if (childElement->nodeAtPoint(request, result, hitTestLocation, accumulatedOffset, HitTestAction::Foreground))
                return { this, 0 };
            continue;
        }

        auto adjustedOffset = accumulatedOffset;
        if (CheckedPtr childSvgModel = dynamicDowncast<RenderSVGModelObject>(*childElement))
            adjustedOffset.moveBy(childSvgModel->currentSVGLayoutLocation());

        if (childElement->isRenderSVGContainer()) {
            if (CheckedPtr viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(*childElement)) {
                if (SVGRenderSupport::isOverflowHidden(*viewportContainer) && !viewportContainer->viewport().contains(hitTestLocation.point()))
                    continue;
            }
            if (auto hitLayer = hitTestSubtreeWithinTransformScopeForSVG(*childElement, adjustedOffset, request, result, hitTestRect, hitTestLocation); hitLayer.layer)
                return hitLayer;
        } else {
            if (childElement->nodeAtPoint(request, result, hitTestLocation, accumulatedOffset, HitTestAction::Foreground))
                return { this, 0 };
        }
    }

    return { };
}

} // namespace WebCore
