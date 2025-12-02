/*
 * Copyright (C) 2014, 2025 Igalia S.L.
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
#include <WebCore/CoordinatedCompositionReason.h>
#include <WebCore/Damage.h>
#include <WebCore/DisplayUpdate.h>
#include <WebCore/GLContext.h>
#include <WebCore/IntSize.h>
#include <WebCore/RunLoopObserver.h>
#include <WebCore/TextureMapperDamageVisualizer.h>
#include <atomic>
#include <optional>
#include <wtf/Atomics.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

namespace WebCore {
class TextureMapper;
class TransformationMatrix;
}

namespace WebKit {
class AcceleratedSurface;
class CoordinatedSceneState;
class LayerTreeHost;
struct RenderProcessInfo;

class ThreadedCompositor : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ThreadedCompositor>, public CanMakeThreadSafeCheckedPtr<ThreadedCompositor> {
    WTF_MAKE_TZONE_ALLOCATED(ThreadedCompositor);
    WTF_MAKE_NONCOPYABLE(ThreadedCompositor);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ThreadedCompositor);
public:
    static Ref<ThreadedCompositor> create(LayerTreeHost&);
    virtual ~ThreadedCompositor();

    uint64_t surfaceID() const;
    int maxTextureSize() const { return m_maxTextureSize; }

    void backgroundColorDidChange();
#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
    void preferredBufferFormatsDidChange();
#endif
    void pendingTilesDidChange();

    void setSize(const WebCore::IntSize&, float);
    void requestCompositionForRenderingUpdate(Function<void()>&&);
    void requestComposition(WebCore::CompositionReason);
    RunLoop* runLoop();

    void invalidate();
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

    void fillGLInformation(RenderProcessInfo&&, CompletionHandler<void(RenderProcessInfo&&)>&&);

private:
    explicit ThreadedCompositor(LayerTreeHost&);

    void scheduleUpdateLocked();
    void flushCompositingState(const OptionSet<WebCore::CompositionReason>&);
    void renderLayerTree();
    void paintToCurrentGLContext(const WebCore::TransformationMatrix&, const WebCore::IntSize&);
    void frameComplete();

    void didCompositeRunLoopObserverFired();

    void updateSceneAttributes(const WebCore::IntSize&, float deviceScaleFactor);

    void initializeFPSCounter();
    void updateFPSCounter();

    const Ref<WorkQueue> m_workQueue;
    CheckedPtr<LayerTreeHost> m_layerTreeHost;
    RefPtr<AcceleratedSurface> m_surface;
    RefPtr<CoordinatedSceneState> m_sceneState;
    std::unique_ptr<WebCore::GLContext> m_context;

    bool m_flipY { false };
    int m_maxTextureSize { 0 };
    std::atomic<unsigned> m_suspendedCount { 0 };

    enum class State {
        Idle,
        Scheduled,
        InProgress,
        ScheduledWhileInProgress,
        WaitingForTiles
    };

    struct {
        mutable Lock lock;
        State state WTF_GUARDED_BY_LOCK(lock) { State::Idle };
        OptionSet<WebCore::CompositionReason> reasons WTF_GUARDED_BY_LOCK(lock);
        Function<void()> didCompositeRenderinUpdateFunction WTF_GUARDED_BY_LOCK(lock);
    } m_state;

    struct {
        Lock lock;
        WebCore::IntSize viewportSize;
        float deviceScaleFactor { 1 };
    } m_attributes;

    RunLoop::Timer m_renderTimer;
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

    std::unique_ptr<WebCore::RunLoopObserver> m_didCompositeRunLoopObserver;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

