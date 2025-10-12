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
#include <WebCore/Damage.h>
#include <WebCore/DisplayUpdate.h>
#include <WebCore/GLContext.h>
#include <WebCore/IntSize.h>
#include <WebCore/TextureMapperDamageVisualizer.h>
#include <atomic>
#include <optional>
#include <wtf/Atomics.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

#if !HAVE(DISPLAY_LINK)
#include "ThreadedDisplayRefreshMonitorPlayStation.h"
#endif

namespace WebCore {
class TextureMapper;
class TransformationMatrix;
}

namespace WebKit {
class AcceleratedSurface;
class CoordinatedSceneState;
class LayerTreeHost;

class ThreadedCompositor : public ThreadSafeRefCounted<ThreadedCompositor>, public CanMakeThreadSafeCheckedPtr<ThreadedCompositor> {
    WTF_MAKE_TZONE_ALLOCATED(ThreadedCompositor);
    WTF_MAKE_NONCOPYABLE(ThreadedCompositor);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ThreadedCompositor);
public:
#if HAVE(DISPLAY_LINK)
    static Ref<ThreadedCompositor> create(LayerTreeHost&);
#else
    static Ref<ThreadedCompositor> create(LayerTreeHost&, ThreadedDisplayRefreshMonitor::Client&, WebCore::PlatformDisplayID);
#endif
    virtual ~ThreadedCompositor();

    uint64_t surfaceID() const;

    void backgroundColorDidChange();

    void setSize(const WebCore::IntSize&, float);
    uint32_t requestComposition();
    void scheduleUpdate();
    RunLoop* runLoop();

    void invalidate();

#if !HAVE(DISPLAY_LINK)
    WebCore::DisplayRefreshMonitor& displayRefreshMonitor() const;
#endif

    void suspend();
    void resume();

    bool isActive() const;

    std::optional<float> fps() const { return m_fpsCounter.fps.load(); };

#if ENABLE(DAMAGE_TRACKING)
    enum class DamagePropagationFlags : uint8_t {
        Unified = 1 << 0,
        UseForCompositing = 1 << 1
    };
    void setDamagePropagationFlags(std::optional<OptionSet<DamagePropagationFlags>>);
    void enableFrameDamageNotificationForTesting();
#endif

private:
#if HAVE(DISPLAY_LINK)
    explicit ThreadedCompositor(LayerTreeHost&);
#else
    ThreadedCompositor(LayerTreeHost&, ThreadedDisplayRefreshMonitor::Client&, WebCore::PlatformDisplayID);
#endif

    void updateSceneState();
    void renderLayerTree();
    void paintToCurrentGLContext(const WebCore::TransformationMatrix&, const WebCore::IntSize&);
    void frameComplete();

#if HAVE(DISPLAY_LINK)
    void didRenderFrameTimerFired();
#else
    void displayUpdateFired();
    void sceneUpdateFinished();
#endif

    void updateSceneAttributes(const WebCore::IntSize&, float deviceScaleFactor);

    void initializeFPSCounter();
    void updateFPSCounter();

    CheckedPtr<LayerTreeHost> m_layerTreeHost;
    RefPtr<AcceleratedSurface> m_surface;
    RefPtr<CoordinatedSceneState> m_sceneState;
    std::unique_ptr<WebCore::GLContext> m_context;

    bool m_flipY { false };
    std::atomic<unsigned> m_suspendedCount { 0 };

    std::unique_ptr<CompositingRunLoop> m_compositingRunLoop;

    struct {
        Lock lock;
        WebCore::IntSize viewportSize;
        float deviceScaleFactor { 1 };

#if !HAVE(DISPLAY_LINK)
        bool clientRendersNextFrame { false };
#endif
    } m_attributes;

    std::unique_ptr<WebCore::TextureMapper> m_textureMapper;

    struct {
        bool exposesFPS { false };
        Seconds calculationInterval { 1_s };
        MonotonicTime lastCalculationTimestamp;
        unsigned frameCountSinceLastCalculation { 0 };
        std::atomic<std::optional<float>> fps;
    } m_fpsCounter;

#if ENABLE(DAMAGE_TRACKING)
    struct {
        std::optional<OptionSet<DamagePropagationFlags>> flags;
        std::unique_ptr<WebCore::TextureMapperDamageVisualizer> visualizer;
        std::atomic<bool> shouldNotifyFrameDamageForTesting { false };
    } m_damage;
#endif

    std::atomic<uint32_t> m_compositionRequestID { 0 };
#if HAVE(DISPLAY_LINK)
    std::atomic<uint32_t> m_compositionResponseID { 0 };
    RunLoop::Timer m_didRenderFrameTimer;
#else
    struct {
        WebCore::PlatformDisplayID displayID;
        WebCore::DisplayUpdate displayUpdate;
        std::unique_ptr<RunLoop::Timer> updateTimer;
    } m_display;

    const Ref<ThreadedDisplayRefreshMonitor> m_displayRefreshMonitor;
#endif
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

