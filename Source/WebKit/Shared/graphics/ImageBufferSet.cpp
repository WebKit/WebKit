/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "ImageBufferSet.h"

#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBufferBackend.h>

#if PLATFORM(COCOA)
#include <WebCore/PlatformCALayer.h>
#endif

#if ENABLE(GPU_PROCESS)

namespace WebKit {

using namespace WebCore;

SwapBuffersDisplayRequirement ImageBufferSet::swapBuffersForDisplay(bool hasEmptyDirtyRegion, bool supportsPartialRepaint)
{
    auto displayRequirement = SwapBuffersDisplayRequirement::NeedsNoDisplay;

    // Make the previous front buffer non-volatile early, so that we can dirty the whole layer if it comes back empty.
    if (!m_frontBuffer || m_frontBuffer->setNonVolatile() == WebCore::SetNonVolatileResult::Empty)
        displayRequirement = SwapBuffersDisplayRequirement::NeedsFullDisplay;
    else if (!hasEmptyDirtyRegion)
        displayRequirement = SwapBuffersDisplayRequirement::NeedsNormalDisplay;

    if (displayRequirement == SwapBuffersDisplayRequirement::NeedsNoDisplay)
        return displayRequirement;

    if (!supportsPartialRepaint)
        displayRequirement = SwapBuffersDisplayRequirement::NeedsFullDisplay;

    if (!m_backBuffer || m_backBuffer->isInUse()) {
        m_previouslyPaintedRect = std::nullopt;
        std::swap(m_backBuffer, m_secondaryBackBuffer);

        // When pulling the secondary back buffer out of hibernation (to become
        // the new front buffer), if it is somehow still in use (e.g. we got
        // three swaps ahead of the render server), just give up and discard it.
        if (m_backBuffer && m_backBuffer->isInUse())
            m_backBuffer = nullptr;
    }

    std::swap(m_frontBuffer, m_backBuffer);

    if (m_frontBuffer && m_frontBuffer->setNonVolatile() == WebCore::SetNonVolatileResult::Empty)
        m_previouslyPaintedRect = std::nullopt;

    return displayRequirement;
}

ImageBufferSet::PaintRectList ImageBufferSet::computePaintingRects(const Region& dirtyRegion, float resolutionScale)
{
    // If we have less than webLayerMaxRectsToPaint rects to paint and they cover less
    // than webLayerWastedSpaceThreshold of the total dirty area, we'll repaint each rect separately.
    // Otherwise, repaint the entire bounding box of the dirty region.
    auto dirtyRects = dirtyRegion.rects();
#if PLATFORM(COCOA)
    IntRect dirtyBounds = dirtyRegion.bounds();
    if (dirtyRects.size() > PlatformCALayer::webLayerMaxRectsToPaint || dirtyRegion.totalArea() > PlatformCALayer::webLayerWastedSpaceThreshold * dirtyBounds.width() * dirtyBounds.height()) {
        dirtyRects.clear();
        dirtyRects.append(dirtyBounds);
    }
#endif

    // FIXME: find a consistent way to scale and snap dirty and CG clip rects.
    Vector<FloatRect, 5> paintingRects;
    for (const auto& rect : dirtyRects) {
        FloatRect scaledRect(rect);
        scaledRect.scale(resolutionScale);
        scaledRect = enclosingIntRect(scaledRect);
        scaledRect.scale(1 / resolutionScale);
        paintingRects.append(scaledRect);
    }
    return paintingRects;
}

void ImageBufferSet::prepareBufferForDisplay(const FloatRect& layerBounds, const Region& dirtyRegion, const PaintRectList& paintingRects, bool requiresClearedPixels)
{
    if (!m_frontBuffer) {
        m_previouslyPaintedRect = std::nullopt;
        return;
    }

    GraphicsContext& context = m_frontBuffer->context();
    context.resetClip();

    WebCore::FloatRect copyRect;
    if (m_backBuffer) {
        WebCore::IntRect enclosingCopyRect { m_previouslyPaintedRect ? *m_previouslyPaintedRect : enclosingIntRect(layerBounds) };
        if (!dirtyRegion.contains(enclosingCopyRect)) {
            WebCore::Region copyRegion(enclosingCopyRect);
            copyRegion.subtract(dirtyRegion);
            copyRect = intersection(copyRegion.bounds(), layerBounds);
            if (!copyRect.isEmpty())
                m_frontBuffer->context().drawImageBuffer(*m_backBuffer, copyRect, copyRect, { WebCore::CompositeOperator::Copy });
        }
    }

    if (paintingRects.size() == 1) {
        context.clip(paintingRects[0]);
        if (copyRect.intersects(paintingRects[0]))
            m_frontBufferIsCleared = false;
    } else {
        Path clipPath;
        for (auto rect : paintingRects) {
            clipPath.addRect(rect);

            // If the copy-forward touched pixels that are about to be painted, then they
            // won't be 'clear' any more.
            if (copyRect.intersects(rect))
                m_frontBufferIsCleared = false;
        }
        context.clipPath(clipPath);
    }

    if (requiresClearedPixels && !m_frontBufferIsCleared)
        context.clearRect(layerBounds);

    m_previouslyPaintedRect = dirtyRegion.bounds();
    m_frontBufferIsCleared = false;
}

void ImageBufferSet::clearBuffers()
{
    m_frontBuffer = m_backBuffer = m_secondaryBackBuffer = nullptr;
}

}

#endif
