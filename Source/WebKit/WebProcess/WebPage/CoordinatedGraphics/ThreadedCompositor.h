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
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrContextThreadSafeProxy.h>
#include <skia/gpu/ganesh/GrDirectContext.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/Atomics.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>

class SkCanvas;

namespace WebCore {
class TextureMapper;
class TransformationMatrix;
}

namespace WTF {
enum class Critical : bool;
}

namespace WebKit {
class AcceleratedSurface;
class CoordinatedSceneState;
class LayerTreeHost;
class WebPage;
struct RenderProcessInfo;

class ThreadedCompositor : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ThreadedCompositor>, public CanMakeThreadSafeCheckedPtr<ThreadedCompositor> {
    WTF_MAKE_TZONE_ALLOCATED(ThreadedCompositor);
    WTF_MAKE_NONCOPYABLE(ThreadedCompositor);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ThreadedCompositor);
public:
    static Ref<ThreadedCompositor> create(WebPage&, LayerTreeHost&, CoordinatedSceneState&);
    virtual ~ThreadedCompositor();

    uint64_t surfaceID() const;
    int maxTextureSize() const { return m_maxTextureSize; }

    void backgroundColorDidChange();
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM) && (USE(GBM) || OS(ANDROID))
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
    void setDamagePropagationSettings(std::optional<OptionSet<DamagePropagationFlags>>, unsigned rectangleThreshold);
    void enableFrameDamageNotificationForTesting();
#endif

    void fillGLInformation(RenderProcessInfo&&, CompletionHandler<void(RenderProcessInfo&&)>&&);

    void releaseMemory(WTF::Critical);

    sk_sp<GrContextThreadSafeProxy> threadSafeGrContext() const { return m_threadSafeGrContext; }

private:
    ThreadedCompositor(WebPage&, LayerTreeHost&, CoordinatedSceneState&);

    void startRenderTimer();
    void stopRenderTimer();
    bool isOnlyRenderingUpdatePendingAndWaitingForTiles() const;

    void scheduleUpdateLocked();
    void flushCompositingState(const OptionSet<WebCore::CompositionReason>&);
    void renderLayerTree();
    void paintToCurrentGLContext(const WebCore::TransformationMatrix&, const WebCore::IntSize&, const OptionSet<WebCore::CompositionReason>&);
    void paintToTextureMapper(const WebCore::TransformationMatrix&, const WebCore::IntSize&, const OptionSet<WebCore::CompositionReason>&);
    void paintToSkiaCanvas(const WebCore::TransformationMatrix&, const WebCore::IntSize&, const OptionSet<WebCore::CompositionReason>&);
    void frameComplete();

    void didCompositeRunLoopObserverFired();

    void updateSceneAttributes(const WebCore::IntSize&, float deviceScaleFactor);

    void initializeFPSCounter();
    void updateFPSCounter();
    void drawFPSCounter(SkCanvas&);
#if ENABLE(DAMAGE_TRACKING)
    void drawSkiaDamage(SkCanvas&, const std::optional<WebCore::Damage>&);
#endif

    const Ref<WorkQueue> m_workQueue;
    CheckedPtr<LayerTreeHost> m_layerTreeHost;
    bool m_useSkia { false };
    RefPtr<AcceleratedSurface> m_surface;
    RefPtr<CoordinatedSceneState> m_sceneState;
    std::unique_ptr<WebCore::GLContext> m_context;
    sk_sp<GrContextThreadSafeProxy> m_threadSafeGrContext;

    bool m_flipY { false };
    int m_maxTextureSize { 0 };
    std::atomic<unsigned> m_suspendedCount { 0 };

    enum class State {
        Idle,
        Scheduled,
        InProgress,
        ScheduledWhileInProgress,
        Invalidated
    };
    static ASCIILiteral stateToString(State);

    struct {
        mutable Lock lock;
        State state WTF_GUARDED_BY_LOCK(lock) { State::Idle };
        bool isRenderTimerActive WTF_GUARDED_BY_LOCK(lock) { false };
        bool isWaitingForTiles WTF_GUARDED_BY_LOCK(lock) { false };
        OptionSet<WebCore::CompositionReason> reasons WTF_GUARDED_BY_LOCK(lock);
        Function<void()> didCompositeRenderingUpdateFunction WTF_GUARDED_BY_LOCK(lock);
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
        bool drawsFPS { false };
        Seconds calculationInterval { 1_s };
        MonotonicTime lastCalculationTimestamp;
        unsigned frameCountSinceLastCalculation { 0 };
        int lastFPS { 0 };
        std::atomic<std::optional<float>> fps;

        // On-screen overlay state, only used when drawsFPS is set.
        int displayedFPS { -1 };
        CString fpsString;
        float backgroundWidth { 0 };
        float backgroundHeight { 0 };
        float textBaseline { 0 };
    } m_fpsCounter;

#if ENABLE(DAMAGE_TRACKING)
    struct {
        std::optional<OptionSet<DamagePropagationFlags>> flags;
        unsigned rectangleThreshold { 4 };
        std::unique_ptr<WebCore::TextureMapperDamageVisualizer> visualizer;

        bool showSkiaDamage { false };
        unsigned skiaDamageMargin { 0 };

        std::atomic<bool> shouldNotifyFrameDamageForTesting { false };
    } m_damage;
#endif

    std::unique_ptr<WebCore::RunLoopObserver> m_didCompositeRunLoopObserver;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

