/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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
#include "LegacyRenderSVGResourceFilter.h"
#include "ReferenceFilterOperation.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderLayerFilters);

bool RenderLayerFilters::isIdentity(RenderElement& renderer)
{
    const auto& operations = renderer.style().filter();
    return CSSFilterRenderer::isIdentity(renderer, operations);
}

IntOutsets RenderLayerFilters::calculateOutsets(RenderElement& renderer, const FloatRect& targetBoundingBox)
{
    const auto& operations = renderer.style().filter();
    return CSSFilterRenderer::calculateOutsets(renderer, operations, targetBoundingBox);
}

std::optional<std::tuple<Ref<Filter>, FloatRect>> RenderLayerFilters::createFilter(RenderElement& renderer, const FloatRect& targetBoundingBox, GraphicsContext& context)
{
    auto preferredFilterRenderingModes = renderer.page().preferredFilterRenderingModes();
    auto filterScale = FloatSize { renderer.page().deviceScaleFactor(), renderer.page().deviceScaleFactor() };

    const auto& operations = renderer.style().filter();

    auto filterRegion = targetBoundingBox;

    if (operations.hasFilterThatMovesPixels()) {
        // For CSSFilterRenderer, filterRegion = targetBoundingBox + filter->outsets()
        auto outsets = CSSFilterRenderer::calculateOutsets(renderer, operations, targetBoundingBox);
        filterRegion.expand(toLayoutBoxExtent(outsets));
    }

    auto filter = CSSFilterRenderer::create(renderer, operations, preferredFilterRenderingModes, filterScale, filterRegion, targetBoundingBox, context, RenderingResourceIdentifier::generate());
    if (!filter)
        return std::nullopt;

    return { { filter.releaseNonNull(), targetBoundingBox } };
}

SwitcherState RenderLayerFilters::beginDrawSourceImage(RenderElement& renderer, const FloatRect& targetBoundingBox, const FloatRect& clipRect, GraphicsContext*& context)
{
    LOG_WITH_STREAM(Filters, stream << "\nRenderLayerFilters " << this << " beginDrawSourceImage\n");
    return RenderFilterResource::beginDrawSourceImage(renderer, targetBoundingBox, clipRect, context);
}

SwitcherState RenderLayerFilters::endDrawSourceImage(RenderElement& renderer, GraphicsContext*& context)
{
    LOG_WITH_STREAM(Filters, stream << "RenderLayerFilters " << this << " endDrawSourceImage\n");
    return RenderFilterResource::endDrawSourceImage(renderer, context);
}

void RenderLayerFilters::addReferenceFilterClient(RenderLayer& layer)
{
    auto& filter = layer.renderer().style().filter();

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
            RefPtr filterElement = layer.renderer().document().getElementById(referenceOperation->fragment());
            if (!filterElement)
                continue;

            CheckedPtr renderer = dynamicDowncast<LegacyRenderSVGResourceFilter>(filterElement->renderer());
            if (!renderer)
                continue;
            renderer->addClientRenderLayer(layer);
            m_internalSVGReferences.append(WTFMove(filterElement));
        }
    }
}

void RenderLayerFilters::removeReferenceFilterClient(RenderLayer& layer)
{
    for (auto& resourceHandle : m_externalSVGReferences)
        resourceHandle->removeClient(*this);

    m_externalSVGReferences.clear();

    for (auto& filterElement : m_internalSVGReferences) {
        if (auto* renderer = filterElement->renderer())
            downcast<LegacyRenderSVGResourceContainer>(*renderer).removeClientRenderLayer(layer);
    }

    m_internalSVGReferences.clear();
}

} // namespace WebCore
