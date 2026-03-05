/*
 * Copyright (C) 2026 Igalia S.L.
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

#pragma once

#if USE(COORDINATED_GRAPHICS)
#include <wtf/CompletionHandler.h>

namespace WebCore {
class FloatPoint;
class GraphicsLayer;
class GraphicsLayerFactory;
class IntRect;
class Region;
class RunLoopObserver;
}

namespace WebKit {
struct RenderProcessInfo;

class FrameRenderer {
public:
    virtual ~FrameRenderer();

    virtual uint64_t surfaceID() const = 0;
    virtual void setNeedsDisplay() { }
    virtual void setNeedsDisplayInRect(const WebCore::IntRect&) { }
    virtual void updateRenderingWithForcedRepaint() = 0;
    virtual void scheduleRenderingUpdate() = 0;
    virtual void suspend();
    virtual void resume();
    virtual void sizeDidChange() = 0;
    virtual void backgroundColorDidChange() { }
    virtual bool ensureDrawing() = 0;
    virtual void fillGLInformation(RenderProcessInfo&&, CompletionHandler<void(RenderProcessInfo&&)>&&) = 0;

    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() { RELEASE_ASSERT_NOT_REACHED(); }
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) { RELEASE_ASSERT_NOT_REACHED(); }
    virtual void setViewOverlayRootLayer(WebCore::GraphicsLayer*) { RELEASE_ASSERT_NOT_REACHED(); }

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM) && (USE(GBM) || OS(ANDROID))
    virtual void preferredBufferFormatsDidChange() { }
#endif

#if ENABLE(DAMAGE_TRACKING)
    virtual void resetDamageHistoryForTesting() = 0;
    virtual void foreachRegionInDamageHistoryForTesting(Function<void(const WebCore::Region&)>&&) const = 0;
#endif

#if PLATFORM(GTK)
    virtual void adjustTransientZoom(double, WebCore::FloatPoint, WebCore::FloatPoint) = 0;
    virtual void commitTransientZoom(double, WebCore::FloatPoint, WebCore::FloatPoint) = 0;
#endif

    void setLayerTreeStateIsFrozen(bool);
    void updateRenderingWithForcedRepaintAsync(CompletionHandler<void()>&&);

protected:
    FrameRenderer();

    virtual bool canUpdateRendering() const = 0;
    virtual void updateRendering() = 0;

    void scheduleRenderingUpdateRunLoopObserver();
    void invalidateRenderingUpdateRunLoopObserver();
    void renderingUpdateRunLoopObserverFired();

    std::unique_ptr<WebCore::RunLoopObserver> m_renderingUpdateRunLoopObserver;
    bool m_layerTreeStateIsFrozen { false };
    bool m_isSuspended { false };
    bool m_isUpdatingRendering { false };
    CompletionHandler<void()> m_forcedRepaintAsyncCallback;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
