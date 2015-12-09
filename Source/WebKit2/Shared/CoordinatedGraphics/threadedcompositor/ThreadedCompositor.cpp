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

#if USE(COORDINATED_GRAPHICS_THREADED)
#include "ThreadedCompositor.h"

#include <WebCore/TransformationMatrix.h>
#include <wtf/CurrentTime.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>

#if USE(OPENGL_ES_2)
#include <GLES2/gl2.h>
#else
#include <GL/gl.h>
#endif

using namespace WebCore;

namespace WebKit {

class CompositingRunLoop {
    WTF_MAKE_NONCOPYABLE(CompositingRunLoop);
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum UpdateTiming {
        Immediate,
        WaitUntilNextFrame,
    };

    CompositingRunLoop(std::function<void()> updateFunction)
        : m_runLoop(RunLoop::current())
        , m_updateTimer(m_runLoop, this, &CompositingRunLoop::updateTimerFired)
        , m_updateFunction(WTF::move(updateFunction))
        , m_lastUpdateTime(0)
    {
    }

    void callOnCompositingRunLoop(std::function<void()> function)
    {
        if (&m_runLoop == &RunLoop::current()) {
            function();
            return;
        }

        m_runLoop.dispatch(WTF::move(function));
    }

    void setUpdateTimer(UpdateTiming timing = Immediate)
    {
        if (m_updateTimer.isActive())
            return;

        const static double targetFPS = 60;
        double nextUpdateTime = 0;
        if (timing == WaitUntilNextFrame)
            nextUpdateTime = std::max((1 / targetFPS) - (monotonicallyIncreasingTime() - m_lastUpdateTime), 0.0);

        m_updateTimer.startOneShot(nextUpdateTime);
    }

    void stopUpdateTimer()
    {
        if (m_updateTimer.isActive())
            m_updateTimer.stop();
    }

    RunLoop& runLoop()
    {
        return m_runLoop;
    }

private:

    void updateTimerFired()
    {
        m_updateFunction();
        m_lastUpdateTime = monotonicallyIncreasingTime();
    }

    RunLoop& m_runLoop;
    RunLoop::Timer<CompositingRunLoop> m_updateTimer;
    std::function<void()> m_updateFunction;

    double m_lastUpdateTime;
};

Ref<ThreadedCompositor> ThreadedCompositor::create(Client* client)
{
    return adoptRef(*new ThreadedCompositor(client));
}

ThreadedCompositor::ThreadedCompositor(Client* client)
    : m_client(client)
    , m_deviceScaleFactor(1)
    , m_threadIdentifier(0)
{
    createCompositingThread();
}

ThreadedCompositor::~ThreadedCompositor()
{
    terminateCompositingThread();
}

void ThreadedCompositor::setNeedsDisplay()
{
    RefPtr<ThreadedCompositor> protector(this);
    callOnCompositingThread([=] {
        protector->scheduleDisplayImmediately();
    });
}

void ThreadedCompositor::setNativeSurfaceHandleForCompositing(uint64_t handle)
{
    RefPtr<ThreadedCompositor> protector(this);
    callOnCompositingThread([=] {
        protector->m_nativeSurfaceHandle = handle;
        protector->m_scene->setActive(true);
    });
}

void ThreadedCompositor::setDeviceScaleFactor(float scale)
{
    RefPtr<ThreadedCompositor> protector(this);
    callOnCompositingThread([=] {
        protector->m_deviceScaleFactor = scale;
        protector->scheduleDisplayImmediately();
    });
}

void ThreadedCompositor::didChangeViewportSize(const IntSize& newSize)
{
    RefPtr<ThreadedCompositor> protector(this);
    callOnCompositingThread([=] {
        protector->viewportController()->didChangeViewportSize(newSize);
    });
}

void ThreadedCompositor::didChangeViewportAttribute(const ViewportAttributes& attr)
{
    RefPtr<ThreadedCompositor> protector(this);
    callOnCompositingThread([=] {
        protector->viewportController()->didChangeViewportAttribute(attr);
    });
}

void ThreadedCompositor::didChangeContentsSize(const IntSize& size)
{
    RefPtr<ThreadedCompositor> protector(this);
    callOnCompositingThread([=] {
        protector->viewportController()->didChangeContentsSize(size);
    });
}

void ThreadedCompositor::scrollTo(const IntPoint& position)
{
    RefPtr<ThreadedCompositor> protector(this);
    callOnCompositingThread([=] {
        protector->viewportController()->scrollTo(position);
    });
}

void ThreadedCompositor::scrollBy(const IntSize& delta)
{
    RefPtr<ThreadedCompositor> protector(this);
    callOnCompositingThread([=] {
        protector->viewportController()->scrollBy(delta);
    });
}

void ThreadedCompositor::purgeBackingStores()
{
    m_client->purgeBackingStores();
}

void ThreadedCompositor::renderNextFrame()
{
    m_client->renderNextFrame();
}

void ThreadedCompositor::updateViewport()
{
    m_compositingRunLoop->setUpdateTimer(CompositingRunLoop::WaitUntilNextFrame);
}

void ThreadedCompositor::commitScrollOffset(uint32_t layerID, const IntSize& offset)
{
    m_client->commitScrollOffset(layerID, offset);
}

bool ThreadedCompositor::ensureGLContext()
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
        return 0;

