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

#include "config.h"
#include "ThreadedCompositor.h"

#if USE(COORDINATED_GRAPHICS)
#include "AcceleratedSurface.h"
#include "CoordinatedSceneState.h"
#include "LayerTreeHost.h"
#include "RenderProcessInfo.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/CoordinatedPlatformLayer.h>
#include <WebCore/Damage.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/TextureMapperLayer.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#else
#include <GLES2/gl2.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(ThreadedCompositor);

Ref<ThreadedCompositor> ThreadedCompositor::create(LayerTreeHost& layerTreeHost)
{
    return adoptRef(*new ThreadedCompositor(layerTreeHost));
}

ThreadedCompositor::ThreadedCompositor(LayerTreeHost& layerTreeHost)
    : m_workQueue(WorkQueue::create("org.webkit.ThreadedCompositor"_s))
    , m_layerTreeHost(&layerTreeHost)
    , m_surface(AcceleratedSurface::create(layerTreeHost.webPage(), [this] { frameComplete(); }))
    , m_sceneState(&m_layerTreeHost->sceneState())
    , m_flipY(m_surface->shouldPaintMirrored())
    , m_renderTimer(m_workQueue->runLoop(), "ThreadedCompositor::RenderTimer"_s, this, &ThreadedCompositor::renderLayerTree)
{
    ASSERT(RunLoop::isMain());

    m_didCompositeRunLoopObserver = makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::GraphicsCommit, [this] {
        this->didCompositeRunLoopObserverFired();
    });

    initializeFPSCounter();
#if ENABLE(DAMAGE_TRACKING)
    m_damage.visualizer = TextureMapperDamageVisualizer::create();
#endif

    const auto& webPage = m_layerTreeHost->webPage();
    updateSceneAttributes(webPage.size(), webPage.deviceScaleFactor());

    m_surface->didCreateCompositingRunLoop(m_workQueue->runLoop());

    m_workQueue->dispatchSync([this] {
        // GLNativeWindowType depends on the EGL implementation: reinterpret_cast works
        // for pointers (only if they are 64-bit wide and not for other cases), and static_cast for
        // numeric types (and when needed they get extended to 64-bit) but not for pointers. Using
        // a plain C cast expression in this one instance works in all cases.
        static_assert(sizeof(GLNativeWindowType) <= sizeof(uint64_t), "GLNativeWindowType must not be longer than 64 bits.");
        auto nativeSurfaceHandle = (GLNativeWindowType)m_surface->window();
        m_context = GLContext::create(PlatformDisplay::sharedDisplay(), nativeSurfaceHandle);
        if (m_context && m_context->makeContextCurrent()) {
            if (!nativeSurfaceHandle)
                m_flipY = !m_flipY;
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);

            m_textureMapper = TextureMapper::create();
        }
    });
}

ThreadedCompositor::~ThreadedCompositor() = default;

uint64_t ThreadedCompositor::surfaceID() const
{
    ASSERT(RunLoop::isMain());
    return m_surface->surfaceID();
}

void ThreadedCompositor::invalidate()
{
    ASSERT(RunLoop::isMain());

    {
        Locker locker { m_state.lock };
        stopRenderTimer();
        m_state.didCompositeRenderingUpdateFunction = nullptr;
        m_state.state = State::Idle;
    }

    m_didCompositeRunLoopObserver->invalidate();
    m_workQueue->dispatchSync([this] {
        if (!m_context || !m_context->makeContextCurrent())
            return;

        // Update the scene at this point ensures the layers state are correctly propagated.
        flushCompositingState(CompositionReason::RenderingUpdate);

        m_sceneState->invalidateCommittedLayers();
        m_textureMapper = nullptr;
        m_surface->willDestroyGLContext();
        m_context = nullptr;
    });
    m_sceneState = nullptr;
    m_layerTreeHost = nullptr;
    m_surface->willDestroyCompositingRunLoop();
    m_surface = nullptr;
}

void ThreadedCompositor::startRenderTimer()
{
    ASSERT(m_state.lock.isHeld());
    ASSERT(!m_state.isRenderTimerActive);
    m_state.isRenderTimerActive = true;
    m_renderTimer.startOneShot(0_s);
}

