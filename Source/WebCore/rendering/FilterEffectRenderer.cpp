/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FilterEffectRenderer.h"

#include "CSSFilter.h"
#include "CachedSVGDocument.h"
#include "CachedSVGDocumentReference.h"
#include "ElementIterator.h"
#include "FEColorMatrix.h"
#include "FEComponentTransfer.h"
#include "FEDropShadow.h"
#include "FEGaussianBlur.h"
#include "FEMerge.h"
#include "Logging.h"
#include "RenderLayer.h"
#include "SVGElement.h"
#include "SVGFilterBuilder.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SourceGraphic.h"
#include <algorithm>
#include <wtf/MathExtras.h>

#if USE(DIRECT2D)
#include <d2d1.h>
#endif

namespace WebCore {

bool FilterEffectRendererHelper::prepareFilterEffect(RenderLayer& layer, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect, const LayoutRect& layerRepaintRect)
{
    ASSERT(m_haveFilterEffect);
    ASSERT(layer.filter());

    auto& filter = *layer.filter();
    auto filterSourceRect = filter.computeSourceImageRectForDirtyRect(filterBoxRect, dirtyRect);

    if (filterSourceRect.isEmpty()) {
        // The dirty rect is not in view, just bail out.
        m_haveFilterEffect = false;
        return false;
    }

    bool hasUpdatedBackingStore = filter.updateBackingStoreRect(filterSourceRect);

    m_renderLayer = &layer;
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

    return true;
}

GraphicsContext* FilterEffectRendererHelper::filterContext() const
{
    if (!m_haveFilterEffect)
        return nullptr;
    return m_renderLayer->filter()->inputContext();
}

bool FilterEffectRendererHelper::beginFilterEffect()
{
    ASSERT(m_renderLayer);
    ASSERT(m_renderLayer->filter());

    auto& filter = *m_renderLayer->filter();
    filter.allocateBackingStoreIfNeeded(m_targetContext);
    // Paint into the context that represents the SourceGraphic of the filter.
    auto* sourceGraphicsContext = filter.inputContext();
    if (!sourceGraphicsContext || filter.filterRegion().isEmpty() || ImageBuffer::sizeNeedsClamping(filter.filterRegion().size())) {
        // Disable the filters and continue.
        m_haveFilterEffect = false;
        return false;
    }

    // Translate the context so that the contents of the layer is captured in the offscreen memory buffer.
    sourceGraphicsContext->save();
    sourceGraphicsContext->translate(-m_paintOffset);
    sourceGraphicsContext->clearRect(m_repaintRect);
    sourceGraphicsContext->clip(m_repaintRect);

    m_startedFilterEffect = true;
    return true;
}

void FilterEffectRendererHelper::applyFilterEffect(GraphicsContext& destinationContext)
{
    ASSERT(m_haveFilterEffect);
    ASSERT(m_renderLayer);
    ASSERT(m_renderLayer->filter());
    ASSERT(m_renderLayer->filter()->inputContext());

    LOG_WITH_STREAM(Filters, stream << "\nFilterEffectRendererHelper " << this << " applyFilterEffect");

    auto& filter = *m_renderLayer->filter();
    filter.inputContext()->restore();

    filter.apply();

    // Get the filtered output and draw it in place.
    LayoutRect destRect = filter.outputRect();
    destRect.move(m_paintOffset.x(), m_paintOffset.y());

    if (auto* outputBuffer = filter.output())
        destinationContext.drawImageBuffer(*outputBuffer, snapRectToDevicePixels(destRect, m_renderLayer->renderer().document().deviceScaleFactor()));

    filter.clearIntermediateResults();

    LOG_WITH_STREAM(Filters, stream << "FilterEffectRendererHelper " << this << " applyFilterEffect done\n");
}

} // namespace WebCore
