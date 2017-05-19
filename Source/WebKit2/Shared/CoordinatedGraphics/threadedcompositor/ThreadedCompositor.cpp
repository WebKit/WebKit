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

#if USE(OPENGL_ES_2)
#include <GLES2/gl2.h>
#else
#include <GL/gl.h>
#endif

using namespace WebCore;

namespace WebKit {

Ref<ThreadedCompositor> ThreadedCompositor::create(Client& client, WebPage& webPage, const IntSize& viewportSize, float scaleFactor, ShouldDoFrameSync doFrameSync, TextureMapper::PaintFlags paintFlags)
{
    return adoptRef(*new ThreadedCompositor(client, webPage, viewportSize, scaleFactor, doFrameSync, paintFlags));
}

ThreadedCompositor::ThreadedCompositor(Client& client, WebPage& webPage, const IntSize& viewportSize, float scaleFactor, ShouldDoFrameSync doFrameSync, TextureMapper::PaintFlags paintFlags)
    : m_client(client)
    , m_viewportSize(viewportSize)
    , m_scaleFactor(scaleFactor)
    , m_doFrameSync(doFrameSync)
    , m_paintFlags(paintFlags)
    , m_needsResize(!viewportSize.isEmpty())
    , m_compositingRunLoop(std::make_unique<CompositingRunLoop>([this] { renderLayerTree(); }))
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    , m_displayRefreshMonitor(ThreadedDisplayRefreshMonitor::create(*this))
#endif
{
    m_clientRendersNextFrame.store(false);
    m_coordinateUpdateCompletionWithClient.store(false);

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
    ASSERT(!isMainThread());

    ASSERT(m_nativeSurfaceHandle);

    m_context = GLContext::createContextForWindow(reinterpret_cast<GLNativeWindowType>(m_nativeSurfaceHandle), &PlatformDisplay::sharedDisplayForCompositing());
    if (!m_context)
        return;

    if (m_doFrameSync == ShouldDoFrameSync::No) {
        if (m_context->makeContextCurrent())
            m_context->swapInterval(0);
    }
}

void ThreadedCompositor::invalidate()
{
    m_scene->detach();
    m_compositingRunLoop->stopUpdates();
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    m_displayRefreshMonitor->invalidate();
#endif
    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this)] {
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
    m_compositingRunLoop->performTask([this, protectedThis = makeRef(*this), scale] {
        m_scaleFactor = scale;
        m_compositingRunLoop->scheduleUpdate();
    });
}

void ThreadedCompositor::setScrollPosition(const IntPoint& scrollPosition, float scale)
{
    m_compositingRunLoop->performTask([this, protectedThis = makeRef(*this), scrollPosition, scale] {
        m_scrollPosition = scrollPosition;
        m_scaleFactor = scale;
        m_compositingRunLoop->scheduleUpdate();
    });
}

void ThreadedCompositor::setViewportSize(const IntSize& viewportSize, float scale)
{
    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this), viewportSize, scale] {
        m_viewportSize = viewportSize;
        m_scaleFactor = scale;
        m_needsResize = true;
        m_compositingRunLoop->scheduleUpdate();
    });
}

void ThreadedCompositor::setDrawsBackground(bool drawsBackground)
{
    m_compositingRunLoop->performTask([this, protectedThis = Ref<ThreadedCompositor>(*this), drawsBackground] {
        m_drawsBackground = drawsBackground;
        m_compositingRunLoop->scheduleUpdate();
    });
}

void ThreadedCompositor::renderNextFrame()
{
    ASSERT(isMainThread());
    m_client.renderNextFrame();
}

void ThreadedCompositor::commitScrollOffset(uint32_t layerID, const IntSize& offset)
{
    ASSERT(isMainThread());
    m_client.commitScrollOffset(layerID, offset);
}

void ThreadedCompositor::updateViewport()
{
    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::forceRepaint()
{
    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this)] {
        SetForScope<bool> change(m_inForceRepaint, true);
        renderLayerTree();
    });
}