void ThreadedCompositor::stopRenderTimer()
{
    ASSERT(m_state.lock.isHeld());
    m_state.isRenderTimerActive = false;
    m_renderTimer.stop();
}

bool ThreadedCompositor::isOnlyRenderingUpdatePendingAndWaitingForTiles() const
{
    ASSERT(m_state.lock.isHeld());
    return m_state.reasons.containsOnly({ CompositionReason::RenderingUpdate }) && m_state.isWaitingForTiles;
}

void ThreadedCompositor::suspend()
{
    ASSERT(RunLoop::isMain());
    m_surface->visibilityDidChange(false);

    if (++m_suspendedCount > 1)
        return;

    Locker locker { m_state.lock };
    stopRenderTimer();
}

void ThreadedCompositor::resume()
{
    ASSERT(RunLoop::isMain());
    m_surface->visibilityDidChange(true);

    ASSERT(m_suspendedCount > 0);
    if (--m_suspendedCount > 0)
        return;

    Locker locker { m_state.lock };
    if (m_state.state == State::Scheduled && !isOnlyRenderingUpdatePendingAndWaitingForTiles())
        startRenderTimer();
}

bool ThreadedCompositor::isActive() const
{
    Locker locker { m_state.lock };
    return m_state.state != State::Idle;
}

void ThreadedCompositor::backgroundColorDidChange()
{
    ASSERT(RunLoop::isMain());
    m_surface->backgroundColorDidChange();
}

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM) && (USE(GBM) || OS(ANDROID))
void ThreadedCompositor::preferredBufferFormatsDidChange()
{
    ASSERT(RunLoop::isMain());
    m_surface->preferredBufferFormatsDidChange();
}
#endif

void ThreadedCompositor::pendingTilesDidChange()
{
    Locker locker { m_state.lock };
    if (!m_state.isWaitingForTiles)
        return;

    if (m_sceneState->pendingTiles())
        return;

    m_state.isWaitingForTiles = false;
    scheduleUpdateLocked();
}

void ThreadedCompositor::setSize(const IntSize& size, float deviceScaleFactor)
{
    ASSERT(RunLoop::isMain());
    Locker locker { m_attributes.lock };
    updateSceneAttributes(size, deviceScaleFactor);
}

#if ENABLE(DAMAGE_TRACKING)
void ThreadedCompositor::setDamagePropagationFlags(std::optional<OptionSet<DamagePropagationFlags>> flags)
{
    m_damage.flags = flags;
    if (m_damage.visualizer && m_damage.flags) {
        // We don't use damage when rendering layers if the visualizer is enabled, because we need to make sure the whole
        // frame is invalidated in the next paint so that previous damage rects are cleared.
        m_damage.flags->remove(DamagePropagationFlags::UseForCompositing);
    }
}

void ThreadedCompositor::enableFrameDamageNotificationForTesting()
{
    m_damage.shouldNotifyFrameDamageForTesting = true;
}
#endif

void ThreadedCompositor::flushCompositingState(const OptionSet<CompositionReason>& reasons)
{
    if (reasons.hasExactlyOneBitSet() && reasons.contains(CompositionReason::Animation))
        return;

    ASSERT(!reasons.contains(CompositionReason::RenderingUpdate) || !m_sceneState->pendingTiles());
    m_sceneState->rootLayer().flushCompositingState(reasons, *m_textureMapper);
    for (auto& layer : m_sceneState->committedLayers())
        layer->flushCompositingState(reasons, *m_textureMapper);
}

