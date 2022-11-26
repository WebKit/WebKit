/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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

#include "CSSFilter.h"
#include "CachedSVGDocument.h"
#include "CachedSVGDocumentReference.h"
#include "FilterTargetSwitcher.h"
#include "Logging.h"
#include "RenderSVGResourceFilter.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

RenderLayerFilters::RenderLayerFilters(RenderLayer& layer)
    : m_layer(layer)
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

void RenderLayerFilters::notifyFinished(CachedResource&, const NetworkLoadMetrics&)
{
    // FIXME: This really shouldn't have to invalidate layer composition,
    // but tests like css3/filters/effect-reference-delete.html fail if that doesn't happen.
    if (auto* enclosingElement = m_layer.enclosingElement())
        enclosingElement->invalidateStyleAndLayerComposition();
    m_layer.renderer().repaint();
}

void RenderLayerFilters::updateReferenceFilterClients(const FilterOperations& operations)
{
    removeReferenceFilterClients();

    for (auto& operation : operations.operations()) {
        if (!is<ReferenceFilterOperation>(*operation))
            continue;

        auto& referenceOperation = downcast<ReferenceFilterOperation>(*operation);
        auto* documentReference = referenceOperation.cachedSVGDocumentReference();
        if (auto* cachedSVGDocument = documentReference ? documentReference->document() : nullptr) {
            // Reference is external; wait for notifyFinished().
            cachedSVGDocument->addClient(*this);
            m_externalSVGReferences.append(cachedSVGDocument);
        } else {
            // Reference is internal; add layer as a client so we can trigger filter repaint on SVG attribute change.
            auto* filterElement = m_layer.renderer().document().getElementById(referenceOperation.fragment());
            if (!filterElement)
                continue;
            auto* renderer = filterElement->renderer();
            if (!is<RenderSVGResourceFilter>(renderer))
                continue;
            downcast<RenderSVGResourceFilter>(*renderer).addClientRenderLayer(&m_layer);
            m_internalSVGReferences.append(filterElement);
        }
    }
}

void RenderLayerFilters::removeReferenceFilterClients()
{
    for (auto& resourceHandle : m_externalSVGReferences)
        resourceHandle->removeClient(*this);

    m_externalSVGReferences.clear();

    for (auto& filterElement : m_internalSVGReferences) {
        if (auto* renderer = filterElement->renderer())
            downcast<RenderSVGResourceContainer>(*renderer).removeClientRenderLayer(&m_layer);
    }
    m_internalSVGReferences.clear();
}

bool RenderLayerFilters::isIdentity(RenderElement& renderer)
{
    const auto& operations = renderer.style().filter();
    return CSSFilter::isIdentity(renderer, operations);
}

IntOutsets RenderLayerFilters::calculateOutsets(RenderElement& renderer, const FloatRect& targetBoundingBox)
{
    const auto& operations = renderer.style().filter();
    
    if (!operations.hasFilterThatMovesPixels())
        return { };

    return CSSFilter::calculateOutsets(renderer, operations, targetBoundingBox);
}

GraphicsContext* RenderLayerFilters::beginFilterEffect(RenderElement& renderer, GraphicsContext& context, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect, const LayoutRect& layerRepaintRect)
{
    auto expandedDirtyRect = dirtyRect;
    auto targetBoundingBox = intersection(filterBoxRect, dirtyRect);

    auto outsets = calculateOutsets(renderer, targetBoundingBox);
    if (!outsets.isZero()) {
        LayoutBoxExtent flippedOutsets { outsets.bottom(), outsets.left(), outsets.top(), outsets.right() };
        expandedDirtyRect.expand(flippedOutsets);
    }

    // Calculate targetBoundingBox since it will be used if the filter is created.
    targetBoundingBox = intersection(filterBoxRect, expandedDirtyRect);
    if (targetBoundingBox.isEmpty())
        return nullptr;

    if (!m_filter || m_targetBoundingBox != targetBoundingBox) {
        m_targetBoundingBox = targetBoundingBox;
        // FIXME: This rebuilds the entire effects chain even if the filter style didn't change.
        m_filter = CSSFilter::create(renderer, renderer.style().filter(), m_preferredFilterRenderingModes, m_filterScale, Filter::ClipOperation::Unite, m_targetBoundingBox, context);
    }

    if (!m_filter)
        return nullptr;

    auto& filter = *m_filter;

    // For CSSFilter, filterRegion = targetBoundingBox + filter->outsets()
    auto filterRegion = targetBoundingBox;
    if (filter.hasFilterThatMovesPixels())
        filterRegion.expand(toLayoutBoxExtent(outsets));

    if (filterRegion.isEmpty())
        return nullptr;

    // For CSSFilter, sourceImageRect = filterRegion.
    bool hasUpdatedBackingStore = false;
    if (m_filterRegion != filterRegion) {
        m_filterRegion = filterRegion;
        hasUpdatedBackingStore = true;
    }

    if (!filter.hasFilterThatMovesPixels())
        m_repaintRect = dirtyRect;
    else if (hasUpdatedBackingStore)
        m_repaintRect = filterRegion;
    else {
        m_repaintRect = dirtyRect;
        m_repaintRect.unite(layerRepaintRect);
        m_repaintRect.intersect(filterRegion);
    }

    resetDirtySourceRect();
    filter.setFilterRegion(m_filterRegion);

    if (!m_targetSwitcher || hasUpdatedBackingStore)
        m_targetSwitcher = FilterTargetSwitcher::create(context, filter, filterRegion, DestinationColorSpace::SRGB());

    if (!m_targetSwitcher)
        return nullptr;

    m_targetSwitcher->willDrawSourceImage(context, m_repaintRect);

    return m_targetSwitcher->drawingContext(context);
}

void RenderLayerFilters::applyFilterEffect(GraphicsContext& destinationContext)
{
    LOG_WITH_STREAM(Filters, stream << "\nRenderLayerFilters " << this << " applyFilterEffect");

    ASSERT(m_targetSwitcher);
    m_targetSwitcher->didDrawSourceImage(destinationContext);

    LOG_WITH_STREAM(Filters, stream << "RenderLayerFilters " << this << " applyFilterEffect done\n");
}

} // namespace WebCore
