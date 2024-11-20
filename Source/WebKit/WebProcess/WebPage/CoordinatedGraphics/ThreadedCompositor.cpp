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
#include "LayerTreeHost.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/PlatformDisplay.h>
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
Ref<ThreadedCompositor> ThreadedCompositor::create(LayerTreeHost& layerTreeHost, float scaleFactor)
{
    return adoptRef(*new ThreadedCompositor(layerTreeHost, scaleFactor));
}
#else
Ref<ThreadedCompositor> ThreadedCompositor::create(LayerTreeHost& layerTreeHost, ThreadedDisplayRefreshMonitor::Client& displayRefreshMonitorClient, float scaleFactor, PlatformDisplayID displayID)
{
    return adoptRef(*new ThreadedCompositor(layerTreeHost, displayRefreshMonitorClient, scaleFactor, displayID));
}
#endif

#if HAVE(DISPLAY_LINK)
ThreadedCompositor::ThreadedCompositor(LayerTreeHost& layerTreeHost, float scaleFactor)
#else
ThreadedCompositor::ThreadedCompositor(LayerTreeHost& layerTreeHost, ThreadedDisplayRefreshMonitor::Client& displayRefreshMonitorClient, float scaleFactor, PlatformDisplayID displayID)
#endif
    : m_layerTreeHost(&layerTreeHost)
    , m_surface(AcceleratedSurface::create(layerTreeHost.webPage(), [this] { frameComplete(); }))
    , m_flipY(m_surface->shouldPaintMirrored())
    , m_compositingRunLoop(makeUnique<CompositingRunLoop>([this] { renderLayerTree(); }))
#if HAVE(DISPLAY_LINK)
    , m_didRenderFrameTimer(RunLoop::main(), this, &ThreadedCompositor::didRenderFrameTimerFired)
#else
    , m_displayRefreshMonitor(ThreadedDisplayRefreshMonitor::create(displayID, displayRefreshMonitorClient, WebCore::DisplayUpdate { 0, c_defaultRefreshRate / 1000 }))
