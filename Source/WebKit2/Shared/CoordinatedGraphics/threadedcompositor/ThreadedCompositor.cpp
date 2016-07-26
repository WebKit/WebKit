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
#include <WebCore/TransformationMatrix.h>

#if USE(OPENGL_ES_2)
#include <GLES2/gl2.h>
#else
#include <GL/gl.h>
#endif

using namespace WebCore;

namespace WebKit {

Ref<ThreadedCompositor> ThreadedCompositor::create(Client* client)
{
    return adoptRef(*new ThreadedCompositor(client));
}

ThreadedCompositor::ThreadedCompositor(Client* client)
    : m_client(client)
    , m_compositingRunLoop(std::make_unique<CompositingRunLoop>([this] { renderLayerTree(); }))
{
    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this)] {
        m_scene = adoptRef(new CoordinatedGraphicsScene(this));
        m_viewportController = std::make_unique<SimpleViewportController>(this);
    });
}

ThreadedCompositor::~ThreadedCompositor()
{
    ASSERT(!m_client);
}

void ThreadedCompositor::invalidate()
{
    m_scene->detach();
    m_compositingRunLoop->stopUpdateTimer();
    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this)] {
        m_scene->purgeGLResources();
        m_context = nullptr;
        m_scene = nullptr;
        m_viewportController = nullptr;
    });
    m_compositingRunLoop = nullptr;
    m_client = nullptr;
}

void ThreadedCompositor::setNativeSurfaceHandleForCompositing(uint64_t handle)
{
    m_compositingRunLoop->stopUpdateTimer();
    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this), handle] {
        m_scene->setActive(!!handle);

        // A new native handle can't be set without destroying the previous one first if any.
        ASSERT(!!handle ^ !!m_nativeSurfaceHandle);
        m_nativeSurfaceHandle = handle;
        if (!m_nativeSurfaceHandle)
            m_context = nullptr;
    });
}

void ThreadedCompositor::setDeviceScaleFactor(float scale)
{
    m_compositingRunLoop->performTask([this, protectedThis = makeRef(*this), scale] {
        m_deviceScaleFactor = scale;
        scheduleDisplayImmediately();
    });
}

void ThreadedCompositor::setDrawsBackground(bool drawsBackground)
{
    m_compositingRunLoop->performTask([this, protectedThis = Ref<ThreadedCompositor>(*this), drawsBackground] {
        m_drawsBackground = drawsBackground;
        scheduleDisplayImmediately();
    });
}

void ThreadedCompositor::didChangeViewportSize(const IntSize& size)
{
    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this), size] {
        m_viewportController->didChangeViewportSize(size);
    });
}

void ThreadedCompositor::didChangeViewportAttribute(const ViewportAttributes& attr)
{
    m_compositingRunLoop->performTask([this, protectedThis = makeRef(*this), attr] {
        m_viewportController->didChangeViewportAttribute(attr);
    });
}

void ThreadedCompositor::didChangeContentsSize(const IntSize& size)
{
    m_compositingRunLoop->performTask([this, protectedThis = makeRef(*this), size] {
        m_viewportController->didChangeContentsSize(size);
    });
}

void ThreadedCompositor::scrollTo(const IntPoint& position)
{
    m_compositingRunLoop->performTask([this, protectedThis = makeRef(*this), position] {
        m_viewportController->scrollTo(position);
    });
}

void ThreadedCompositor::scrollBy(const IntSize& delta)
{
    m_compositingRunLoop->performTask([this, protectedThis = makeRef(*this), delta] {
        m_viewportController->scrollBy(delta);
    });
}

void ThreadedCompositor::purgeBackingStores()
{
    ASSERT(isMainThread());
    m_client->purgeBackingStores();
}

void ThreadedCompositor::renderNextFrame()
{
    ASSERT(isMainThread());
    m_client->renderNextFrame();
}

void ThreadedCompositor::commitScrollOffset(uint32_t layerID, const IntSize& offset)
{
    ASSERT(isMainThread());
    m_client->commitScrollOffset(layerID, offset);
}

void ThreadedCompositor::updateViewport()
{
    m_compositingRunLoop->startUpdateTimer(CompositingRunLoop::WaitUntilNextFrame);
}

void ThreadedCompositor::scheduleDisplayImmediately()
{
    m_compositingRunLoop->startUpdateTimer(CompositingRunLoop::Immediate);
}

bool ThreadedCompositor::tryEnsureGLContext()
{
    if (!glContext())
        return false;

    glContext()->makeContextCurrent();
    // The window size may be out of sync with the page size at this point, and getting
    // the viewport parameters incorrect, means that the content will be misplaced. Thus
    // we set the viewport parameters directly from the window size.
    IntSize contextSize = glContext()->defaultFrameBufferSize();
    if (m_viewportSize != contextSize) {
        glViewport(0, 0, contextSize.width(), contextSize.height());
        m_viewportSize = contextSize;
    }

    return true;
}

GLContext* ThreadedCompositor::glContext()
{
    if (m_context)
        return m_context.get();

    if (!m_nativeSurfaceHandle)
        return nullptr;

    m_context = GLContext::createContextForWindow(reinterpret_cast<GLNativeWindowType>(m_nativeSurfaceHandle), GLContext::sharingContext());
    return m_context.get();
}

void ThreadedCompositor::forceRepaint()
{
    m_compositingRunLoop->performTaskSync([this, protectedThis = makeRef(*this)] {
        renderLayerTree();
    });
}

void ThreadedCompositor::didChangeVisibleRect()
{
    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), visibleRect = m_viewportController->visibleContentsRect(), scale = m_viewportController->pageScaleFactor()] {
        if (m_client)
            m_client->setVisibleContentsRect(visibleRect, FloatPoint::zero(), scale);
    });

    scheduleDisplayImmediately();
}

void ThreadedCompositor::renderLayerTree()
{
    if (!m_scene || !m_scene->isActive())
        return;

    if (!tryEnsureGLContext())
        return;

    FloatRect clipRect(0, 0, m_viewportSize.width(), m_viewportSize.height());

    TransformationMatrix viewportTransform;
    FloatPoint scrollPostion = m_viewportController->visibleContentsRect().location();
    viewportTransform.scale(m_viewportController->pageScaleFactor() * m_deviceScaleFactor);
    viewportTransform.translate(-scrollPostion.x(), -scrollPostion.y());

    if (!m_drawsBackground) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    m_scene->paintToCurrentGLContext(viewportTransform, 1, clipRect, Color::transparent, !m_drawsBackground, scrollPostion);

    glContext()->swapBuffers();
}

void ThreadedCompositor::updateSceneState(const CoordinatedGraphicsState& state)
{
    ASSERT(isMainThread());
    RefPtr<CoordinatedGraphicsScene> scene = m_scene;
    m_scene->appendUpdate([scene, state] {
        scene->commitSceneState(state);
    });

    scheduleDisplayImmediately();
}

}
#endif // USE(COORDINATED_GRAPHICS_THREADED)