void ThreadedCompositor::paintToCurrentGLContext(const TransformationMatrix& matrix, const IntSize& size, const OptionSet<CompositionReason>& reasons)
{
    FloatRect clipRect(FloatPoint { }, size);
    TextureMapperLayer& currentRootLayer = m_sceneState->rootLayer().ensureTarget();
    if (currentRootLayer.transform() != matrix)
        currentRootLayer.setTransform(matrix);

    bool sceneHasRunningAnimations = currentRootLayer.applyAnimationsRecursively(MonotonicTime::now());

    m_textureMapper->beginPainting(m_flipY ? TextureMapper::FlipY::Yes : TextureMapper::FlipY::No);
    m_textureMapper->beginClip(TransformationMatrix(), FloatRoundedRect(clipRect));

#if ENABLE(DAMAGE_TRACKING)
    std::optional<FloatRoundedRect> rectContainingRegionThatActuallyChanged;
    currentRootLayer.prepareForPainting(*m_textureMapper);
    if (m_damage.flags) {
        Damage frameDamage(size, m_damage.flags->contains(DamagePropagationFlags::Unified) ? Damage::Mode::BoundingBox : Damage::Mode::Rectangles);

        WTFBeginSignpost(this, CollectDamage);
        currentRootLayer.collectDamage(*m_textureMapper, frameDamage);
        WTFEndSignpost(this, CollectDamage);

        if (m_damage.shouldNotifyFrameDamageForTesting && m_layerTreeHost)
            m_layerTreeHost->notifyFrameDamageForTesting(frameDamage.regionForTesting());

        m_surface->setFrameDamage(WTF::move(frameDamage));

        if (m_damage.flags->contains(DamagePropagationFlags::UseForCompositing)) {
            const auto& damageSinceLastSurfaceUse = m_surface->renderTargetDamage();
            if (damageSinceLastSurfaceUse && !FloatRect(damageSinceLastSurfaceUse->bounds()).contains(clipRect))
                rectContainingRegionThatActuallyChanged = FloatRoundedRect(damageSinceLastSurfaceUse->bounds());

            m_textureMapper->setDamage(damageSinceLastSurfaceUse);
        }
    }

    if (rectContainingRegionThatActuallyChanged)
        m_textureMapper->beginClip(TransformationMatrix(), *rectContainingRegionThatActuallyChanged);
#endif

    m_surface->clear(reasons);

    WTFBeginSignpost(this, PaintTextureMapperLayerTree);
    currentRootLayer.paint(*m_textureMapper);
    WTFEndSignpost(this, PaintTextureMapperLayerTree);

#if ENABLE(DAMAGE_TRACKING)
    if (rectContainingRegionThatActuallyChanged)
        m_textureMapper->endClip();
#endif

#if ENABLE(DAMAGE_TRACKING)
    if (m_damage.visualizer) {
        m_damage.visualizer->paintDamage(*m_textureMapper, m_surface->frameDamage());
        // When damage visualizer is active, we cannot send the original damage to the platform as in this case
        // the damage rects visualized previous frame may not get erased if platform actually uses damage.
        m_surface->setFrameDamage(Damage(size, Damage::Mode::Full));
    }
#endif

    m_textureMapper->endClip();
    m_textureMapper->endPainting();

    if (sceneHasRunningAnimations)
        requestComposition(CompositionReason::Animation);
}

#if HAVE(OS_SIGNPOST) || USE(SYSPROF_CAPTURE)
static String reasonsToString(const OptionSet<CompositionReason>& reasons)
{
    StringBuilder builder;
    for (auto reason : reasons) {
        if (!builder.isEmpty())
            builder.append(", "_s);
        builder.append(enumName(reason));
    }
    return builder.toString();
}
#endif

