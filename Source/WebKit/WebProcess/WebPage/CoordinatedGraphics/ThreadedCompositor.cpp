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
#include <WebCore/FontCache.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/Settings.h>
#include <WebCore/SkiaCompositingLayer.h>
#include <WebCore/TextureMapperLayer.h>
#include <WebCore/TransformationMatrix.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkFont.h>
#include <skia/core/SkFontMgr.h>
#include <skia/core/SkPaint.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>

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

Ref<ThreadedCompositor> ThreadedCompositor::create(WebPage& webPage, LayerTreeHost& layerTreeHost, CoordinatedSceneState& sceneState)
{
    return adoptRef(*new ThreadedCompositor(webPage, layerTreeHost, sceneState));
}

ThreadedCompositor::ThreadedCompositor(WebPage& webPage, LayerTreeHost& layerTreeHost, CoordinatedSceneState& sceneState)
    : m_workQueue(WorkQueue::create("org.webkit.ThreadedCompositor"_s))
    , m_layerTreeHost(&layerTreeHost)
    , m_useSkia(webPage.corePage()->settings().useSkiaForComposition())
    , m_surface(AcceleratedSurface::create(webPage, [this] { frameComplete(); }, AcceleratedSurface::RenderingPurpose::Composited, m_useSkia))
    , m_sceneState(&sceneState)
    , m_flipY(m_surface->shouldPaintMirrored())
    , m_renderTimer(m_workQueue->runLoop(), "ThreadedCompositor::RenderTimer"_s, this, &ThreadedCompositor::renderLayerTree)
{
    ASSERT(RunLoop::isMain());

    m_didCompositeRunLoopObserver = makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::GraphicsCommit, [this] {
        this->didCompositeRunLoopObserverFired();
    });

    initializeFPSCounter();
#if ENABLE(DAMAGE_TRACKING)
    if (m_useSkia) {
        // WEBKIT_SHOW_DAMAGE=N renders the frame damage rects, insetting them
        // by N-1 pixels (mirrors TextureMapperDamageVisualizer's behavior).
        if (const auto* showDamageVariable = getenv("WEBKIT_SHOW_DAMAGE")) {
            if (auto value = parseInteger<unsigned>(StringView::fromLatin1(showDamageVariable)); value && *value) {
                m_damage.showSkiaDamage = true;
                m_damage.skiaDamageMargin = *value - 1;
            }
        }
    } else
        m_damage.visualizer = TextureMapperDamageVisualizer::create();
