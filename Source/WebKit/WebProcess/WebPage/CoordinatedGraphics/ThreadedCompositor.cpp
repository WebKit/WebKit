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
#include <WebCore/IntRect.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/Settings.h>
#include <WebCore/SkiaCompositingLayer.h>
#include <WebCore/SkiaDamageRestriction.h>
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
#include <wtf/Vector.h>
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
#if ENABLE(DAMAGE_TRACKING)
    , m_damageStatsTimer(m_workQueue->runLoop(), "ThreadedCompositor::DamageStatsTimer"_s, this, &ThreadedCompositor::dumpDamageStats)
#endif
{
    ASSERT(RunLoop::isMain());

    m_didCompositeRunLoopObserver = makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::GraphicsCommit, [this] {
        this->didCompositeRunLoopObserverFired();
    });

    initializeFPSCounter();
#if ENABLE(DAMAGE_TRACKING)
    if (m_useSkia) {
        // WEBKIT_DAMAGE_DEBUG shows Skia frame damage.
        //   1: this frame's damage as a translucent blue overlay
        //   2: the buffer's whole accumulated damage as a translucent green overlay
        //   3: clear the scene to black and redraw only the damaged region
        if (const auto* damageDebugVariable = getenv("WEBKIT_DAMAGE_DEBUG")) {
            if (auto value = parseInteger<unsigned>(StringView::fromLatin1(damageDebugVariable)); value && *value <= 3)
                m_damage.debugMode = static_cast<DamageDebugMode>(*value);
        }

        // WEBKIT_DAMAGE_STATS logs rolling averages of how fine-grained the damage is (see recordDamageStats).
        if (getenv("WEBKIT_DAMAGE_STATS"))
            m_damageStats.enabled = true;
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

#if ENABLE(DAMAGE_TRACKING)
        // Dump stats on a wall clock from the compositor thread, so a window is reported even when
        // the page is idle and no frame is drawn.
        if (m_damageStats.enabled) {
            m_damageStats.lastDump = MonotonicTime::now();
            m_damageStatsTimer.startRepeating(m_damageStats.window);
        }
#endif
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
#if ENABLE(DAMAGE_TRACKING)
        m_damageStatsTimer.stop();
#endif
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
    if (m_damage.visualizer && m_damage.flags) {
        // The TextureMapper damage visualizer needs the whole frame invalidated each paint to clear
        // the previous frame's overlay, so don't drive compositing from damage.
        m_damage.flags->remove(DamagePropagationFlags::UseForCompositing);
    }
    // WEBKIT_DAMAGE_DEBUG (Skia) keeps UseForCompositing so the damage pipeline runs unchanged. The
    // restriction is just not applied while overlays show (see paintToSkiaCanvas), so the whole frame
    // repaints to erase the previous overlay.

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

bool ThreadedCompositor::paintToCurrentGLContext(const TransformationMatrix& matrix, const IntSize& size, const OptionSet<CompositionReason>& reasons)
{
    if (m_useSkia)
        return paintToSkiaCanvas(matrix, size, reasons);

    paintToTextureMapper(matrix, size, reasons);
    return true;
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

        const bool debugIndicatorsEnabled = m_damage.debugIndicatorsEnabled;
        const bool useForCompositing = m_damage.flags->contains(DamagePropagationFlags::UseForCompositing);
        recordFrameDamage(WTF::move(frameDamage), debugIndicatorsEnabled ? DebugOverlays::Yes : DebugOverlays::No, useForCompositing ? AcceleratedSurface::AccumulateIntoSwapChain::Yes : AcceleratedSurface::AccumulateIntoSwapChain::No);

        if (useForCompositing) {
            if (debugIndicatorsEnabled) {
                // Repaint the whole frame while indicators show. Damage is unknown, so skip the scissor below.
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
        m_surface->setFrameDamage(Damage(size, Damage::Mode::Full));
    }
#endif

    m_textureMapper->endClip();
    m_textureMapper->endPainting();

    if (sceneHasRunningAnimations)
        requestComposition(CompositionReason::Animation);
}

#if ENABLE(DAMAGE_TRACKING)
void ThreadedCompositor::recordFrameDamage(Damage&& damage, DebugOverlays debugOverlays, AcceleratedSurface::AccumulateIntoSwapChain accumulate)
{
    // Debug overlays aren't part of the layer-tree damage and can move anywhere, so record full-frame
    // damage. This makes them present, and makes buffers repaint fully once the overlays are turned off.
    if (debugOverlays == DebugOverlays::Yes)
        damage.makeFull();

    if (m_damage.shouldNotifyFrameDamageForTesting && m_layerTreeHost)
        m_layerTreeHost->notifyFrameDamageForTesting(damage.regionForTesting());

    // Skip empty damage, because accumulating it would mark every swap-chain buffer fully damaged.
    if (damage.isEmpty())
        return;

    m_surface->setFrameDamage(WTF::move(damage), accumulate);
}

static void drawDamageOverlay(SkCanvas& canvas, const Damage& damage, SkColor color)
{
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(color);

    for (const auto& rect : damage.rectsForPainting())
        canvas.drawRect(SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()), paint);
}

static std::pair<unsigned, uint64_t> damageMetrics(const std::optional<Damage>& damage)
{
    if (!damage)
        return { 0, 0 };

    // rects() may return overlapping rectangles (see Damage::rects()), so simply adding their areas
    // would count shared pixels twice and can exceed the viewport area. Unite them into a Region to
    // measure the real coverage without overlaps.
    const auto rects = damage->rects();
    Region region;
    for (const auto& rect : rects)
        region.unite(Region { rect });
    return { static_cast<unsigned>(rects.size()), region.totalArea() };
}

void ThreadedCompositor::recordDamageStats(const IntSize& viewport, bool compositingUsesDamage)
{
    if (!m_damageStats.enabled)
        return;

    const uint64_t viewportArea = static_cast<uint64_t>(viewport.width()) * viewport.height();

    const auto [frameRects, frameArea] = damageMetrics(m_surface->frameDamage());
    m_damageStats.frames++;
    m_damageStats.frameRects += frameRects;
    m_damageStats.frameArea += frameArea;
    m_damageStats.viewportArea += viewportArea;

    if (compositingUsesDamage) {
        // renderTargetDamage() was already read on the restriction path this frame, so this adds no
        // work. It returns the buffer's accumulated repaint region.
        const auto [repaintRects, repaintArea] = damageMetrics(m_surface->renderTargetDamage());
        m_damageStats.repaintFrames++;
        m_damageStats.repaintRects += repaintRects;
        m_damageStats.repaintArea += repaintArea;
        m_damageStats.repaintViewportArea += viewportArea;
    }
}

void ThreadedCompositor::dumpDamageStats()
{
    const auto now = MonotonicTime::now();
    const Seconds elapsed = now - m_damageStats.lastDump;
    m_damageStats.lastDump = now;

    const unsigned frames = m_damageStats.frames;
    if (!frames)
        WTFLogAlways("[DamageStats] %.0f ms, no frame composited", elapsed.milliseconds()); // NOLINT
    else {
        const double fps = elapsed.seconds() > 0 ? frames / elapsed.seconds() : 0.0;
        const double avgFrameRects = m_damageStats.frameRects / static_cast<double>(frames);
        const double frameAreaPercent = m_damageStats.viewportArea ? 100.0 * m_damageStats.frameArea / m_damageStats.viewportArea : 0.0;
        if (m_damageStats.repaintFrames) {
            const double avgRepaintRects = m_damageStats.repaintRects / static_cast<double>(m_damageStats.repaintFrames);
            const double repaintAreaPercent = m_damageStats.repaintViewportArea ? 100.0 * m_damageStats.repaintArea / m_damageStats.repaintViewportArea : 0.0;
            WTFLogAlways("[DamageStats] %.0f ms, %u frames (%.1f fps) | frame-damage: %.1f rects, %.2f%% of viewport | repaint-region: %.1f rects, %.2f%% of viewport", // NOLINT
                elapsed.milliseconds(), frames, fps, avgFrameRects, frameAreaPercent, avgRepaintRects, repaintAreaPercent);
        } else {
            WTFLogAlways("[DamageStats] %.0f ms, %u frames (%.1f fps) | frame-damage: %.1f rects, %.2f%% of viewport | repaint-region: n/a (damage compositing off)", // NOLINT
                elapsed.milliseconds(), frames, fps, avgFrameRects, frameAreaPercent);
        }
    }

    m_damageStats.frames = 0;
    m_damageStats.frameRects = 0;
    m_damageStats.frameArea = 0;
    m_damageStats.viewportArea = 0;
    m_damageStats.repaintFrames = 0;
    m_damageStats.repaintRects = 0;
    m_damageStats.repaintArea = 0;
    m_damageStats.repaintViewportArea = 0;
}
#endif

bool ThreadedCompositor::paintToSkiaCanvas(const TransformationMatrix& matrix, const IntSize& size, const OptionSet<CompositionReason>& reasons)
{
    auto* canvas = m_surface->canvas();
    if (!canvas)
        return false;

    auto& rootLayer = m_sceneState->rootLayer().ensureSkiaTarget();
    rootLayer.setTransform(matrix);

    canvas->save();

    // No damage region paints the whole frame (see SkiaCompositingLayer::paint()).
    std::optional<DamageRegion> damageRegion;
#if ENABLE(DAMAGE_TRACKING)
    // Read the state once so the restriction decision and swap-chain accumulation below agree even if
    // the main thread changes it mid-frame. The FPS counter is a debug overlay too.
    const bool debugOverlaysEnabled = m_damage.debugIndicatorsEnabled || m_fpsCounter.drawsFPS;
    const bool useDamageForCompositing = m_damage.flags && m_damage.flags->contains(DamagePropagationFlags::UseForCompositing);
    // Overlay debug modes (1, 2) draw on top of the composited frame, and so do the layer indicators
    // and FPS counter. All of them need the whole frame repainted each paint to erase the previous
    // overlay. The rest of the damage pipeline stays intact, only the restriction is not applied.
    const bool overlayNeedsFullRepaint = debugOverlaysEnabled
        || m_damage.debugMode == DamageDebugMode::CurrentFrame
        || m_damage.debugMode == DamageDebugMode::Accumulated;
    // Also collect when a WEBKIT_DAMAGE_DEBUG mode or WEBKIT_DAMAGE_STATS is on, so the overlay and
    // statistics work even with the compositing feature disabled.
    if (m_damage.flags || m_damage.debugMode != DamageDebugMode::None || m_damageStats.enabled) {
        Damage frameDamage(size, m_damage.flags && m_damage.flags->contains(DamagePropagationFlags::Unified) ? Damage::Mode::BoundingBox : Damage::Mode::Rectangles);
        rootLayer.collectDamage(size, frameDamage);

        // Accumulate into the swap chain when damage drives compositing, so a later frame that reuses
        // a buffer still repaints what changed this frame.
        recordFrameDamage(WTF::move(frameDamage), debugOverlaysEnabled ? DebugOverlays::Yes : DebugOverlays::No,
            useDamageForCompositing ? AcceleratedSurface::AccumulateIntoSwapChain::Yes : AcceleratedSurface::AccumulateIntoSwapChain::No);
    }

    if (m_damage.debugMode == DamageDebugMode::RedrawDamagedOnly) {
        // Clear the whole scene to black and redraw only what changed this frame, so the damage shows
        // up against black.
        canvas->clear(SK_ColorBLACK);
        auto plan = planFrameRestriction(m_surface->frameDamage(), size);
        if (plan.kind == FrameRestrictionPlan::Kind::SkipPaint)
            damageRegion = DamageRegion { };
        else if (plan.kind == FrameRestrictionPlan::Kind::RestrictToDamage)
            damageRegion = WTF::move(plan.damageRegion);
        // FullRepaint leaves damageRegion unset, so the whole frame paints over the black clear.
    } else if (useDamageForCompositing && !overlayNeedsFullRepaint) {
        // Restriction is off while overlays show, so the whole frame repaints and they refresh.
        // recordFrameDamage above told the platform what changed this frame. The region planned against
        // here is what this buffer must redraw to become current. The two differ when an older buffer
        // is reused.
        auto plan = planFrameRestriction(m_surface->renderTargetDamage(), size);
        switch (plan.kind) {
        case FrameRestrictionPlan::Kind::FullRepaint:
            m_surface->clear(reasons);
            break;
        case FrameRestrictionPlan::Kind::SkipPaint:
            // Buffer already current, so hand paint() an empty region and it draws nothing.
            damageRegion = DamageRegion { };
            break;
        case FrameRestrictionPlan::Kind::RestrictToDamage: {
            // Clip the clear to the damage region so undamaged content stays and translucent content
            // composites once.
            SkAutoCanvasRestore autoRestore(canvas, true);
            clipToDamageInDeviceSpace(*canvas, *plan.damageRegion);
            m_surface->clear(reasons);
            damageRegion = WTF::move(plan.damageRegion);
            break;
        }
        }
    } else
        m_surface->clear(reasons);

    // renderTargetDamage() is read above only on the restriction path, so read the repaint region
    // only there to keep this free of side effects.
    recordDamageStats(size, useDamageForCompositing && !overlayNeedsFullRepaint);
#else
    m_surface->clear(reasons);
#endif

    rootLayer.paint(*canvas, WTF::move(damageRegion));
    canvas->restore();

#if ENABLE(DAMAGE_TRACKING)
    // Overlay modes draw the damage rects on top of the finished frame.
    bool didDrawOverlay = false;
    if (m_damage.debugMode == DamageDebugMode::CurrentFrame) {
        if (const auto& damage = m_surface->frameDamage(); damage && !damage->isEmpty()) {
            drawDamageOverlay(*canvas, *damage, SkColorSetARGB(128, 0, 0, 255));
            didDrawOverlay = true;
        }
    } else if (m_damage.debugMode == DamageDebugMode::Accumulated) {
        if (const auto& damage = m_surface->renderTargetDamage(); damage && !damage->isEmpty()) {
            drawDamageOverlay(*canvas, *damage, SkColorSetARGB(128, 0, 255, 0));
            didDrawOverlay = true;
        }
    }

    // Every debug mode repaints or clears the whole surface each frame, so report full damage to the
    // platform. Otherwise the previous frame's overlay (or black fill) may not get erased.
    if (m_damage.debugMode != DamageDebugMode::None)
        m_surface->setFrameDamage(Damage(size, Damage::Mode::Full));

    // An overlay was drawn into this buffer. Schedule one more composition so that once damage stops,
    // the next frame repaints the overlay away instead of leaving it frozen on screen on an idle page.
    if (didDrawOverlay)
        requestComposition(CompositionReason::Animation);
#endif

    if (m_fpsCounter.drawsFPS)
        drawFPSCounter(*canvas);

    if (auto* surface = canvas->getSurface())
        PlatformDisplay::sharedDisplay().skiaGrContext()->flushAndSubmit(surface, GrSyncCpu::kNo);

    if (rootLayer.hasRunningAnimations())
        requestComposition(CompositionReason::Animation);

    return true;
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
    bool framePainted = paintToCurrentGLContext(viewportTransform, viewportSize, reasons);
    WTFEndSignpost(this, PaintToGLContext);

    updateFPSCounter();

    if (shouldNotifiyDidComposite)
        m_didCompositeRunLoopObserver->schedule(&RunLoop::mainSingleton());

    WTFEmitSignpost(this, DidRenderFrame, "reasons: %s", reasonsToString(reasons).ascii().data());

    if (m_context)
        m_context->swapBuffers();
    else
        PlatformDisplay::sharedDisplay().skiaGLContext()->swapBuffers();

#if ENABLE(DAMAGE_TRACKING)
    // Fine-grained target damage only matters when the Skia compositor draws only the rects.
    // TextureMapper consumers only read bounds. A WEBKIT_DAMAGE_DEBUG mode keeps UseForCompositing, so
    // the records stay fine-grained and the overlay shows the real rects.
    const bool damageUsedForCompositing = m_useSkia && m_damage.flags && m_damage.flags->contains(DamagePropagationFlags::UseForCompositing);
#else
    const bool damageUsedForCompositing = false;
#endif
    m_surface->didRenderFrame(framePainted ? AcceleratedSurface::FramePainted::Yes : AcceleratedSurface::FramePainted::No,
        damageUsedForCompositing ? AcceleratedSurface::DamageUsedForCompositing::Yes : AcceleratedSurface::DamageUsedForCompositing::No);
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
