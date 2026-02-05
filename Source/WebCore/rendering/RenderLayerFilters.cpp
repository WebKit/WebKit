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
#include "RenderObjectInlines.h"
#include "RenderSVGShape.h"
#include "RenderStyle+GettersInlines.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderLayerFilters);

Ref<RenderLayerFilters> RenderLayerFilters::create(RenderLayer& layer, FloatSize scale)
{
    return adoptRef(*new RenderLayerFilters(layer, scale));
}

RenderLayerFilters::RenderLayerFilters(RenderLayer& layer, FloatSize scale)
    : m_layer(&layer)
    , m_filterScale(scale)
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
    CheckedPtr layer = m_layer.get();
    if (!layer)
        return;

    // FIXME: This really shouldn't have to invalidate layer composition,
    // but tests like css3/filters/effect-reference-delete.html fail if that doesn't happen.
    if (RefPtr enclosingElement = layer->enclosingElement())
        enclosingElement->invalidateStyleAndLayerComposition();
    layer->renderer().repaint();
}

void RenderLayerFilters::updateReferenceFilterClients(const Style::Filter& filter)
{
    removeReferenceFilterClients();

    for (auto& value : filter) {
        WTF::switchOn(value,
            [&](const Style::FilterReference& filterReference) {
                RefPtr documentReference = filterReference.cachedSVGDocumentReference;
                if (auto* cachedSVGDocument = documentReference ? documentReference->document() : nullptr) {
                    // Reference is external; wait for notifyFinished().
                    cachedSVGDocument->addClient(*this);
                    m_externalSVGReferences.append(cachedSVGDocument);
                } else {
                    // Reference is internal; add layer as a client so we can trigger filter repaint on SVG attribute change.
                    CheckedPtr layer = m_layer.get();
                    if (!layer)
                        return;
                    RefPtr filterElement = layer->renderer().document().getElementById(filterReference.cachedFragment);
                    if (!filterElement)
                        return;
                    CheckedPtr renderer = dynamicDowncast<LegacyRenderSVGResourceFilter>(filterElement->renderer());
                    if (!renderer)
                        return;
                    renderer->addClientRenderLayer(*layer);
                    m_internalSVGReferences.append(filterElement.releaseNonNull());
                }
            },
            []<CSSValueID C, typename T>(const FunctionNotation<C, T>&) { }
        );
    }
}

void RenderLayerFilters::removeReferenceFilterClients()
{
    for (auto& resourceHandle : m_externalSVGReferences)
        resourceHandle->removeClient(*this);

    m_externalSVGReferences.clear();

    if (CheckedPtr layer = m_layer.get()) {
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
    auto preferredFilterRenderingModes = renderer.page().preferredFilterRenderingModes(context);
    auto outsets = calculateOutsets(renderer, filterBoxRect);

    auto dirtyFilterRegion = dirtyRect;
    auto filterRegion = dirtyRect;

    if (auto* shape = dynamicDowncast<RenderSVGShape>(renderer)) {
        // In LBSE, the filter region will be recomputed in createReferenceFilter().
        // FIXME: The LBSE filter geometry is not correct.
        filterRegion = dirtyFilterRegion = enclosingLayoutRect(shape->objectBoundingBox());
    } else {
        if (!outsets.isZero()) {
            // FIXME: This flipping was added for drop-shadow, but it's not obvious that it's correct.
            LayoutBoxExtent flippedOutsets { outsets.bottom(), outsets.left(), outsets.top(), outsets.right() };
            dirtyFilterRegion.expand(flippedOutsets);
        }

        dirtyFilterRegion = intersection(filterBoxRect, dirtyFilterRegion);
        filterRegion = dirtyFilterRegion;

        if (!outsets.isZero())
            filterRegion.expand(toLayoutBoxExtent(outsets));
    }

    if (filterRegion.isEmpty())
        return nullptr;

    auto geometryReferenceGeometryChanged = [](auto& existingGeometry, auto& newGeometry) {
        return existingGeometry.referenceBox != newGeometry.referenceBox || existingGeometry.scale != newGeometry.scale;
    };

    auto geometry = FilterGeometry {
        .referenceBox = filterBoxRect,
        .filterRegion = filterRegion,
        .scale = m_filterScale,
    };

    bool hasUpdatedBackingStore = false;
    if (!m_filter || geometryReferenceGeometryChanged(m_filter->geometry(), geometry) || m_preferredFilterRenderingModes != preferredFilterRenderingModes) {
        // FIXME: This rebuilds the entire effects chain even if the filter style didn't change.
        m_filter = CSSFilterRenderer::create(renderer, renderer.style().filter(), geometry, preferredFilterRenderingModes, renderer.settings().showDebugBorders(), context);
        hasUpdatedBackingStore = true;
    } else if (filterRegion != m_filter->filterRegion()) {
        m_filter->setFilterRegion(filterRegion);
        hasUpdatedBackingStore = true;
    }

    m_preferredFilterRenderingModes = preferredFilterRenderingModes;

    if (!m_filter)
        return nullptr;

    Ref filter = *m_filter;
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
            sourceImageRect = renderer.objectBoundingBox();
        else
            sourceImageRect = dirtyFilterRegion;
        m_targetSwitcher = GraphicsContextSwitcher::create(context, sourceImageRect, DestinationColorSpace::SRGB(), { WTF::move(filter) });
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
