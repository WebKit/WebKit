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

#include "CompositingRunLoop.h"
#include <WebCore/PlatformDisplay.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/SetForScope.h>

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

#if HAVE(DISPLAY_LINK)
Ref<ThreadedCompositor> ThreadedCompositor::create(Client& client, PlatformDisplayID displayID, const IntSize& viewportSize, float scaleFactor, bool flipY)
{
    return adoptRef(*new ThreadedCompositor(client, displayID, viewportSize, scaleFactor, flipY));
}
#else
Ref<ThreadedCompositor> ThreadedCompositor::create(Client& client, ThreadedDisplayRefreshMonitor::Client& displayRefreshMonitorClient, PlatformDisplayID displayID, const IntSize& viewportSize, float scaleFactor, bool flipY)
{
    return adoptRef(*new ThreadedCompositor(client, displayRefreshMonitorClient, displayID, viewportSize, scaleFactor, flipY));
}
#endif

#if HAVE(DISPLAY_LINK)
ThreadedCompositor::ThreadedCompositor(Client& client, PlatformDisplayID displayID, const IntSize& viewportSize, float scaleFactor, bool flipY)
#else
ThreadedCompositor::ThreadedCompositor(Client& client, ThreadedDisplayRefreshMonitor::Client& displayRefreshMonitorClient, PlatformDisplayID displayID, const IntSize& viewportSize, float scaleFactor, bool flipY)
#endif
    : m_client(client)
    , m_flipY(flipY)
    , m_compositingRunLoop(makeUnique<CompositingRunLoop>([this] { renderLayerTree(); }))
#if !HAVE(DISPLAY_LINK)
    , m_displayRefreshMonitor(ThreadedDisplayRefreshMonitor::create(displayID, displayRefreshMonitorClient, WebCore::DisplayUpdate { 0, c_defaultRefreshRate / 1000 }))
#endif
{
    {
        // Locking isn't really necessary here, but it's done for consistency.
        Locker locker { m_attributes.lock };
        m_attributes.viewportSize = viewportSize;
        m_attributes.scaleFactor = scaleFactor;
        m_attributes.needsResize = !viewportSize.isEmpty();
    }

#if !HAVE(DISPLAY_LINK)
    m_display.displayID = displayID;
    m_display.displayUpdate = { 0, c_defaultRefreshRate / 1000 };
#else
    UNUSED_PARAM(displayID);
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

        m_scene = adoptRef(new CoordinatedGraphicsScene(this));
        m_nativeSurfaceHandle = m_client.nativeSurfaceHandleForCompositing();

        createGLContext();
        if (m_context) {
            if (!m_nativeSurfaceHandle)
                m_flipY = !m_flipY;
            m_scene->setActive(true);
        }
    });
}

ThreadedCompositor::~ThreadedCompositor()
{
}

void ThreadedCompositor::createGLContext()
{
    ASSERT(m_compositingRunLoop->isCurrent());

    // GLNativeWindowType depends on the EGL implementation: reinterpret_cast works
    // for pointers (only if they are 64-bit wide and not for other cases), and static_cast for
    // numeric types (and when needed they get extended to 64-bit) but not for pointers. Using
    // a plain C cast expression in this one instance works in all cases.
    static_assert(sizeof(GLNativeWindowType) <= sizeof(uint64_t), "GLNativeWindowType must not be longer than 64 bits.");
    auto windowType = (GLNativeWindowType) m_nativeSurfaceHandle;
    m_context = GLContext::create(windowType, PlatformDisplay::sharedDisplayForCompositing());
    if (m_context) {
        m_context->makeContextCurrent();
        m_client.didCreateGLContext();
    }
}

void ThreadedCompositor::invalidate()
{
    m_scene->detach();
    m_compositingRunLoop->stopUpdates();
#if !HAVE(DISPLAY_LINK)
    m_displayRefreshMonitor->invalidate();
#endif
    m_compositingRunLoop->performTaskSync([this, protectedThis = Ref { *this }] {
        if (!m_context || !m_context->makeContextCurrent())
            return;

        // Update the scene at this point ensures the layers state are correctly propagated
        // in the ThreadedCompositor and in the CompositingCoordinator.
        updateSceneWithoutRendering();

        m_scene->purgeGLResources();
        m_client.willDestroyGLContext();
        m_context = nullptr;
        m_client.didDestroyGLContext();
        m_scene = nullptr;

#if !HAVE(DISPLAY_LINK)
        m_display.updateTimer = nullptr;
#endif
    });
    m_compositingRunLoop = nullptr;
}

