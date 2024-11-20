/*
 * Copyright (C) 2014 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(COORDINATED_GRAPHICS)

#include "CompositingRunLoop.h"
#include "CoordinatedGraphicsScene.h"
#include <WebCore/DisplayUpdate.h>
#include <WebCore/GLContext.h>
#include <WebCore/IntSize.h>
#include <wtf/Atomics.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

#if !HAVE(DISPLAY_LINK)
#include "ThreadedDisplayRefreshMonitor.h"
#endif

namespace WebCore {
class Damage;
}

namespace WebKit {

class AcceleratedSurface;
class LayerTreeHost;

class ThreadedCompositor : public CoordinatedGraphicsSceneClient, public ThreadSafeRefCounted<ThreadedCompositor> {
    WTF_MAKE_TZONE_ALLOCATED(ThreadedCompositor);
    WTF_MAKE_NONCOPYABLE(ThreadedCompositor);
public:
    enum class DamagePropagation : uint8_t {
        None,
        Region,
        Unified,
    };

#if HAVE(DISPLAY_LINK)
    static Ref<ThreadedCompositor> create(LayerTreeHost&, float scaleFactor);
#else
    static Ref<ThreadedCompositor> create(LayerTreeHost&, ThreadedDisplayRefreshMonitor::Client&, float scaleFactor, WebCore::PlatformDisplayID);
#endif
    virtual ~ThreadedCompositor();

    uint64_t surfaceID() const;

    void setViewportSize(const WebCore::IntSize&, float scale);
    void backgroundColorDidChange();
#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
    void preferredBufferFormatsDidChange();
#endif


    uint32_t requestComposition(const RefPtr<Nicosia::Scene>&);
    void updateScene();
    void updateSceneWithoutRendering();

    void invalidate();

    void forceRepaint();

#if !HAVE(DISPLAY_LINK)
    WebCore::DisplayRefreshMonitor& displayRefreshMonitor() const;
#endif

    void suspend();
    void resume();

private:
#if HAVE(DISPLAY_LINK)
    ThreadedCompositor(LayerTreeHost&, float scaleFactor);
#else
    ThreadedCompositor(LayerTreeHost&, ThreadedDisplayRefreshMonitor::Client&, float scaleFactor, WebCore::PlatformDisplayID);
#endif

    // CoordinatedGraphicsSceneClient
    void updateViewport() override;
    const WebCore::Damage& addSurfaceDamage(const WebCore::Damage&) override;

    void renderLayerTree();
    void frameComplete();

#if HAVE(DISPLAY_LINK)
    void didRenderFrameTimerFired();
#else
    void displayUpdateFired();
    void sceneUpdateFinished();
#endif

    CheckedPtr<LayerTreeHost> m_layerTreeHost;
    std::unique_ptr<AcceleratedSurface> m_surface;
    RefPtr<CoordinatedGraphicsScene> m_scene;
    std::unique_ptr<WebCore::GLContext> m_context;

    bool m_flipY { false };
    DamagePropagation m_damagePropagation { DamagePropagation::None };
    unsigned m_suspendedCount { 0 };

    std::unique_ptr<CompositingRunLoop> m_compositingRunLoop;

    struct {
        Lock lock;
        WebCore::IntSize viewportSize;
        float scaleFactor { 1 };
        bool needsResize { false };
        Vector<RefPtr<Nicosia::Scene>> states;

        bool clientRendersNextFrame { false };
        uint32_t compositionRequestID { 0 };
    } m_attributes;

#if HAVE(DISPLAY_LINK)
    std::atomic<uint32_t> m_compositionResponseID { 0 };
    RunLoop::Timer m_didRenderFrameTimer;
#else
    struct {
        WebCore::PlatformDisplayID displayID;
        WebCore::DisplayUpdate displayUpdate;
        std::unique_ptr<RunLoop::Timer> updateTimer;
    } m_display;

    Ref<ThreadedDisplayRefreshMonitor> m_displayRefreshMonitor;
#endif
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

