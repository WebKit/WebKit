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
#include "ReferencedSVGResources.h"
#include "RenderAncestorIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderElementInlines.h"
#include "RenderLayerFilters.h"
#include "RenderLayerInlines.h"
#include "RenderLayerModelObject.h"
#include "RenderLayerSVGAdditionsInlines.h"
#include "RenderObjectInlines.h"
#include "RenderSVGContainer.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGModelObject.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceContainer.h"
#include "RenderSVGRoot.h"
#include "RenderSVGViewportContainer.h"
#include "SVGFilterElement.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RenderLayer::SVGData);

bool RenderLayer::hasVisibleContentForPaintingForSVG() const
{
    ASSERT(m_svgData);
    ASSERT(renderer().document().settings().layerBasedSVGEngineEnabled());
    if (!m_svgData->enclosingHiddenOrResourceContainer)
        return true;

    // Hidden SVG containers (<defs> / <symbol> ...) and their children are never painted directly.
    CheckedPtr container = m_svgData->enclosingHiddenOrResourceContainer.get();
    if (!is<RenderSVGResourceContainer>(container.get()))
        return false;

    // SVG resource layers and their children are only painted indirectly, via paintResourceLayerForSVG().
    ASSERT(container->hasLayer());
    CheckedPtr containerLayer = container->layer();
    return containerLayer->isPaintingResourceLayerForSVG();
}

void RenderLayer::paintResourceLayerForSVG(GraphicsContext& context, const AffineTransform& layerContentTransform)
{
    ASSERT(m_svgData);
    ASSERT(renderer().document().settings().layerBasedSVGEngineEnabled());

    bool wasPaintingSVGResourceLayer = m_svgData->isPaintingResourceLayer;
    m_svgData->isPaintingResourceLayer = true;
    context.concatCTM(layerContentTransform);

    auto localPaintDirtyRect = LayoutRect::infiniteRect();

    CheckedPtr rootPaintingLayer = [&] () -> RenderLayer* {
        auto* curr = parent();
        while (curr && !(curr->renderer().isAnonymous() && is<RenderSVGViewportContainer>(curr->renderer())))
            curr = curr->parent();
        return curr;
    }();
    ASSERT(rootPaintingLayer);

    LayerPaintingInfo paintingInfo(rootPaintingLayer, localPaintDirtyRect, PaintBehavior::Normal, LayoutSize());

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

bool RenderLayer::shouldSkipRepaintAfterLayoutForSVG() const
{
    ASSERT(m_svgData);
    ASSERT(renderer().document().settings().layerBasedSVGEngineEnabled());

    // The SVG containers themselves never trigger repaints, only their contents are allowed to.
    // SVG container sizes/positions are only ever determined by their children, so they will
    // change as a reaction on a re-position/re-sizing of the children - which already properly
    // trigger repaints.
    return is<RenderSVGContainer>(renderer()) && !shouldPaintWithFilters();
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

} // namespace WebCore
