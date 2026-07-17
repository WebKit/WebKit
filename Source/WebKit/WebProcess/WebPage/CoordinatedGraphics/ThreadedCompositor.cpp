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
#include <WebCore/SkiaDamageRegion.h>
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
    , m_flipY(!m_surface->shouldPaintMirrored())
    , m_renderTimer(m_workQueue->runLoop(), "ThreadedCompositor::RenderTimer"_s, this, &ThreadedCompositor::renderLayerTree)
{
    ASSERT(RunLoop::isMain());

    m_didCompositeRunLoopObserver = makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::GraphicsCommit, [this] {
        this->didCompositeRunLoopObserverFired();
    });

    initializeFPSCounter();
#if ENABLE(DAMAGE_TRACKING)
    if (m_useSkia) {
        // The margin TextureMapperDamageVisualizer takes means nothing here, since the overlay fills the
        // damage as a region, but the same variable enables both so it does not depend on the compositor.
        if (const auto* showDamageEnvvar = getenv("WEBKIT_SHOW_DAMAGE")) {
            if (auto value = parseInteger<unsigned>(StringView::fromLatin1(showDamageEnvvar)); value && *value)
                m_damage.showAccumulatedDamageOverlay = true;
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
    updateRenderTimer();
}

void ThreadedCompositor::stopRenderTimer()
{
    ASSERT(m_state.lock.isHeld());
    m_state.isRenderTimerActive = false;
    updateRenderTimer();
}

void ThreadedCompositor::updateRenderTimer()
{
    ASSERT(m_state.lock.isHeld());

    // m_renderTimer is bound to the compositor thread's run loop (m_workQueue), so it must be started
    // and stopped there. The render timer's desired state is tracked synchronously under m_state.lock by
    // m_state.isRenderTimerActive and may be changed from the main thread (e.g. from suspend()/resume()/
    // invalidate()), so hop to the compositor thread to bring the timer in line with that state.
    if (!m_workQueue->runLoop().isCurrent()) {
        m_workQueue->dispatch([protectedThis = Ref { *this }] {
            Locker locker { protectedThis->m_state.lock };
            protectedThis->updateRenderTimer();
        });
        return;
    }

    if (m_state.isRenderTimerActive) {
        if (!m_renderTimer.isActive())
            m_renderTimer.startOneShot(0_s);
    } else
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

    rectangleThreshold = Damage::clampRectangleThreshold(rectangleThreshold);
    if (m_surface) {
        m_surface->setFrameDamageRectangleThreshold(rectangleThreshold);
        m_surface->setDamageUsedForCompositing(damageUsedForCompositing());
    }
}

bool ThreadedCompositor::damageUsedForCompositing() const
{
    // Only the Skia compositor draws just the damage rects. TextureMapper scissors with the damage
    // bounds, which needs neither fine-grained records nor swap-chain accumulation.
    return m_useSkia && m_damage.flags && m_damage.flags->contains(DamagePropagationFlags::UseForCompositing);
}

bool ThreadedCompositor::drawsOverlay() const
{
    // The damage overlay is drawn by paintToSkiaCanvas(), the visualizer by paintToTextureMapper(), so
    // each path only has its own. Both draw over the damage itself, and damaging what they drew would
    // feed their own rects back into what they show next, growing until they cover the surface. So the
    // frame repaints in full instead. The FPS counter has no such loop and damages its own box.
    if (m_useSkia)
        return m_damage.showAccumulatedDamageOverlay;

    return !!m_damage.visualizer;
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

TargetContents ThreadedCompositor::paintToCurrentGLContext(const TransformationMatrix& matrix, const IntSize& size, const OptionSet<CompositionReason>& reasons)
{
    if (m_useSkia)
        return paintToSkiaCanvas(matrix, size, reasons);

    paintToTextureMapper(matrix, size, reasons);
    return TargetContents::Valid;
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

        recordFrameDamage(WTF::move(frameDamage));

        if (m_damage.flags->contains(DamagePropagationFlags::UseForCompositing)) {
            if (drawsOverlay()) {
                // No damage means repaint the whole frame, so skip the scissor below too.
                m_textureMapper->setDamage(std::nullopt);
            } else {
                const auto& damageSinceLastSurfaceUse = m_surface->renderTargetDamage();
                if (damageSinceLastSurfaceUse && !FloatRect(damageSinceLastSurfaceUse->bounds()).contains(clipRect))
                    rectContainingRegionThatActuallyChanged = FloatRoundedRect(damageSinceLastSurfaceUse->bounds());

                m_textureMapper->setDamage(damageSinceLastSurfaceUse);
            }
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
        m_surface->setFrameDamageForPlatformOnly(Damage(size, Damage::Mode::Full));
    }
#endif

    m_textureMapper->endClip();
    m_textureMapper->endPainting();

    if (sceneHasRunningAnimations)
        requestComposition(CompositionReason::Animation);
}

#if ENABLE(DAMAGE_TRACKING)
void ThreadedCompositor::recordFrameDamage(Damage&& damage)
{
    if (m_damage.shouldNotifyFrameDamageForTesting && m_layerTreeHost)
        m_layerTreeHost->notifyFrameDamageForTesting(damage.regionForTesting());

    // Nothing changed this frame: an empty damage contributes nothing to the targets, since Damage::add()
    // ignores empty damage. Skip it rather than replace the recorded frame damage with an empty one.
    if (damage.isEmpty())
        return;

    m_surface->setFrameDamage(WTF::move(damage));
}

static void drawDamageOverlay(SkCanvas& canvas, const Damage& damage, const IntSize& size, SkColor color)
{
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(color);

    // Drawn as a region, so the translucent overlay composites once even where the rects touch. No region
    // means the damage covers the whole surface, which the overlay has to show as such.
    if (auto damageRegion = SkiaDamageRegion::create(damage, size))
        damageRegion->fillCanvasInDeviceSpace(canvas, paint);
    else
        canvas.drawPaint(paint);
}
#endif

TargetContents ThreadedCompositor::paintToSkiaCanvas(const TransformationMatrix& matrix, const IntSize& size, const OptionSet<CompositionReason>& reasons)
{
    auto* canvas = m_surface->canvas();
    if (!canvas)
        return TargetContents::Invalid;

    auto& rootLayer = m_sceneState->rootLayer().ensureSkiaTarget();
    rootLayer.setTransform(matrix);

    // paint() collects this frame's damage into frameDamage, and limits the draw to what this target must
    // redraw: priorTargetDamage - what changed since the target was last current, read before this frame
    // is recorded - folded with frameDamage. An unset priorTargetDamage repaints the whole target.
    std::optional<Damage> frameDamage;
    std::optional<Damage> priorTargetDamage;
    const std::optional<SkColor> clearColor = m_surface->skiaClearColor(reasons);

#if ENABLE(DAMAGE_TRACKING)
    // Also collect for the accumulated-damage overlay, so it works with the compositing feature off.
    if (m_damage.flags || m_damage.showAccumulatedDamageOverlay)
        frameDamage = Damage(size, m_damage.flags && m_damage.flags->contains(DamagePropagationFlags::Unified) ? Damage::Mode::BoundingBox : Damage::Mode::Rectangles);

    // No layer damages the counter's box, so seed it here, before paint() collects.
    if (frameDamage && m_fpsCounter.drawsFPS)
        frameDamage->add(takeFPSCounterDamage());

    // Read the target's record before this frame is recorded into it.
    if (damageUsedForCompositing() && !drawsOverlay())
        priorTargetDamage = m_surface->renderTargetDamage();
#endif

    canvas->save();
    const bool hasRunningAnimations = rootLayer.paint(*canvas, frameDamage, priorTargetDamage, clearColor);
    canvas->restore();

#if ENABLE(DAMAGE_TRACKING)
    // Record into every target, so each one repaints this frame's damage the next time it is used.
    if (frameDamage)
        recordFrameDamage(WTF::move(*frameDamage));

    if (m_damage.showAccumulatedDamageOverlay) {
        if (const auto& damage = m_surface->renderTargetDamage(); damage && !damage->isEmpty()) {
            drawDamageOverlay(*canvas, *damage, size, SkColorSetARGB(128, 0, 255, 0));

            // Schedule one more composition, so that once damage stops the next frame repaints the
            // overlay away instead of leaving it frozen on an idle page.
            requestComposition(CompositionReason::Animation);
        }
    }

    // An overlay is redrawn every frame and is in no layer's damage, so tell the platform the whole
    // surface changed. Otherwise it may leave the previous frame's overlay on screen.
    if (drawsOverlay())
        m_surface->setFrameDamageForPlatformOnly(Damage(size, Damage::Mode::Full));
#endif

    if (m_fpsCounter.drawsFPS)
        drawFPSCounter(*canvas);

    if (auto* surface = canvas->getSurface())
        PlatformDisplay::sharedDisplay().skiaGrContext()->flushAndSubmit(surface, GrSyncCpu::kNo);

    if (hasRunningAnimations)
        requestComposition(CompositionReason::Animation);

    return TargetContents::Valid;
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
    const auto targetContents = paintToCurrentGLContext(viewportTransform, viewportSize, reasons);
    WTFEndSignpost(this, PaintToGLContext);

    updateFPSCounter();

    if (shouldNotifiyDidComposite)
        m_didCompositeRunLoopObserver->schedule(&RunLoop::mainSingleton());

    WTFEmitSignpost(this, DidRenderFrame, "reasons: %s", reasonsToString(reasons).ascii().data());

    if (m_context)
        m_context->swapBuffers();
    else
        PlatformDisplay::sharedDisplay().skiaGLContext()->swapBuffers();

    m_surface->didRenderFrame(targetContents);
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

static const SkFont& fpsCounterFont()
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
    return font;
}

// Scale the box padding with the font size so the overlay stays proportionate at large
// WEBKIT_DRAW_FPS_FONT_SIZE values (~3px at the default size of 14).
static float fpsCounterPadding()
{
    return fpsCounterFont().getSize() * 0.2f;
}

FloatRect ThreadedCompositor::fpsCounterRect() const
{
    return FloatRect(0, 0, m_fpsCounter.backgroundWidth, m_fpsCounter.backgroundHeight);
}

void ThreadedCompositor::updateFPSCounterGeometry()
{
    if (m_fpsCounter.lastFPS == m_fpsCounter.displayedFPS)
        return;

    m_fpsCounter.displayedFPS = m_fpsCounter.lastFPS;
    m_fpsCounter.fpsString = String::number(m_fpsCounter.lastFPS).ascii();

    const auto& font = fpsCounterFont();
    SkRect textBounds;
    font.measureText(m_fpsCounter.fpsString.data(), m_fpsCounter.fpsString.length(), SkTextEncoding::kUTF8, &textBounds);

    const float padding = fpsCounterPadding();
    m_fpsCounter.backgroundWidth = textBounds.width() + padding * 2;
    m_fpsCounter.backgroundHeight = textBounds.height() + padding * 2;
    m_fpsCounter.textBaseline = -textBounds.fTop + padding;
}

#if ENABLE(DAMAGE_TRACKING)
IntRect ThreadedCompositor::takeFPSCounterDamage()
{
    // Nothing to repaint on the frames between counts, and drawFPSCounter() redraws the box regardless.
    if (m_fpsCounter.lastFPS == m_fpsCounter.displayedFPS)
        return { };

    updateFPSCounterGeometry();

    // The box drawn last frame is damaged too, since it shrinks when the count gets shorter.
    auto damage = enclosingIntRect(fpsCounterRect());
    damage.unite(std::exchange(m_fpsCounter.lastDrawnRect, damage));
    return damage;
}
#endif

void ThreadedCompositor::drawFPSCounter(SkCanvas& canvas)
{
    updateFPSCounterGeometry();

    // Drawn in device space at the top-left corner, matching the debug repaint
    // counter style used by SkiaCompositingLayer.
    SkAutoCanvasRestore autoRestore(&canvas, true);
    canvas.resetMatrix();

    SkPaint backgroundPaint;
    backgroundPaint.setColor(SK_ColorBLACK);
    backgroundPaint.setStyle(SkPaint::kFill_Style);
    canvas.drawRect(SkRect(fpsCounterRect()), backgroundPaint);

    SkPaint textPaint;
    textPaint.setColor(SK_ColorWHITE);
    textPaint.setAntiAlias(true);
    canvas.drawString(m_fpsCounter.fpsString.data(), fpsCounterPadding(), m_fpsCounter.textBaseline, fpsCounterFont(), textPaint);
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

void ThreadedCompositor::releaseMemory(WTF::Critical critical)
{
    m_workQueue->dispatchSync([protectedThis = Ref { *this }, critical] {
        PlatformDisplay::sharedDisplay().skiaReleaseUnusedResources(critical);
    });
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