#endif
{
    ASSERT(RunLoop::isMain());

    m_surface->didCreateCompositingRunLoop(m_compositingRunLoop->runLoop());

    m_attributes.viewportSize = m_surface->size();
    m_attributes.needsResize = !m_attributes.viewportSize.isEmpty();
    m_attributes.scaleFactor = scaleFactor;

    auto& webPage = layerTreeHost.webPage();
    m_damagePropagation = ([](const WebCore::Settings& settings) {
        if (!settings.propagateDamagingInformation())
            return DamagePropagation::None;
        if (settings.unifyDamagedRegions())
            return DamagePropagation::Unified;
        return DamagePropagation::Region;
    })(webPage.corePage()->settings());

#if !HAVE(DISPLAY_LINK)
    m_display.displayID = displayID;
    m_display.displayUpdate = { 0, c_defaultRefreshRate / 1000 };
#endif

    m_compositingRunLoop->performTaskSync([this, protectedThis = Ref { *this }] {
#if !HAVE(DISPLAY_LINK)
        m_display.updateTimer = makeUnique<RunLoop::Timer>(RunLoop::current(), this, &ThreadedCompositor::displayUpdateFired);
#if USE(GLIB_EVENT_LOOP)
        m_display.updateTimer->setPriority(RunLoopSourcePriority::CompositingThreadUpdateTimer);
        m_display.updateTimer->setName("[WebKit] ThreadedCompositor::DisplayUpdate");
#endif
        m_display.updateTimer->startOneShot(Seconds { 1.0 / m_display.displayUpdate.updatesPerSecond });
#endif

        const auto propagateDamage = (m_damagePropagation == DamagePropagation::None)
            ? WebCore::Damage::ShouldPropagate::No : WebCore::Damage::ShouldPropagate::Yes;
        m_scene = adoptRef(new CoordinatedGraphicsScene(this, propagateDamage));

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
            m_scene->setActive(true);
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
    m_scene->detach();
    m_compositingRunLoop->stopUpdates();
#if HAVE(DISPLAY_LINK)
    m_didRenderFrameTimer.stop();
#else
    m_displayRefreshMonitor->invalidate();
#endif
    m_compositingRunLoop->performTaskSync([this, protectedThis = Ref { *this }] {
        if (!m_context || !m_context->makeContextCurrent())
            return;

        // Update the scene at this point ensures the layers state are correctly propagated
        // in the ThreadedCompositor and in the CompositingCoordinator.
        updateSceneWithoutRendering();

        m_scene->purgeGLResources();
        m_surface->willDestroyGLContext();
        m_context = nullptr;
        m_surface->finalize();
        m_scene = nullptr;

#if !HAVE(DISPLAY_LINK)
        m_display.updateTimer = nullptr;
#endif
    });
    m_layerTreeHost = nullptr;
    m_surface->willDestroyCompositingRunLoop();
    m_compositingRunLoop = nullptr;
}

void ThreadedCompositor::suspend()
{
    ASSERT(RunLoop::isMain());
    m_surface->visibilityDidChange(false);

    if (++m_suspendedCount > 1)
        return;

    m_compositingRunLoop->suspend();
    m_compositingRunLoop->performTaskSync([this, protectedThis = Ref { *this }] {
        m_scene->setActive(false);
    });
}

void ThreadedCompositor::resume()
{
    ASSERT(RunLoop::isMain());
    m_surface->visibilityDidChange(true);

    ASSERT(m_suspendedCount > 0);
    if (--m_suspendedCount > 0)
        return;

    m_compositingRunLoop->performTaskSync([this, protectedThis = Ref { *this }] {
        m_scene->setActive(true);
    });
    m_compositingRunLoop->resume();
}

void ThreadedCompositor::setViewportSize(const IntSize& size, float scaleFactor)
{
    ASSERT(RunLoop::isMain());
    m_surface->hostResize(size);

    Locker locker { m_attributes.lock };
    m_attributes.viewportSize = m_surface->size();
    m_attributes.scaleFactor = scaleFactor;
    m_attributes.needsResize = true;
    m_compositingRunLoop->scheduleUpdate();
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

void ThreadedCompositor::updateViewport()
{
    m_compositingRunLoop->scheduleUpdate();
}

const WebCore::Damage& ThreadedCompositor::addSurfaceDamage(const WebCore::Damage& damage)
{
    return m_surface->addDamage(damage);
}

void ThreadedCompositor::forceRepaint()
{
    // FIXME: Implement this once it's possible to do these forced updates
    // in a way that doesn't starve out the underlying graphics buffers.
}

void ThreadedCompositor::renderLayerTree()
{
    ASSERT(m_compositingRunLoop->isCurrent());
#if PLATFORM(GTK) || PLATFORM(WPE)
    TraceScope traceScope(RenderLayerTreeStart, RenderLayerTreeEnd);
#endif

    if (!m_scene || !m_scene->isActive())
        return;

    if (!m_context || !m_context->makeContextCurrent())
        return;

#if !HAVE(DISPLAY_LINK)
    m_display.updateTimer->stop();
#endif

    // Retrieve the scene attributes in a thread-safe manner.
    WebCore::IntSize viewportSize;
    float scaleFactor;
    bool needsResize;
    uint32_t compositionRequestID;

    Vector<RefPtr<Nicosia::Scene>> states;

    {
        Locker locker { m_attributes.lock };
        viewportSize = m_attributes.viewportSize;
        scaleFactor = m_attributes.scaleFactor;
        needsResize = m_attributes.needsResize;
        compositionRequestID = m_attributes.compositionRequestID;

        states = WTFMove(m_attributes.states);

        if (!states.isEmpty()) {
            // Client has to be notified upon finishing this scene update.
            m_attributes.clientRendersNextFrame = true;
        }

        // Reset the needsResize attribute to false.
        m_attributes.needsResize = false;
    }

    TransformationMatrix viewportTransform;
    viewportTransform.scale(scaleFactor);

    // Resize the client, if necessary, before the will-render-frame call is dispatched.
    // GL viewport is updated separately, if necessary. This establishes sequencing where
    // everything inside the will-render and did-render scope is done for a constant-sized scene,
    // and similarly all GL operations are done inside that specific scope.

    if (needsResize)
        m_surface->clientResize(viewportSize);

    m_surface->willRenderFrame();
    RunLoop::main().dispatch([this, protectedThis = Ref { *this }] {
        if (m_layerTreeHost)
            m_layerTreeHost->willRenderFrame();
    });

    if (needsResize)
        glViewport(0, 0, viewportSize.width(), viewportSize.height());

    m_surface->clearIfNeeded();
    WTFBeginSignpost(this, ApplyStateChanges);
    m_scene->applyStateChanges(states);
    WTFEndSignpost(this, ApplyStateChanges);

    WTFBeginSignpost(this, PaintToGLContext);
    m_scene->paintToCurrentGLContext(viewportTransform, FloatRect { FloatPoint { }, viewportSize }, m_damagePropagation == DamagePropagation::Unified, m_flipY);
    WTFEndSignpost(this, PaintToGLContext);

    WTFEmitSignpost(this, DidRenderFrame, "compositionResponseID %i", compositionRequestID);

    auto damageRegion = [&]() -> WebCore::Region {
        // FIXME: find a way to know if main frame scrolled since last frame to return early here.

        const auto& damage = m_scene->lastDamage();
        if (m_damagePropagation == DamagePropagation::None || damage.isInvalid())
            return { };

        WebCore::Damage boundsDamage;
        const auto& region = [&] -> WebCore::Region {
            if (m_damagePropagation == DamagePropagation::Unified) {
                boundsDamage.add(damage.bounds());
                if (boundsDamage.isInvalid() || boundsDamage.isEmpty())
                    return { };

                return boundsDamage.region();
            }
            if (damage.isEmpty())
                return { };

            return damage.region();
        }();

        if (region.isRect() && region.contains(IntRect({ }, viewportSize)))
            return { };

        return region;
    }();

    m_context->swapBuffers();

    m_surface->didRenderFrame(WTFMove(damageRegion));
#if HAVE(DISPLAY_LINK)
    m_compositionResponseID = compositionRequestID;
    if (!m_didRenderFrameTimer.isActive())
        m_didRenderFrameTimer.startOneShot(0_s);
#endif
    RunLoop::main().dispatch([this, protectedThis = Ref { *this }] {
        if (m_layerTreeHost)
            m_layerTreeHost->didRenderFrame();
    });
}

uint32_t ThreadedCompositor::requestComposition(const RefPtr<Nicosia::Scene>& state)
{
    ASSERT(RunLoop::isMain());
    uint32_t compositionRequestID;
    {
        Locker locker { m_attributes.lock };
        if (state)
            m_attributes.states.append(state);
        compositionRequestID = ++m_attributes.compositionRequestID;
    }
    m_compositingRunLoop->scheduleUpdate();
    return compositionRequestID;
}

void ThreadedCompositor::updateScene()
{
    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::updateSceneWithoutRendering()
{
    Vector<RefPtr<Nicosia::Scene>> states;

    {
        Locker locker { m_attributes.lock };
        states = WTFMove(m_attributes.states);

    }
    m_scene->applyStateChanges(states);
    m_scene->updateSceneState();
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

}
#endif // USE(COORDINATED_GRAPHICS)