void ThreadedCompositor::renderLayerTree()
{
    ASSERT(m_sceneState);
    ASSERT(m_workQueue->runLoop().isCurrent());
#if PLATFORM(GTK) || PLATFORM(WPE)
    TraceScope traceScope(RenderLayerTreeStart, RenderLayerTreeEnd);
#endif

    if (m_suspendedCount > 0)
        return;

    OptionSet<CompositionReason> reasons;
    bool shouldNotifiyDidComposite = false;
    {
        Locker locker { m_state.lock };

        // The timer has been stopped.
        if (!m_state.isRenderTimerActive)
            return;

        m_state.isRenderTimerActive = false;
        reasons = std::exchange(m_state.reasons, { });
        if (reasons.contains(CompositionReason::RenderingUpdate)) {
            if (m_state.isWaitingForTiles) {
                reasons.remove(CompositionReason::RenderingUpdate);
                m_state.reasons.add(CompositionReason::RenderingUpdate);
            } else
                shouldNotifiyDidComposite = !!m_state.didCompositeRenderingUpdateFunction;
        }

        ASSERT(m_state.state == State::Scheduled);
        m_state.state = State::InProgress;
    }

    if (!m_context || !m_context->makeContextCurrent())
        return;

    // Retrieve the scene attributes in a thread-safe manner.
    IntSize viewportSize;
    float deviceScaleFactor;
    {
        Locker locker { m_attributes.lock };
        viewportSize = m_attributes.viewportSize;
        deviceScaleFactor = m_attributes.deviceScaleFactor;
    }

    if (viewportSize.isEmpty())
        return;

    TransformationMatrix viewportTransform;
    viewportTransform.scale(deviceScaleFactor);

    m_surface->willRenderFrame(viewportSize);

    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }] {
        if (m_layerTreeHost)
            m_layerTreeHost->willRenderFrame();
    });

    WTFBeginSignpost(this, FlushCompositingState);
    flushCompositingState(reasons);
    WTFEndSignpost(this, FlushCompositingState);

    WTFBeginSignpost(this, PaintToGLContext);
    paintToCurrentGLContext(viewportTransform, viewportSize, reasons);
    WTFEndSignpost(this, PaintToGLContext);

    updateFPSCounter();

    if (shouldNotifiyDidComposite)
        m_didCompositeRunLoopObserver->schedule(&RunLoop::mainSingleton());

    WTFEmitSignpost(this, DidRenderFrame, "reasons: %s", reasonsToString(reasons).ascii().data());

    m_context->swapBuffers();

    m_surface->didRenderFrame();

    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }] {
        if (m_layerTreeHost)
            m_layerTreeHost->didRenderFrame();
    });
}

void ThreadedCompositor::requestCompositionForRenderingUpdate(Function<void()>&& didCompositeFunction)
{
    ASSERT(RunLoop::isMain());
    Locker locker { m_state.lock };
    m_state.reasons.add(CompositionReason::RenderingUpdate);
    ASSERT(!m_state.didCompositeRenderingUpdateFunction);
    m_state.didCompositeRenderingUpdateFunction = WTF::move(didCompositeFunction);
    if (m_sceneState->pendingTiles())
        m_state.isWaitingForTiles = true;
    scheduleUpdateLocked();
}

void ThreadedCompositor::requestComposition(CompositionReason reason)
{
    Locker locker { m_state.lock };
    m_state.reasons.add(reason);
    scheduleUpdateLocked();
}

