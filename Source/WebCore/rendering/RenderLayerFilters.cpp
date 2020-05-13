/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

void RenderLayerFilters::setFilter(RefPtr<CSSFilter>&& filter)
{
    m_filter = WTFMove(filter);
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

void RenderLayerFilters::buildFilter(RenderElement& renderer, float scaleFactor, RenderingMode renderingMode)
{
    if (!m_filter) {
        m_filter = CSSFilter::create();
        m_filter->setFilterScale(scaleFactor);
        m_filter->setRenderingMode(renderingMode);
    } else if (m_filter->filterScale() != scaleFactor) {
        m_filter->setFilterScale(scaleFactor);
        m_filter->clearIntermediateResults();
    }

    // If the filter fails to build, remove it from the layer. It will still attempt to
    // go through regular processing (e.g. compositing), but never apply anything.
    // FIXME: this rebuilds the entire effects chain even if the filter style didn't change.
    if (!m_filter->build(renderer, renderer.style().filter(), FilterConsumer::FilterProperty))
        m_filter = nullptr;
}

GraphicsContext* RenderLayerFilters::beginFilterEffect(GraphicsContext& destinationContext, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect, const LayoutRect& layerRepaintRect)
{
    if (!m_filter)
        return nullptr;

    auto& filter = *m_filter;
    auto filterSourceRect = filter.computeSourceImageRectForDirtyRect(filterBoxRect, dirtyRect);
    if (filterSourceRect.isEmpty())
        return nullptr;

    bool hasUpdatedBackingStore = filter.updateBackingStoreRect(filterSourceRect);
    if (!filter.hasFilterThatMovesPixels())
        m_repaintRect = dirtyRect;
    else {
        if (hasUpdatedBackingStore)
            m_repaintRect = filterSourceRect;
        else {
            m_repaintRect = dirtyRect;
            m_repaintRect.unite(layerRepaintRect);
            m_repaintRect.intersect(filterSourceRect);
        }
    }
    m_paintOffset = filterSourceRect.location();
    resetDirtySourceRect();

    filter.determineFilterPrimitiveSubregion();

    filter.allocateBackingStoreIfNeeded(destinationContext);
    auto* sourceGraphicsContext = filter.inputContext();
    if (!sourceGraphicsContext || filter.filterRegion().isEmpty() || ImageBuffer::sizeNeedsClamping(filter.filterRegion().size()))
        return nullptr;

    // Translate the context so that the contents of the layer is captured in the offscreen memory buffer.
    sourceGraphicsContext->save();
    sourceGraphicsContext->translate(-m_paintOffset);
    sourceGraphicsContext->clearRect(m_repaintRect);
    sourceGraphicsContext->clip(m_repaintRect);

    return sourceGraphicsContext;
}

void RenderLayerFilters::applyFilterEffect(GraphicsContext& destinationContext)
{
    ASSERT(m_filter->inputContext());

    LOG_WITH_STREAM(Filters, stream << "\nRenderLayerFilters " << this << " applyFilterEffect");

    auto& filter = *m_filter;
    filter.inputContext()->restore();

    filter.apply();

    // Get the filtered output and draw it in place.
    LayoutRect destRect = filter.outputRect();
    destRect.move(m_paintOffset.x(), m_paintOffset.y());

    if (auto* outputBuffer = filter.output())
        destinationContext.drawImageBuffer(*outputBuffer, snapRectToDevicePixels(destRect, m_layer.renderer().document().deviceScaleFactor()));

    filter.clearIntermediateResults();

    LOG_WITH_STREAM(Filters, stream << "RenderLayerFilters " << this << " applyFilterEffect done\n");
}

} // namespace WebCore