    m_context = GLContext::createContextForWindow(reinterpret_cast<GLNativeWindowType>(m_nativeSurfaceHandle), GLContext::sharingContext());
    return m_context.get();
}

void ThreadedCompositor::scheduleDisplayImmediately()
{
    m_compositingRunLoop->setUpdateTimer(CompositingRunLoop::Immediate);
}

void ThreadedCompositor::didChangeVisibleRect()
{
    FloatRect visibleRect = viewportController()->visibleContentsRect();
    float scale = viewportController()->pageScaleFactor();
    callOnMainThread([=] {
        m_client->setVisibleContentsRect(visibleRect, FloatPoint::zero(), scale);
    });

    scheduleDisplayImmediately();
}

void ThreadedCompositor::renderLayerTree()
{
    if (!m_scene)
        return;

    if (!ensureGLContext())
        return;

    FloatRect clipRect(0, 0, m_viewportSize.width(), m_viewportSize.height());

    TransformationMatrix viewportTransform;
    FloatPoint scrollPostion = viewportController()->visibleContentsRect().location();
    viewportTransform.scale(viewportController()->pageScaleFactor() * m_deviceScaleFactor);
    viewportTransform.translate(-scrollPostion.x(), -scrollPostion.y());

    m_scene->paintToCurrentGLContext(viewportTransform, 1, clipRect, Color::white, false, scrollPostion);

    glContext()->swapBuffers();
}

void ThreadedCompositor::updateSceneState(const CoordinatedGraphicsState& state)
{
    RefPtr<CoordinatedGraphicsScene> scene = m_scene;
    m_scene->appendUpdate([scene, state] {
        scene->commitSceneState(state);
    });

    setNeedsDisplay();
}

void ThreadedCompositor::callOnCompositingThread(std::function<void()> function)
{
    m_compositingRunLoop->callOnCompositingRunLoop(WTF::move(function));
}

void ThreadedCompositor::compositingThreadEntry(void* coordinatedCompositor)
{
    static_cast<ThreadedCompositor*>(coordinatedCompositor)->runCompositingThread();
}

void ThreadedCompositor::createCompositingThread()
{
    if (m_threadIdentifier)
        return;

    LockHolder locker(m_initializeRunLoopConditionMutex);
    m_threadIdentifier = createThread(compositingThreadEntry, this, "WebCore: ThreadedCompositor");

    m_initializeRunLoopCondition.wait(m_initializeRunLoopConditionMutex);
}

void ThreadedCompositor::runCompositingThread()
{
    {
        LockHolder locker(m_initializeRunLoopConditionMutex);

        m_compositingRunLoop = std::make_unique<CompositingRunLoop>([&] {
            renderLayerTree();
        });
        m_scene = adoptRef(new CoordinatedGraphicsScene(this));
        m_viewportController = std::make_unique<SimpleViewportController>(this);

        m_initializeRunLoopCondition.notifyOne();
    }

    m_compositingRunLoop->runLoop().run();

    m_compositingRunLoop->stopUpdateTimer();
    m_scene->purgeGLResources();

    {
        LockHolder locker(m_terminateRunLoopConditionMutex);
        m_compositingRunLoop = nullptr;
        m_context = nullptr;
        m_terminateRunLoopCondition.notifyOne();
    }

    detachThread(m_threadIdentifier);
}

void ThreadedCompositor::terminateCompositingThread()
{
    LockHolder locker(m_terminateRunLoopConditionMutex);

    m_scene->detach();
    m_compositingRunLoop->runLoop().stop();

    m_terminateRunLoopCondition.wait(m_terminateRunLoopConditionMutex);
}

}
#endif // USE(COORDINATED_GRAPHICS_THREADED)
