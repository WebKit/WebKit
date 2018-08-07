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

#if USE(COORDINATED_GRAPHICS_THREADED)

#include "CompositingRunLoop.h"
#include "ThreadedDisplayRefreshMonitor.h"
#include <WebCore/PlatformDisplay.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/SetForScope.h>

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#elif USE(OPENGL_ES_2)
#include <GLES2/gl2.h>
#else
#include <GL/gl.h>
#endif

namespace WebKit {
using namespace WebCore;

Ref<ThreadedCompositor> ThreadedCompositor::create(Client& client, WebPage& webPage, const IntSize& viewportSize, float scaleFactor, ShouldDoFrameSync doFrameSync, TextureMapper::PaintFlags paintFlags)
{
    return adoptRef(*new ThreadedCompositor(client, webPage, viewportSize, scaleFactor, doFrameSync, paintFlags));
}

ThreadedCompositor::ThreadedCompositor(Client& client, WebPage& webPage, const IntSize& viewportSize, float scaleFactor, ShouldDoFrameSync doFrameSync, TextureMapper::PaintFlags paintFlags)
    : m_client(client)
    , m_doFrameSync(doFrameSync)
    , m_paintFlags(paintFlags)
    , m_compositingRunLoop(std::make_unique<CompositingRunLoop>([this] { renderLayerTree(); }))
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    , m_displayRefreshMonitor(ThreadedDisplayRefreshMonitor::create(*this))
#endif
{
    {
        // Locking isn't really necessary here, but it's done for consistency.
        LockHolder locker(m_attributes.lock);
        m_attributes.viewportSize = viewportSize;
        m_attributes.scaleFactor = scaleFactor;
        m_attributes.needsResize = !viewportSize.isEmpty();
    }

    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this)] {
        m_scene = adoptRef(new CoordinatedGraphicsScene(this));
        m_nativeSurfaceHandle = m_client.nativeSurfaceHandleForCompositing();
        if (m_nativeSurfaceHandle) {
            createGLContext();
            m_scene->setActive(true);
        } else
            m_scene->setActive(false);
    });
}

ThreadedCompositor::~ThreadedCompositor()
{
}

void ThreadedCompositor::createGLContext()
{
    ASSERT(!RunLoop::isMain());

    ASSERT(m_nativeSurfaceHandle);

    m_context = GLContext::createContextForWindow(reinterpret_cast<GLNativeWindowType>(m_nativeSurfaceHandle), &PlatformDisplay::sharedDisplayForCompositing());
    if (!m_context)
        return;

    if (!m_context->makeContextCurrent())
        return;

    if (m_doFrameSync == ShouldDoFrameSync::No)
        m_context->swapInterval(0);
}

void ThreadedCompositor::invalidate()
{
    m_scene->detach();
    m_compositingRunLoop->stopUpdates();
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    m_displayRefreshMonitor->invalidate();
#endif
    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this)] {
        if (!m_context || !m_context->makeContextCurrent())
            return;
        m_scene->purgeGLResources();
        m_context = nullptr;
        m_client.didDestroyGLContext();
        m_scene = nullptr;
    });
    m_compositingRunLoop = nullptr;
}

void ThreadedCompositor::setNativeSurfaceHandleForCompositing(uint64_t handle)
{
    m_compositingRunLoop->stopUpdates();
    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this), handle] {
        // A new native handle can't be set without destroying the previous one first if any.
        ASSERT(!!handle ^ !!m_nativeSurfaceHandle);
        m_nativeSurfaceHandle = handle;
        if (m_nativeSurfaceHandle) {
            createGLContext();
            m_scene->setActive(true);
        } else {
            m_scene->setActive(false);
            m_context = nullptr;
        }
    });
}

