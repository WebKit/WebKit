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

#include "config.h"
#include "ThreadedCompositor.h"

#if USE(COORDINATED_GRAPHICS)
#include "AcceleratedSurface.h"
#include "CompositingRunLoop.h"
#include "CoordinatedSceneState.h"
#include "LayerTreeHost.h"
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

#if !HAVE(DISPLAY_LINK)
#include "ThreadedDisplayRefreshMonitor.h"
#endif

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES2/gl2.h>
#endif

namespace WebKit {
using namespace WebCore;

#if !HAVE(DISPLAY_LINK)
static constexpr unsigned c_defaultRefreshRate = 60000;
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(ThreadedCompositor);

#if HAVE(DISPLAY_LINK)
Ref<ThreadedCompositor> ThreadedCompositor::create(LayerTreeHost& layerTreeHost)
{
    return adoptRef(*new ThreadedCompositor(layerTreeHost));
}
#else
Ref<ThreadedCompositor> ThreadedCompositor::create(LayerTreeHost& layerTreeHost, ThreadedDisplayRefreshMonitor::Client& displayRefreshMonitorClient, PlatformDisplayID displayID)
{
    return adoptRef(*new ThreadedCompositor(layerTreeHost, displayRefreshMonitorClient, displayID));
}
#endif

#if HAVE(DISPLAY_LINK)
ThreadedCompositor::ThreadedCompositor(LayerTreeHost& layerTreeHost)
#else
ThreadedCompositor::ThreadedCompositor(LayerTreeHost& layerTreeHost, ThreadedDisplayRefreshMonitor::Client& displayRefreshMonitorClient, PlatformDisplayID displayID)
#endif
    : m_layerTreeHost(&layerTreeHost)
    , m_surface(AcceleratedSurface::create(*this, layerTreeHost.webPage(), [this] { frameComplete(); }))
    , m_sceneState(&m_layerTreeHost->sceneState())
    , m_flipY(m_surface->shouldPaintMirrored())
    , m_compositingRunLoop(makeUnique<CompositingRunLoop>([this] { renderLayerTree(); }))
#if ENABLE(DAMAGE_TRACKING)
    , m_damageVisualizer(TextureMapperDamageVisualizer::create())
#endif
#if HAVE(DISPLAY_LINK)
    , m_didRenderFrameTimer(RunLoop::main(), this, &ThreadedCompositor::didRenderFrameTimerFired)
#else
    , m_displayRefreshMonitor(ThreadedDisplayRefreshMonitor::create(displayID, displayRefreshMonitorClient, WebCore::DisplayUpdate { 0, c_defaultRefreshRate / 1000 }))
#endif
{
    ASSERT(RunLoop::isMain());

    const auto& webPage = m_layerTreeHost->webPage();
    updateSceneAttributes(webPage.size(), webPage.deviceScaleFactor());

    m_surface->didCreateCompositingRunLoop(m_compositingRunLoop->runLoop());

#if HAVE(DISPLAY_LINK)
#if USE(GLIB_EVENT_LOOP)
    m_didRenderFrameTimer.setPriority(RunLoopSourcePriority::RunLoopTimer - 1);
#endif
#else
    m_display.displayID = displayID;
    m_display.displayUpdate = { 0, c_defaultRefreshRate / 1000 };
#endif

    m_compositingRunLoop->performTaskSync([this, protectedThis = Ref { *this }] {
#if !HAVE(DISPLAY_LINK)
        m_display.updateTimer = makeUnique<RunLoop::Timer>(RunLoop::currentSingleton(), this, &ThreadedCompositor::displayUpdateFired);
#if USE(GLIB_EVENT_LOOP)
        m_display.updateTimer->setPriority(RunLoopSourcePriority::CompositingThreadUpdateTimer);
        m_display.updateTimer->setName("[WebKit] ThreadedCompositor::DisplayUpdate");
#endif
        m_display.updateTimer->startOneShot(Seconds { 1.0 / m_display.displayUpdate.updatesPerSecond });
#endif

        // GLNativeWindowType depends on the EGL implementation: reinterpret_cast works
        // for pointers (only if they are 64-bit wide and not for other cases), and static_cast for
        // numeric types (and when needed they get extended to 64-bit) but not for pointers. Using
        // a plain C cast expression in this one instance works in all cases.
        static_assert(sizeof(GLNativeWindowType) <= sizeof(uint64_t), "GLNativeWindowType must not be longer than 64 bits.");
        auto nativeSurfaceHandle = (GLNativeWindowType)m_surface->window();
        m_context = GLContext::create(nativeSurfaceHandle, PlatformDisplay::sharedDisplay());
        if (m_context && m_context->makeContextCurrent()) {
            if (!nativeSurfaceHandle)
                m_flipY = !m_flipY;

            m_surface->didCreateGLContext();
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
    m_compositingRunLoop->stopUpdates();
#if HAVE(DISPLAY_LINK)
    m_didRenderFrameTimer.stop();
#else
    m_displayRefreshMonitor->invalidate();
#endif
    m_compositingRunLoop->performTaskSync([this, protectedThis = Ref { *this }] {
        if (!m_context || !m_context->makeContextCurrent())
            return;

        // Update the scene at this point ensures the layers state are correctly propagated.
        updateSceneState();

        m_sceneState->invalidateCommittedLayers();
        m_textureMapper = nullptr;
        m_surface->willDestroyGLContext();
        m_context = nullptr;
        m_surface->finalize();

#if !HAVE(DISPLAY_LINK)
        m_display.updateTimer = nullptr;
#endif
    });
    m_sceneState = nullptr;
    m_layerTreeHost = nullptr;
    m_surface->willDestroyCompositingRunLoop();
    m_compositingRunLoop = nullptr;
    m_surface = nullptr;
}

void ThreadedCompositor::suspend()
{
    ASSERT(RunLoop::isMain());
    m_surface->visibilityDidChange(false);

    if (++m_suspendedCount > 1)
        return;

    m_compositingRunLoop->suspend();
}

void ThreadedCompositor::resume()
{
    ASSERT(RunLoop::isMain());
    m_surface->visibilityDidChange(true);

    ASSERT(m_suspendedCount > 0);
    if (--m_suspendedCount > 0)
        return;

    m_compositingRunLoop->resume();
}

bool ThreadedCompositor::isActive() const
{
    return m_compositingRunLoop->isActive();
}

void ThreadedCompositor::backgroundColorDidChange()
{
    ASSERT(RunLoop::isMain());
    m_surface->backgroundColorDidChange();
}

#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
void ThreadedCompositor::preferredBufferFormatsDidChange()
{
    ASSERT(RunLoop::isMain());
    m_surface->preferredBufferFormatsDidChange();
}
#endif

void ThreadedCompositor::setSize(const IntSize& size, float deviceScaleFactor)
{
    ASSERT(RunLoop::isMain());
    Locker locker { m_attributes.lock };
    updateSceneAttributes(size, deviceScaleFactor);
}

#if ENABLE(DAMAGE_TRACKING)
void ThreadedCompositor::setDamagePropagation(Damage::Propagation damagePropagation)
{
    m_damagePropagation = damagePropagation;
}
#endif

void ThreadedCompositor::updateSceneState()
{
    if (!m_textureMapper)
        m_textureMapper = TextureMapper::create();

    m_sceneState->rootLayer().flushCompositingState(*m_textureMapper);
    for (auto& layer : m_sceneState->committedLayers())
        layer->flushCompositingState(*m_textureMapper);
}

void ThreadedCompositor::paintToCurrentGLContext(const TransformationMatrix& matrix, const IntSize& size)
{
    updateSceneState();

    FloatRect clipRect(FloatPoint { }, size);
    TextureMapperLayer& currentRootLayer = m_sceneState->rootLayer().ensureTarget();
    if (currentRootLayer.transform() != matrix)
        currentRootLayer.setTransform(matrix);

    bool sceneHasRunningAnimations = currentRootLayer.applyAnimationsRecursively(MonotonicTime::now());

    m_textureMapper->beginPainting(m_flipY ? TextureMapper::FlipY::Yes : TextureMapper::FlipY::No);
    m_textureMapper->beginClip(TransformationMatrix(), FloatRoundedRect(clipRect));

    std::optional<FloatRoundedRect> rectContainingRegionThatActuallyChanged;
#if ENABLE(DAMAGE_TRACKING)
    currentRootLayer.prepareForPainting(*m_textureMapper);
    Damage frameDamage;
    if (m_damagePropagation != Damage::Propagation::None) {
        WTFBeginSignpost(this, CollectDamage);
        currentRootLayer.collectDamage(*m_textureMapper, frameDamage);
        WTFEndSignpost(this, CollectDamage);

        if (m_damagePropagation == Damage::Propagation::Unified) {
            Damage boundsDamage;
            boundsDamage.add(frameDamage.bounds());
            frameDamage = WTFMove(boundsDamage);
        }

        const auto& damageSinceLastSurfaceUse = m_surface->addDamage(!frameDamage.isInvalid() && !frameDamage.isEmpty() ? frameDamage : Damage::invalid());
        if (m_frameDamageHistory) {
            // Passing damage rects through region to remove overlaps so that rects are more predictable from the testing perspective.
            // In other words - the tests should test the damaged area - not the way it's stored internally.
            m_frameDamageHistory->addDamage(std::make_pair(!frameDamage.isInvalid(), frameDamage.region()));
        }

        if (!m_damageVisualizer && !damageSinceLastSurfaceUse.isInvalid() && !FloatRect(damageSinceLastSurfaceUse.bounds()).contains(clipRect))
            rectContainingRegionThatActuallyChanged = FloatRoundedRect(damageSinceLastSurfaceUse.bounds());
        if (!m_damageVisualizer)
            m_textureMapper->setDamage(damageSinceLastSurfaceUse);
    }
#endif

    if (rectContainingRegionThatActuallyChanged)
        m_textureMapper->beginClip(TransformationMatrix(), *rectContainingRegionThatActuallyChanged);

    WTFBeginSignpost(this, PaintTextureMapperLayerTree);
    currentRootLayer.paint(*m_textureMapper);
    WTFEndSignpost(this, PaintTextureMapperLayerTree);

    if (rectContainingRegionThatActuallyChanged)
        m_textureMapper->endClip();

    m_fpsCounter.updateFPSAndDisplay(*m_textureMapper, clipRect.location(), matrix);
#if ENABLE(DAMAGE_TRACKING)
    if (m_damageVisualizer)
        m_damageVisualizer->paintDamage(*m_textureMapper, frameDamage);
#endif

    m_textureMapper->endClip();
    m_textureMapper->endPainting();

    if (sceneHasRunningAnimations)
        scheduleUpdate();
}

void ThreadedCompositor::renderLayerTree()
{
    ASSERT(m_sceneState);
    ASSERT(m_compositingRunLoop->isCurrent());
#if PLATFORM(GTK) || PLATFORM(WPE)
    TraceScope traceScope(RenderLayerTreeStart, RenderLayerTreeEnd);
#endif

    if (m_suspendedCount > 0)
        return;

    if (!m_context || !m_context->makeContextCurrent())
        return;

#if !HAVE(DISPLAY_LINK)
    m_display.updateTimer->stop();
#endif

    // Retrieve the scene attributes in a thread-safe manner.
    IntSize viewportSize;
    float deviceScaleFactor;
    {
        Locker locker { m_attributes.lock };
        viewportSize = m_attributes.viewportSize;
        deviceScaleFactor = m_attributes.deviceScaleFactor;

#if !HAVE(DISPLAY_LINK)
        // Client has to be notified upon finishing this scene update.
        m_attributes.clientRendersNextFrame = m_sceneState->layersDidChange();
#endif
    }

    if (viewportSize.isEmpty())
        return;

    TransformationMatrix viewportTransform;
    viewportTransform.scale(deviceScaleFactor);

    // Resize the surface, if necessary, before the will-render-frame call is dispatched.
    // GL viewport is updated separately, if necessary. This establishes sequencing where
    // everything inside the will-render and did-render scope is done for a constant-sized scene,
    // and similarly all GL operations are done inside that specific scope.
    bool needsGLViewportResize = m_surface->resize(viewportSize);

    m_surface->willRenderFrame();
    RunLoop::protectedMain()->dispatch([this, protectedThis = Ref { *this }] {
        if (m_layerTreeHost)
            m_layerTreeHost->willRenderFrame();
    });

    if (needsGLViewportResize)
        glViewport(0, 0, viewportSize.width(), viewportSize.height());

    m_surface->clearIfNeeded();

    WTFBeginSignpost(this, PaintToGLContext);
    paintToCurrentGLContext(viewportTransform, viewportSize);
    WTFEndSignpost(this, PaintToGLContext);

    uint32_t compositionRequestID = m_compositionRequestID.load();
#if HAVE(DISPLAY_LINK)
    m_compositionResponseID = compositionRequestID;
    if (!m_didRenderFrameTimer.isActive())
        m_didRenderFrameTimer.startOneShot(0_s);
#elif !HAVE(OS_SIGNPOST) && !USE(SYSPROF_CAPTURE)
    UNUSED_VARIABLE(compositionRequestID);
#endif

    WTFEmitSignpost(this, DidRenderFrame, "compositionResponseID %i", compositionRequestID);

    m_context->swapBuffers();

    m_surface->didRenderFrame();

    RunLoop::protectedMain()->dispatch([this, protectedThis = Ref { *this }] {
        if (m_layerTreeHost)
            m_layerTreeHost->didRenderFrame();
    });
}

uint32_t ThreadedCompositor::requestComposition()
{
    ASSERT(RunLoop::isMain());
    uint32_t compositionRequestID = ++m_compositionRequestID;
    scheduleUpdate();
    return compositionRequestID;
}

void ThreadedCompositor::scheduleUpdate()
{
    m_compositingRunLoop->scheduleUpdate();
}

RunLoop* ThreadedCompositor::runLoop()
{
    if (!m_compositingRunLoop)
        return nullptr;

    return &m_compositingRunLoop->runLoop();
}

void ThreadedCompositor::frameComplete()
{
    WTFEmitSignpost(this, FrameComplete);

    ASSERT(m_compositingRunLoop->isCurrent());
#if !HAVE(DISPLAY_LINK)
    displayUpdateFired();
    sceneUpdateFinished();
#else
    Locker stateLocker { m_compositingRunLoop->stateLock() };
    m_compositingRunLoop->updateCompleted(stateLocker);
#endif
}

#if HAVE(DISPLAY_LINK)
void ThreadedCompositor::didRenderFrameTimerFired()
{
    if (m_layerTreeHost)
        m_layerTreeHost->didComposite(m_compositionResponseID);
}
#else
WebCore::DisplayRefreshMonitor& ThreadedCompositor::displayRefreshMonitor() const
{
    return m_displayRefreshMonitor.get();
}

void ThreadedCompositor::displayUpdateFired()
{
    m_display.displayUpdate = m_display.displayUpdate.nextUpdate();

    WebProcess::singleton().eventDispatcher().notifyScrollingTreesDisplayDidRefresh(m_display.displayID);

    m_display.updateTimer->startOneShot(Seconds { 1.0 / m_display.displayUpdate.updatesPerSecond });
}

void ThreadedCompositor::sceneUpdateFinished()
{
    // The composition has finished. Now we have to determine how to manage
    // the scene update completion.

    // The DisplayRefreshMonitor will be used to dispatch a callback on the client thread if:
    //  - clientRendersNextFrame is true (i.e. client has to be notified about the finished update), or
    //  - a DisplayRefreshMonitor callback was requested from the Web engine
    bool shouldDispatchDisplayRefreshCallback = m_displayRefreshMonitor->requiresDisplayRefreshCallback(m_display.displayUpdate);

    if (!shouldDispatchDisplayRefreshCallback) {
        Locker locker { m_attributes.lock };
        shouldDispatchDisplayRefreshCallback |= m_attributes.clientRendersNextFrame;
    }

    Locker stateLocker { m_compositingRunLoop->stateLock() };

    // Schedule the DisplayRefreshMonitor callback, if necessary.
    if (shouldDispatchDisplayRefreshCallback)
        m_displayRefreshMonitor->dispatchDisplayRefreshCallback();

    // Mark the scene update as completed.
    m_compositingRunLoop->updateCompleted(stateLocker);
}
#endif // !HAVE(DISPLAY_LINK)

#if ENABLE(DAMAGE_TRACKING)
void ThreadedCompositor::resetFrameDamageHistory()
{
    m_frameDamageHistory = WTF::makeUnique<FrameDamageHistory>();
}
#endif

void ThreadedCompositor::updateSceneAttributes(const IntSize& size, float deviceScaleFactor)
{
    m_attributes.viewportSize = size;
    m_attributes.deviceScaleFactor = deviceScaleFactor;
    m_attributes.viewportSize.scale(m_attributes.deviceScaleFactor);
}

}
#endif // USE(COORDINATED_GRAPHICS)
