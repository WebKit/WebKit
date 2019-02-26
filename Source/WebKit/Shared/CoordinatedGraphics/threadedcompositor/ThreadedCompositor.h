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

#pragma once

#if USE(COORDINATED_GRAPHICS_THREADED)

#include "CompositingRunLoop.h"
#include "CoordinatedGraphicsScene.h"
#include <WebCore/CoordinatedGraphicsState.h>
#include <WebCore/GLContext.h>
#include <WebCore/IntSize.h>
#include <WebCore/TextureMapper.h>
#include <wtf/Atomics.h>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/ThreadSafeRefCounted.h>

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
#include "ThreadedDisplayRefreshMonitor.h"
#endif

namespace WebKit {

class CoordinatedGraphicsScene;
class CoordinatedGraphicsSceneClient;

class ThreadedCompositor : public CoordinatedGraphicsSceneClient, public ThreadSafeRefCounted<ThreadedCompositor> {
    WTF_MAKE_NONCOPYABLE(ThreadedCompositor);
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Client {
    public:
        virtual uint64_t nativeSurfaceHandleForCompositing() = 0;
        virtual void didDestroyGLContext() = 0;

        virtual void resize(const WebCore::IntSize&) = 0;
        virtual void willRenderFrame() = 0;
        virtual void didRenderFrame() = 0;
    };

    enum class ShouldDoFrameSync { No, Yes };

    static Ref<ThreadedCompositor> create(Client&, ThreadedDisplayRefreshMonitor::Client&, WebCore::PlatformDisplayID, const WebCore::IntSize&, float scaleFactor, ShouldDoFrameSync = ShouldDoFrameSync::Yes, WebCore::TextureMapper::PaintFlags = 0);
    virtual ~ThreadedCompositor();

    void setNativeSurfaceHandleForCompositing(uint64_t);
    void setScaleFactor(float);
    void setScrollPosition(const WebCore::IntPoint&, float scale);
    void setViewportSize(const WebCore::IntSize&, float scale);

    void updateSceneState(const WebCore::CoordinatedGraphicsState&);

    void invalidate();

    void forceRepaint();

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    RefPtr<WebCore::DisplayRefreshMonitor> displayRefreshMonitor(WebCore::PlatformDisplayID);
    void handleDisplayRefreshMonitorUpdate();
#endif

    void frameComplete();

private:
    ThreadedCompositor(Client&, ThreadedDisplayRefreshMonitor::Client&, WebCore::PlatformDisplayID, const WebCore::IntSize&, float scaleFactor, ShouldDoFrameSync, WebCore::TextureMapper::PaintFlags);

    // CoordinatedGraphicsSceneClient
    void updateViewport() override;

    void renderLayerTree();
    void sceneUpdateFinished();

    void createGLContext();

    Client& m_client;
    RefPtr<CoordinatedGraphicsScene> m_scene;
    std::unique_ptr<WebCore::GLContext> m_context;

    uint64_t m_nativeSurfaceHandle;
    ShouldDoFrameSync m_doFrameSync;
    WebCore::TextureMapper::PaintFlags m_paintFlags { 0 };
    bool m_inForceRepaint { false };

    std::unique_ptr<CompositingRunLoop> m_compositingRunLoop;

    struct {
        Lock lock;
        WebCore::IntSize viewportSize;
        WebCore::IntPoint scrollPosition;
        float scaleFactor { 1 };
        bool needsResize { false };
        Vector<WebCore::CoordinatedGraphicsState> states;

        bool clientRendersNextFrame { false };
        bool coordinateUpdateCompletionWithClient { false };
    } m_attributes;

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    Ref<ThreadedDisplayRefreshMonitor> m_displayRefreshMonitor;
#endif
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS_THREADED)