void ThreadedCompositor::setScaleFactor(float scale)
{
    LockHolder locker(m_attributes.lock);
    m_attributes.scaleFactor = scale;
    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::setScrollPosition(const IntPoint& scrollPosition, float scale)
{
    LockHolder locker(m_attributes.lock);
    m_attributes.scrollPosition = scrollPosition;
    m_attributes.scaleFactor = scale;
    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::setViewportSize(const IntSize& viewportSize, float scale)
{
    LockHolder locker(m_attributes.lock);
    m_attributes.viewportSize = viewportSize;
    m_attributes.scaleFactor = scale;
    m_attributes.needsResize = true;
    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::setDrawsBackground(bool drawsBackground)
{
    LockHolder locker(m_attributes.lock);
    m_attributes.drawsBackground = drawsBackground;
    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::renderNextFrame()
{
    ASSERT(RunLoop::isMain());
    m_client.renderNextFrame();
}

void ThreadedCompositor::commitScrollOffset(uint32_t layerID, const IntSize& offset)
{
    ASSERT(RunLoop::isMain());
    m_client.commitScrollOffset(layerID, offset);
}

void ThreadedCompositor::updateViewport()
{
    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::forceRepaint()
{
    // FIXME: Enable this for WPE once it's possible to do these forced updates
    // in a way that doesn't starve out the underlying graphics buffers.
#if PLATFORM(GTK)
    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this)] {
        SetForScope<bool> change(m_inForceRepaint, true);
        renderLayerTree();
    });
#endif
}

void ThreadedCompositor::renderLayerTree()
{
    if (!m_scene || !m_scene->isActive())
        return;

    if (!m_context || !m_context->makeContextCurrent())
        return;

    m_client.willRenderFrame();

    // Retrieve the scene attributes in a thread-safe manner.
    WebCore::IntSize viewportSize;
    WebCore::IntPoint scrollPosition;
    float scaleFactor;
    bool drawsBackground;
    bool needsResize;
    Vector<WebCore::CoordinatedGraphicsState> states;
    Vector<uint32_t> atlasesToRemove;

    {
        LockHolder locker(m_attributes.lock);
        viewportSize = m_attributes.viewportSize;
        scrollPosition = m_attributes.scrollPosition;
        scaleFactor = m_attributes.scaleFactor;
        drawsBackground = m_attributes.drawsBackground;
        needsResize = m_attributes.needsResize;

        states = WTFMove(m_attributes.states);
        atlasesToRemove = WTFMove(m_attributes.atlasesToRemove);

        if (!states.isEmpty()) {
            // Client has to be notified upon finishing this scene update.
            m_attributes.clientRendersNextFrame = true;

            // Coordinate scene update completion with the client in case of changed or updated platform layers.
            // But do not change coordinateUpdateCompletionWithClient while in force repaint because that
            // demands immediate scene update completion regardless of platform layers.
            if (!m_inForceRepaint) {
                bool coordinateUpdate = false;
                for (auto& state : states)
                    coordinateUpdate |= std::any_of(state.layersToUpdate.begin(), state.layersToUpdate.end(),
                        [](auto& it) { return it.second.platformLayerChanged || it.second.platformLayerUpdated; });
                m_attributes.coordinateUpdateCompletionWithClient = coordinateUpdate;
            }
        }

        // Reset the needsResize attribute to false.
        m_attributes.needsResize = false;
    }

    if (needsResize)
        glViewport(0, 0, viewportSize.width(), viewportSize.height());

    TransformationMatrix viewportTransform;
    viewportTransform.scale(scaleFactor);
    viewportTransform.translate(-scrollPosition.x(), -scrollPosition.y());

    if (!drawsBackground) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    m_scene->applyStateChanges(states);
    m_scene->releaseUpdateAtlases(atlasesToRemove);
    m_scene->paintToCurrentGLContext(viewportTransform, 1, FloatRect { FloatPoint { }, viewportSize },
        Color::transparent, !drawsBackground, scrollPosition, m_paintFlags);

    m_context->swapBuffers();

    if (m_scene->isActive())
        m_client.didRenderFrame();
}

void ThreadedCompositor::sceneUpdateFinished()
{
    // The composition has finished. Now we have to determine how to manage
    // the scene update completion.

    // The DisplayRefreshMonitor will be used to dispatch a callback on the client thread if:
    //  - clientRendersNextFrame is true (i.e. client has to be notified about the finished update), or
    //  - a DisplayRefreshMonitor callback was requested from the Web engine
    bool shouldDispatchDisplayRefreshCallback { false };

    // If coordinateUpdateCompletionWithClient is true, the scene update completion has to be
    // delayed until the DisplayRefreshMonitor callback.
    bool shouldCoordinateUpdateCompletionWithClient { false };

    {
        LockHolder locker(m_attributes.lock);
        shouldDispatchDisplayRefreshCallback = m_attributes.clientRendersNextFrame
            || m_displayRefreshMonitor->requiresDisplayRefreshCallback();
        shouldCoordinateUpdateCompletionWithClient = m_attributes.coordinateUpdateCompletionWithClient;
    }

    LockHolder stateLocker(m_compositingRunLoop->stateLock());

    // Schedule the DisplayRefreshMonitor callback, if necessary.
    if (shouldDispatchDisplayRefreshCallback)
        m_displayRefreshMonitor->dispatchDisplayRefreshCallback();

    // Mark the scene update as completed if no coordination is required and if not in a forced repaint.
    if (!shouldCoordinateUpdateCompletionWithClient && !m_inForceRepaint)
        m_compositingRunLoop->updateCompleted(stateLocker);

    // Independent of the scene update, the composition itself is now completed.
    m_compositingRunLoop->compositionCompleted(stateLocker);
}

void ThreadedCompositor::updateSceneState(const CoordinatedGraphicsState& state)
{
    LockHolder locker(m_attributes.lock);
    m_attributes.states.append(state);
    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::releaseUpdateAtlases(const Vector<uint32_t>& atlasesToRemove)
{
    LockHolder locker(m_attributes.lock);
    m_attributes.atlasesToRemove.appendVector(atlasesToRemove);
    m_compositingRunLoop->scheduleUpdate();
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
RefPtr<WebCore::DisplayRefreshMonitor> ThreadedCompositor::displayRefreshMonitor(PlatformDisplayID)
{
    return m_displayRefreshMonitor.copyRef();
}

void ThreadedCompositor::requestDisplayRefreshMonitorUpdate()
{
    // This is invoked by ThreadedDisplayRefreshMonitor when a fresh update is required.

    LockHolder stateLocker(m_compositingRunLoop->stateLock());
    {
        // coordinateUpdateCompletionWithClient is set to true in order to delay the scene update
        // completion until the DisplayRefreshMonitor is fired on the main thread after the composition
        // is completed.
        LockHolder locker(m_attributes.lock);
        m_attributes.coordinateUpdateCompletionWithClient = true;
    }
    m_compositingRunLoop->scheduleUpdate(stateLocker);
}

void ThreadedCompositor::handleDisplayRefreshMonitorUpdate(bool hasBeenRescheduled)
{
    // Retrieve the clientRendersNextFrame and coordinateUpdateCompletionWithClient.
    bool clientRendersNextFrame { false };
    bool coordinateUpdateCompletionWithClient { false };
    {
        LockHolder locker(m_attributes.lock);
        clientRendersNextFrame = std::exchange(m_attributes.clientRendersNextFrame, false);
        coordinateUpdateCompletionWithClient = std::exchange(m_attributes.coordinateUpdateCompletionWithClient, false);
    }

    // If clientRendersNextFrame is true, the client is finally notified about the scene update nearing
    // completion. The client will use this opportunity to clean up resources as appropriate. It can also
    // perform any layer flush that was requested during the composition, or by any DisplayRefreshMonitor
    // notifications that have been handled at this point.
    if (clientRendersNextFrame)
        m_client.renderNextFrame();

    LockHolder stateLocker(m_compositingRunLoop->stateLock());

    // If required, mark the current scene update as completed. CompositingRunLoop will take care of
    // scheduling a new update in case an update was marked as pending due to previous layer flushes
    // or DisplayRefreshMonitor notifications.
    if (coordinateUpdateCompletionWithClient)
        m_compositingRunLoop->updateCompleted(stateLocker);

    // If the DisplayRefreshMonitor was scheduled again, we immediately demand the update completion
    // coordination (like we do in requestDisplayRefreshMonitorUpdate()) and request an update.
    if (hasBeenRescheduled) {
        {
            LockHolder locker(m_attributes.lock);
            m_attributes.coordinateUpdateCompletionWithClient = true;
        }
        m_compositingRunLoop->scheduleUpdate(stateLocker);
    }
}
#endif

void ThreadedCompositor::frameComplete()
{
    ASSERT(!RunLoop::isMain());
    sceneUpdateFinished();
}

}
#endif // USE(COORDINATED_GRAPHICS_THREADED)
