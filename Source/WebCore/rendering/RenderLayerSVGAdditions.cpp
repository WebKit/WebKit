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
#include "RenderLayerFilters.h"
#include "RenderLayerInlines.h"
#include "RenderLayerModelObject.h"
#include "RenderLayerSVGAdditionsInlines.h"
#include "RenderSVGContainer.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGModelObject.h"
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

} // namespace WebCore