void ThreadedCompositor::renderLayerTree()
{
    if (!m_scene || !m_scene->isActive())
        return;

    if (!m_context || !m_context->makeContextCurrent())
        return;

    m_client.willRenderFrame();

    if (m_needsResize) {
        glViewport(0, 0, m_viewportSize.width(), m_viewportSize.height());
        m_needsResize = false;
    }
    FloatRect clipRect(0, 0, m_viewportSize.width(), m_viewportSize.height());

    TransformationMatrix viewportTransform;
    viewportTransform.scale(m_scaleFactor);
    viewportTransform.translate(-m_scrollPosition.x(), -m_scrollPosition.y());

    if (!m_drawsBackground) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    m_scene->paintToCurrentGLContext(viewportTransform, 1, clipRect, Color::transparent, !m_drawsBackground, m_scrollPosition, m_paintFlags);

    m_context->swapBuffers();

    if (m_scene->isActive())
        m_client.didRenderFrame();
}

void ThreadedCompositor::sceneUpdateFinished()
{
    bool shouldDispatchDisplayRefreshCallback = m_clientRendersNextFrame.load()
        || m_displayRefreshMonitor->requiresDisplayRefreshCallback();
    bool shouldCoordinateUpdateCompletionWithClient = m_coordinateUpdateCompletionWithClient.load();

    if (shouldDispatchDisplayRefreshCallback)
        m_displayRefreshMonitor->dispatchDisplayRefreshCallback();
    if (!shouldCoordinateUpdateCompletionWithClient && !m_inForceRepaint)
        m_compositingRunLoop->updateCompleted();
}

void ThreadedCompositor::updateSceneState(const CoordinatedGraphicsState& state)
{
    ASSERT(isMainThread());
    RefPtr<CoordinatedGraphicsScene> scene = m_scene;
    m_scene->appendUpdate([this, scene, state] {
        scene->commitSceneState(state);

        m_clientRendersNextFrame.store(true);
        // Do not change m_coordinateUpdateCompletionWithClient while in force repaint.
        if (m_inForceRepaint)
            return;
        bool coordinateUpdate = std::any_of(state.layersToUpdate.begin(), state.layersToUpdate.end(),
            [](const std::pair<CoordinatedLayerID, CoordinatedGraphicsLayerState>& it) {
                return it.second.platformLayerChanged || it.second.platformLayerUpdated;
            });
        m_coordinateUpdateCompletionWithClient.store(coordinateUpdate);
    });

    m_compositingRunLoop->scheduleUpdate();
}

void ThreadedCompositor::releaseUpdateAtlases(Vector<uint32_t>&& atlasesToRemove)
{
    ASSERT(isMainThread());
    m_compositingRunLoop->performTask([scene = makeRef(*m_scene), atlasesToRemove = WTFMove(atlasesToRemove)] {
        scene->releaseUpdateAtlases(atlasesToRemove);
    });
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
RefPtr<WebCore::DisplayRefreshMonitor> ThreadedCompositor::displayRefreshMonitor(PlatformDisplayID)
{
    return m_displayRefreshMonitor.copyRef();
}

void ThreadedCompositor::renderNextFrameIfNeeded()
{
    if (m_clientRendersNextFrame.compareExchangeStrong(true, false))
        m_scene->renderNextFrame();
}

void ThreadedCompositor::completeCoordinatedUpdateIfNeeded()
{
    if (m_coordinateUpdateCompletionWithClient.compareExchangeStrong(true, false))
        m_compositingRunLoop->updateCompleted();
}

void ThreadedCompositor::coordinateUpdateCompletionWithClient()
{
    m_coordinateUpdateCompletionWithClient.store(true);
    if (!m_compositingRunLoop->isActive())
        m_compositingRunLoop->scheduleUpdate();
}
#endif

void ThreadedCompositor::frameComplete()
{
    ASSERT(!isMainThread());
    sceneUpdateFinished();
}

}
#endif // USE(COORDINATED_GRAPHICS_THREADED)
