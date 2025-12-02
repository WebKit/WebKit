/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "RenderLayerFilters.h"

#include "CSSFilterRenderer.h"
#include "CachedSVGDocument.h"
#include "CachedSVGDocumentReference.h"
#include "ContainerNodeInlines.h"
#include "GraphicsContextSwitcher.h"
#include "LegacyRenderSVGResourceFilter.h"
#include "Logging.h"
#include "ReferenceFilterOperation.h"
#include "RenderObjectInlines.h"
#include "RenderSVGShape.h"
#include "RenderStyleInlines.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderLayerFilters);

Ref<RenderLayerFilters> RenderLayerFilters::create(RenderLayer& layer)
{
    return adoptRef(*new RenderLayerFilters(layer));
}

RenderLayerFilters::RenderLayerFilters(RenderLayer& layer)
    : m_layer(&layer)
{
}

RenderLayerFilters::~RenderLayerFilters()
{
    removeReferenceFilterClients();
}

bool RenderLayerFilters::hasFilterThatMovesPixels() const
{
    return m_filter && m_filter->hasFilterThatMovesPixels();
}

bool RenderLayerFilters::hasFilterThatShouldBeRestrictedBySecurityOrigin() const
{
    return m_filter && m_filter->hasFilterThatShouldBeRestrictedBySecurityOrigin();
}

bool RenderLayerFilters::hasSourceImage() const
{
    return m_targetSwitcher && m_targetSwitcher->hasSourceImage();
}

void RenderLayerFilters::notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
{
    CheckedPtr layer = m_layer;
    if (!layer)
        return;

    // FIXME: This really shouldn't have to invalidate layer composition,
    // but tests like css3/filters/effect-reference-delete.html fail if that doesn't happen.
    if (auto* enclosingElement = layer->enclosingElement())
        enclosingElement->invalidateStyleAndLayerComposition();
    layer->renderer().repaint();
}

void RenderLayerFilters::updateReferenceFilterClients(const Style::Filter& filter)
{
    removeReferenceFilterClients();

    for (auto& value : filter) {
        Ref operation = value.value;
        RefPtr referenceOperation = dynamicDowncast<Style::ReferenceFilterOperation>(operation);
        if (!referenceOperation)
            continue;

        auto* documentReference = referenceOperation->cachedSVGDocumentReference();
        if (auto* cachedSVGDocument = documentReference ? documentReference->document() : nullptr) {
            // Reference is external; wait for notifyFinished().
            cachedSVGDocument->addClient(*this);
            m_externalSVGReferences.append(cachedSVGDocument);
        } else {
            // Reference is internal; add layer as a client so we can trigger filter repaint on SVG attribute change.
            CheckedPtr layer = m_layer;
            if (!layer)
                continue;
            RefPtr filterElement = layer->renderer().document().getElementById(referenceOperation->fragment());
            if (!filterElement)
                continue;
            CheckedPtr renderer = dynamicDowncast<LegacyRenderSVGResourceFilter>(filterElement->renderer());
            if (!renderer)
                continue;
            renderer->addClientRenderLayer(*layer);
            m_internalSVGReferences.append(WTFMove(filterElement));
        }
    }
}

void RenderLayerFilters::removeReferenceFilterClients()
{
    for (auto& resourceHandle : m_externalSVGReferences)
        resourceHandle->removeClient(*this);

    m_externalSVGReferences.clear();

    if (CheckedPtr layer = m_layer) {
        for (auto& filterElement : m_internalSVGReferences) {
            if (CheckedPtr renderer = filterElement->renderer())
                downcast<LegacyRenderSVGResourceContainer>(*renderer).removeClientRenderLayer(*layer);
        }
    }
    m_internalSVGReferences.clear();
}

bool RenderLayerFilters::isIdentity(RenderElement& renderer)
{
    const auto& filter = renderer.style().filter();
    return CSSFilterRenderer::isIdentity(renderer, filter);
}

IntOutsets RenderLayerFilters::calculateOutsets(RenderElement& renderer, const FloatRect& targetBoundingBox)
{
    const auto& filter = renderer.style().filter();

    if (!filter.hasFilterThatMovesPixels())
        return { };

    return CSSFilterRenderer::calculateOutsets(renderer, filter, targetBoundingBox);
}