#endif

    updateSceneAttributes(webPage.size(), webPage.deviceScaleFactor());

    m_surface->didCreateCompositingRunLoop(m_workQueue->runLoop());

    m_workQueue->dispatchSync([this] {
        // GLNativeWindowType depends on the EGL implementation: reinterpret_cast works
        // for pointers (only if they are 64-bit wide and not for other cases), and static_cast for
        // numeric types (and when needed they get extended to 64-bit) but not for pointers. Using
        // a plain C cast expression in this one instance works in all cases.
        static_assert(sizeof(GLNativeWindowType) <= sizeof(uint64_t), "GLNativeWindowType must not be longer than 64 bits.");
        auto nativeSurfaceHandle = (GLNativeWindowType)m_surface->window();
        auto context = GLContext::create(PlatformDisplay::sharedDisplay(), nativeSurfaceHandle);
        if (!context || !context->makeContextCurrent()) {
            m_state.state = State::Invalidated;
            return;
        }

        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);

        if (m_useSkia) {
            PlatformDisplay::sharedDisplay().setSkiaGLContextForCurrentThread(WTF::move(context));
            m_threadSafeGrContext = PlatformDisplay::sharedDisplay().skiaGrContext()->threadSafeProxy();
        } else {
            m_context = WTF::move(context);
            m_textureMapper = TextureMapper::create();
            if (!nativeSurfaceHandle)
                m_flipY = !m_flipY;
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
        m_state.state = State::Invalidated;
    }

    m_didCompositeRunLoopObserver->invalidate();
    m_workQueue->dispatchSync([this] {
        if (!m_useSkia && (!m_context || !m_context->makeContextCurrent()))
            return;

        // Update the scene at this point ensures the layers state are correctly propagated.
        flushCompositingState(CompositionReason::RenderingUpdate);

        m_sceneState->invalidateCommittedLayers();
        m_textureMapper = nullptr;
        m_surface->willDestroyGLContext();
        m_context = nullptr;
        if (m_useSkia)
            PlatformDisplay::sharedDisplay().setSkiaGLContextForCurrentThread(nullptr);
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
    return m_state.state != State::Idle && m_state.state != State::Invalidated;
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
void ThreadedCompositor::setDamagePropagationSettings(std::optional<OptionSet<DamagePropagationFlags>> flags, unsigned rectangleThreshold)
{
    m_damage.flags = flags;
    if ((m_damage.visualizer || m_damage.showSkiaDamage) && m_damage.flags) {
        // We don't use damage when rendering layers if the visualizer is enabled, because we need to make sure the whole
        // frame is invalidated in the next paint so that previous damage rects are cleared.
        m_damage.flags->remove(DamagePropagationFlags::UseForCompositing);
    }

    rectangleThreshold = Damage::clampRectangleThreshold(rectangleThreshold);
    m_damage.rectangleThreshold = rectangleThreshold;
    if (m_surface)
        m_surface->setFrameDamageRectangleThreshold(rectangleThreshold);
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

#if ASSERT_ENABLED
    {
        Locker locker { m_state.lock };
        ASSERT(!reasons.contains(CompositionReason::RenderingUpdate) || !m_state.isWaitingForTiles);
    }
#endif

    m_sceneState->flushCompositingState(reasons, m_useSkia);
}

void ThreadedCompositor::paintToCurrentGLContext(const TransformationMatrix& matrix, const IntSize& size, const OptionSet<CompositionReason>& reasons)
{
    if (m_useSkia)
        paintToSkiaCanvas(matrix, size, reasons);
    else
        paintToTextureMapper(matrix, size, reasons);
}

void ThreadedCompositor::paintToTextureMapper(const TransformationMatrix& matrix, const IntSize& size, const OptionSet<CompositionReason>& reasons)
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

        if (!frameDamage.isEmpty())
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

void ThreadedCompositor::paintToSkiaCanvas(const TransformationMatrix& matrix, const IntSize& size, const OptionSet<CompositionReason>& reasons)
{
    auto* canvas = m_surface->canvas();
    if (!canvas)
        return;

    auto& rootLayer = m_sceneState->rootLayer().ensureSkiaTarget();
    rootLayer.setTransform(matrix);

    m_surface->clear(reasons);

    canvas->save();

    std::optional<Damage> frameDamage;
#if ENABLE(DAMAGE_TRACKING)
    if (m_damage.flags)
        frameDamage = Damage(size, m_damage.flags->contains(DamagePropagationFlags::Unified) ? Damage::Mode::BoundingBox : Damage::Mode::Rectangles);
#endif

    bool sceneHasRunningAnimations = rootLayer.paint(*canvas, frameDamage);
    canvas->restore();

#if ENABLE(DAMAGE_TRACKING)
    if (frameDamage) {
        if (m_damage.shouldNotifyFrameDamageForTesting && m_layerTreeHost)
            m_layerTreeHost->notifyFrameDamageForTesting(frameDamage->regionForTesting());

        if (!frameDamage->isEmpty())
            m_surface->setFrameDamage(WTF::move(*frameDamage));
    }
#endif

#if ENABLE(DAMAGE_TRACKING)
    if (m_damage.showSkiaDamage) {
        if (auto damage = m_surface->frameDamage())
            drawSkiaDamage(*canvas, damage);

        // When the damage visualizer is active we cannot send the original damage to the platform, as the
        // damage rects visualized in the previous frame may not get erased if the platform uses damage.
        m_surface->setFrameDamage(Damage(size, Damage::Mode::Full));
    }
#endif

    if (m_fpsCounter.drawsFPS)
        drawFPSCounter(*canvas);

    if (auto* surface = canvas->getSurface())
        PlatformDisplay::sharedDisplay().skiaGrContext()->flushAndSubmit(surface, GrSyncCpu::kNo);

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

        if (m_state.state == State::Invalidated)
            return;

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

    if (!m_useSkia && (!m_context || !m_context->makeContextCurrent()))
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

    if (m_context)
        m_context->swapBuffers();
    else
        PlatformDisplay::sharedDisplay().skiaGLContext()->swapBuffers();

    m_surface->didRenderFrame();
    m_surface->sendFrame();

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
    case State::Invalidated:
        return "Invalidated"_s;
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
    case State::Invalidated:
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
    case State::Invalidated:
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

    // WEBKIT_DRAW_FPS=1 additionally renders the FPS as an on-screen overlay,
    // reusing the calculation interval (which WEBKIT_SHOW_FPS may override).
    if (const auto* drawFPSEnvironment = getenv("WEBKIT_DRAW_FPS")) {
        if (auto enabled = parseInteger<unsigned>(StringView::fromLatin1(drawFPSEnvironment)); enabled && *enabled) {
            m_fpsCounter.exposesFPS = true;
            m_fpsCounter.drawsFPS = true;
        }
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
        m_fpsCounter.lastFPS = static_cast<int>(std::round(m_fpsCounter.frameCountSinceLastCalculation / delta.seconds()));
        WTFSetCounter(FPS, m_fpsCounter.lastFPS);
        if (m_fpsCounter.exposesFPS)
            m_fpsCounter.fps = m_fpsCounter.frameCountSinceLastCalculation / delta.seconds();
        m_fpsCounter.frameCountSinceLastCalculation = 0;
        m_fpsCounter.lastCalculationTimestamp += delta;
    } else if (m_fpsCounter.exposesFPS)
        m_fpsCounter.fps = std::nullopt;
}

void ThreadedCompositor::drawFPSCounter(SkCanvas& canvas)
{
    static SkFont font = [] {
        constexpr unsigned defaultFontSize = 14;
        unsigned fontSize = defaultFontSize;
        if (const auto* fontSizeEnvvar = getenv("WEBKIT_DRAW_FPS_FONT_SIZE")) {
            if (auto value = parseInteger<unsigned>(StringView::fromLatin1(fontSizeEnvvar)); value && *value)
                fontSize = *value;
        }
        auto typeface = FontCache::forCurrentThread().fontManager().matchFamilyStyle("monospace", SkFontStyle::Bold());
        SkFont f(typeface, fontSize);
        f.setEdging(SkFont::Edging::kAntiAlias);
        f.setSubpixel(true);
        return f;
    }();

    // Scale the box padding with the font size so the overlay stays
    // proportionate at large WEBKIT_DRAW_FPS_FONT_SIZE values
    // (~3px at the default size of 14).
    const float padding = font.getSize() * 0.2f;

    if (m_fpsCounter.lastFPS != m_fpsCounter.displayedFPS) {
        m_fpsCounter.displayedFPS = m_fpsCounter.lastFPS;
        m_fpsCounter.fpsString = String::number(m_fpsCounter.lastFPS).ascii();
        SkRect textBounds;
        font.measureText(m_fpsCounter.fpsString.data(), m_fpsCounter.fpsString.length(), SkTextEncoding::kUTF8, &textBounds);
        m_fpsCounter.backgroundWidth = textBounds.width() + padding * 2;
        m_fpsCounter.backgroundHeight = textBounds.height() + padding * 2;
        m_fpsCounter.textBaseline = -textBounds.fTop + padding;
    }

    // Drawn in device space at the top-left corner, matching the debug repaint
    // counter style used by SkiaCompositingLayer.
    SkAutoCanvasRestore autoRestore(&canvas, true);
    canvas.resetMatrix();

    SkPaint backgroundPaint;
    backgroundPaint.setColor(SK_ColorBLACK);
    backgroundPaint.setStyle(SkPaint::kFill_Style);
    canvas.drawRect(SkRect::MakeXYWH(0, 0, m_fpsCounter.backgroundWidth, m_fpsCounter.backgroundHeight), backgroundPaint);

    SkPaint textPaint;
    textPaint.setColor(SK_ColorWHITE);
    textPaint.setAntiAlias(true);
    canvas.drawString(m_fpsCounter.fpsString.data(), padding, m_fpsCounter.textBaseline, font, textPaint);
}

#if ENABLE(DAMAGE_TRACKING)
void ThreadedCompositor::drawSkiaDamage(SkCanvas& canvas, const std::optional<WebCore::Damage>& damage)
{
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SkColorSetARGB(200, 255, 0, 0));

    const auto margin = static_cast<SkScalar>(m_damage.skiaDamageMargin);
    for (const auto& rect : *damage)
        canvas.drawRect(SkRect::MakeXYWH(rect.x() - margin, rect.y() - margin, rect.width() + margin * 2, rect.height() + margin * 2), paint);
}
#endif

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

void ThreadedCompositor::releaseMemory(WTF::Critical critical)
{
    m_workQueue->dispatchSync([protectedThis = Ref { *this }, critical] {
        PlatformDisplay::sharedDisplay().skiaReleaseUnusedResources(critical);
    });
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