void ThreadedCompositor::suspend()
{
    if (++m_suspendedCount > 1)
        return;

    m_compositingRunLoop->suspend();
    m_compositingRunLoop->performTaskSync([this, protectedThis = Ref { *this }] {
        m_scene->setActive(false);
    });
}

void ThreadedCompositor::resume()
{
    ASSERT(m_suspendedCount > 0);
    if (--m_suspendedCount > 0)
        return;

    m_compositingRunLoop->performTaskSync([this, protectedThis = Ref { *this }] {
        m_scene->setActive(true);
    });
    m_compositingRunLoop->resume();
}

void ThreadedCompositor::setScrollPosition(const IntPoint& scrollPosition, float scale)
{
    Locker locker { m_attributes.lock };
    m_attributes.scrollPosition = scrollPosition;
    m_attributes.scaleFactor = scale;
    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::setViewportSize(const IntSize& viewportSize, float scale)
{
    Locker locker { m_attributes.lock };
    m_attributes.viewportSize = viewportSize;
    m_attributes.scaleFactor = scale;
    m_attributes.needsResize = true;
    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::updateViewport()
{
    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::forceRepaint()
{
    // FIXME: Implement this once it's possible to do these forced updates
    // in a way that doesn't starve out the underlying graphics buffers.
}

void ThreadedCompositor::renderLayerTree()
{
    if (!m_scene || !m_scene->isActive())
        return;

    if (!m_context || !m_context->makeContextCurrent())
        return;

#if !HAVE(DISPLAY_LINK)
    m_display.updateTimer->stop();
#endif

    // Retrieve the scene attributes in a thread-safe manner.
    WebCore::IntSize viewportSize;
    WebCore::IntPoint scrollPosition;
    float scaleFactor;
    bool needsResize;

    Vector<RefPtr<Nicosia::Scene>> states;

    {
        Locker locker { m_attributes.lock };
        viewportSize = m_attributes.viewportSize;
        scrollPosition = m_attributes.scrollPosition;
        scaleFactor = m_attributes.scaleFactor;
        needsResize = m_attributes.needsResize;

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
    viewportTransform.translate(-scrollPosition.x(), -scrollPosition.y());

    // Resize the client, if necessary, before the will-render-frame call is dispatched.
    // GL viewport is updated separately, if necessary. This establishes sequencing where
    // everything inside the will-render and did-render scope is done for a constant-sized scene,
    // and similarly all GL operations are done inside that specific scope.

    if (needsResize)
        m_client.resize(viewportSize);

    m_client.willRenderFrame();

    if (needsResize)
        glViewport(0, 0, viewportSize.width(), viewportSize.height());

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    m_scene->applyStateChanges(states);
    m_scene->paintToCurrentGLContext(viewportTransform, FloatRect { FloatPoint { }, viewportSize }, m_flipY);

    m_context->swapBuffers();

    if (m_scene->isActive())
        m_client.didRenderFrame();
}

void ThreadedCompositor::sceneUpdateFinished()
{
#if !HAVE(DISPLAY_LINK)
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
#endif

    Locker stateLocker { m_compositingRunLoop->stateLock() };

#if !HAVE(DISPLAY_LINK)
    // Schedule the DisplayRefreshMonitor callback, if necessary.
    if (shouldDispatchDisplayRefreshCallback)
        m_displayRefreshMonitor->dispatchDisplayRefreshCallback();
#endif

    // Mark the scene update as completed.
    m_compositingRunLoop->updateCompleted(stateLocker);
}

void ThreadedCompositor::updateSceneState(const RefPtr<Nicosia::Scene>& state)
{
    Locker locker { m_attributes.lock };
    m_attributes.states.append(state);
    m_compositingRunLoop->scheduleUpdate();
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

#if !HAVE(DISPLAY_LINK)
WebCore::DisplayRefreshMonitor& ThreadedCompositor::displayRefreshMonitor() const
{
    return m_displayRefreshMonitor.get();
}
#endif

void ThreadedCompositor::frameComplete()
{
    ASSERT(m_compositingRunLoop->isCurrent());
#if !HAVE(DISPLAY_LINK)
    displayUpdateFired();
#endif
    sceneUpdateFinished();
}

#if !HAVE(DISPLAY_LINK)
void ThreadedCompositor::displayUpdateFired()
{
    m_display.displayUpdate = m_display.displayUpdate.nextUpdate();

    m_client.displayDidRefresh(m_display.displayID);

    m_display.updateTimer->startOneShot(Seconds { 1.0 / m_display.displayUpdate.updatesPerSecond });
}
#endif

}
#endif // USE(COORDINATED_GRAPHICS)