ASCIILiteral ThreadedCompositor::stateToString(ThreadedCompositor::State state)
{
    switch (state) {
    case State::Idle:
        return "Idle"_s;
    case State::Scheduled:
        return "Scheduled"_s;
    case State::InProgress:
        return "InProgress"_s;
    case State::ScheduledWhileInProgress:
        return "ScheduledWhileInProgress"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void ThreadedCompositor::scheduleUpdateLocked()
{
    ASSERT(m_state.lock.isHeld());
    WTFEmitSignpost(this, ScheduleComposition, "reasons: %s, state: %s, waiting for tiles: %s, render timer active: %s", reasonsToString(m_state.reasons).ascii().data(), stateToString(m_state.state).characters(), m_state.isWaitingForTiles ? "yes" : "no", m_state.isRenderTimerActive ? "yes" : "no");

    switch (m_state.state) {
    case State::Idle:
        m_state.state = State::Scheduled;
        if (!m_state.isWaitingForTiles && !m_suspendedCount.load())
            startRenderTimer();
        break;
    case State::Scheduled:
        if (!m_state.isRenderTimerActive && !m_suspendedCount.load())
            startRenderTimer();
        break;
    case State::InProgress:
        m_state.state = State::ScheduledWhileInProgress;
        break;
    case State::ScheduledWhileInProgress:
        break;
    }
}

void ThreadedCompositor::frameComplete()
{
    ASSERT(m_workQueue->runLoop().isCurrent());

    Locker locker { m_state.lock };
    WTFEmitSignpost(this, FrameComplete, "reasons: %s, state: %s, waiting for tiles: %s", reasonsToString(m_state.reasons).ascii().data(), stateToString(m_state.state).characters(), m_state.isWaitingForTiles ? "yes" : "no");

    switch (m_state.state) {
    case State::Idle:
    case State::Scheduled:
        break;
    case State::InProgress:
        if (m_state.reasons.contains(CompositionReason::RenderingUpdate) && m_state.isWaitingForTiles)
            m_state.state = State::Scheduled;
        else
            m_state.state = State::Idle;
        break;
    case State::ScheduledWhileInProgress:
        m_state.state = State::Scheduled;
        if (!isOnlyRenderingUpdatePendingAndWaitingForTiles() && !m_suspendedCount.load())
            startRenderTimer();
        break;
    }
}

RunLoop* ThreadedCompositor::runLoop()
{
    return m_surface ? &m_workQueue->runLoop() : nullptr;
}

void ThreadedCompositor::didCompositeRunLoopObserverFired()
{
    m_didCompositeRunLoopObserver->invalidate();
    Function<void()> didCompositeFunction;
    {
        Locker locker { m_state.lock };
        didCompositeFunction = std::exchange(m_state.didCompositeRenderingUpdateFunction, nullptr);
    }
    if (didCompositeFunction)
        didCompositeFunction();
}

void ThreadedCompositor::updateSceneAttributes(const IntSize& size, float deviceScaleFactor)
{
    m_attributes.viewportSize = size;
    m_attributes.deviceScaleFactor = deviceScaleFactor;
    m_attributes.viewportSize.scale(m_attributes.deviceScaleFactor);
}

void ThreadedCompositor::initializeFPSCounter()
{
    // When the envvar is set, the FPS is logged to the console, so it may be necessary to enable the
    // 'LogsPageMessagesToSystemConsole' runtime preference to see it.
    const auto showFPSEnvironment = String::fromLatin1(getenv("WEBKIT_SHOW_FPS"));
    bool ok = false;
    Seconds interval(showFPSEnvironment.toDouble(&ok));
    if (ok && interval) {
        m_fpsCounter.exposesFPS = true;
        m_fpsCounter.calculationInterval = interval;
    }
}

void ThreadedCompositor::updateFPSCounter()
{
    if (!m_fpsCounter.exposesFPS
#if USE(SYSPROF_CAPTURE)
        && !SysprofAnnotator::singletonIfCreated()
#endif
    )
        return;

    m_fpsCounter.frameCountSinceLastCalculation++;
    const Seconds delta = MonotonicTime::now() - m_fpsCounter.lastCalculationTimestamp;
    if (delta >= m_fpsCounter.calculationInterval) {
        WTFSetCounter(FPS, static_cast<int>(std::round(m_fpsCounter.frameCountSinceLastCalculation / delta.seconds())));
        if (m_fpsCounter.exposesFPS)
            m_fpsCounter.fps = m_fpsCounter.frameCountSinceLastCalculation / delta.seconds();
        m_fpsCounter.frameCountSinceLastCalculation = 0;
        m_fpsCounter.lastCalculationTimestamp += delta;
    } else if (m_fpsCounter.exposesFPS)
        m_fpsCounter.fps = std::nullopt;
}

void ThreadedCompositor::fillGLInformation(RenderProcessInfo&& info, CompletionHandler<void(RenderProcessInfo&&)>&& completionHandler)
{
    m_workQueue->dispatchSync([protectedThis = Ref { *this }, info = WTF::move(info), completionHandler = WTF::move(completionHandler)]() mutable {
        info.glRenderer = String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        info.glVendor = String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
        info.glVersion = String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        info.glShadingVersion = String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
        info.glExtensions = String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));

        auto eglDisplay = eglGetCurrentDisplay();
        info.eglVersion = String::fromUTF8(eglQueryString(eglDisplay, EGL_VERSION));
        info.eglVendor = String::fromUTF8(eglQueryString(eglDisplay, EGL_VENDOR));
        info.eglExtensions = makeString(unsafeSpan(eglQueryString(nullptr, EGL_EXTENSIONS)), ' ', unsafeSpan(eglQueryString(eglDisplay, EGL_EXTENSIONS)));

        RunLoop::mainSingleton().dispatch([info = WTF::move(info), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(WTF::move(info));
        });
    });
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