GraphicsContext* RenderLayerFilters::beginFilterEffect(RenderElement& renderer, GraphicsContext& context, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect, const LayoutRect& layerRepaintRect, const LayoutRect& clipRect)
{
    auto expandedDirtyRect = dirtyRect;
    auto targetBoundingBox = intersection(filterBoxRect, dirtyRect);

    auto preferredFilterRenderingModes = renderer.page().preferredFilterRenderingModes(context);

    auto outsets = calculateOutsets(renderer, targetBoundingBox);
    if (!outsets.isZero()) {
        LayoutBoxExtent flippedOutsets { outsets.bottom(), outsets.left(), outsets.top(), outsets.right() };
        expandedDirtyRect.expand(flippedOutsets);
    }

    if (is<RenderSVGShape>(renderer))
        targetBoundingBox = enclosingLayoutRect(renderer.objectBoundingBox());
    else {
        // Calculate targetBoundingBox since it will be used if the filter is created.
        targetBoundingBox = intersection(filterBoxRect, expandedDirtyRect);
    }

    if (targetBoundingBox.isEmpty())
        return nullptr;

    if (!m_filter || m_targetBoundingBox != targetBoundingBox || m_preferredFilterRenderingModes != preferredFilterRenderingModes) {
        m_targetBoundingBox = targetBoundingBox;
        // FIXME: This rebuilds the entire effects chain even if the filter style didn't change.
        m_filter = CSSFilterRenderer::create(renderer, renderer.style().filter(), preferredFilterRenderingModes, m_filterScale, m_targetBoundingBox, context);
    }

    if (!m_filter)
        return nullptr;

    Ref filter = *m_filter;
    auto filterRegion = m_targetBoundingBox;

    if (filter->hasFilterThatMovesPixels()) {
        // For CSSFilterRenderer, filterRegion = targetBoundingBox + filter->outsets()
        filterRegion.expand(toLayoutBoxExtent(outsets));
    } else if (auto* shape = dynamicDowncast<RenderSVGShape>(renderer))
        filterRegion = shape->currentSVGLayoutRect();

    if (filterRegion.isEmpty())
        return nullptr;

    // For CSSFilterRenderer, sourceImageRect = filterRegion.
    bool hasUpdatedBackingStore = false;
    if (m_filterRegion != filterRegion || m_preferredFilterRenderingModes != preferredFilterRenderingModes) {
        m_filterRegion = filterRegion;
        m_preferredFilterRenderingModes = preferredFilterRenderingModes;
        hasUpdatedBackingStore = true;
    }

    filter->setFilterRegion(m_filterRegion);

    if (!filter->hasFilterThatMovesPixels())
        m_repaintRect = dirtyRect;
    else if (hasUpdatedBackingStore || !hasSourceImage())
        m_repaintRect = filterRegion;
    else {
        m_repaintRect = dirtyRect;
        m_repaintRect.unite(layerRepaintRect);
        m_repaintRect.intersect(filterRegion);
    }

    resetDirtySourceRect();

    if (!m_targetSwitcher || hasUpdatedBackingStore) {
        FloatRect sourceImageRect;
        if (is<RenderSVGShape>(renderer))
            sourceImageRect = renderer.strokeBoundingBox();
        else
            sourceImageRect = m_targetBoundingBox;
        m_targetSwitcher = GraphicsContextSwitcher::create(context, sourceImageRect, DestinationColorSpace::SRGB(), { WTFMove(filter) });
    }

    if (!m_targetSwitcher)
        return nullptr;

    m_targetSwitcher->beginClipAndDrawSourceImage(context, m_repaintRect, clipRect);

    return m_targetSwitcher->drawingContext(context);
}

void RenderLayerFilters::applyFilterEffect(GraphicsContext& destinationContext)
{
    LOG_WITH_STREAM(Filters, stream << "\nRenderLayerFilters " << this << " applyFilterEffect");

    ASSERT(m_targetSwitcher);
    m_targetSwitcher->endClipAndDrawSourceImage(destinationContext, DestinationColorSpace::SRGB());

    LOG_WITH_STREAM(Filters, stream << "RenderLayerFilters " << this << " applyFilterEffect done\n");
}

} // namespace WebCore
