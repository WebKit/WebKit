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

#ifndef ThreadedCompositor_h
#define ThreadedCompositor_h

#if USE(COORDINATED_GRAPHICS_THREADED)

#include "CompositingRunLoop.h"
#include "CoordinatedGraphicsScene.h"
#include <WebCore/GLContext.h>
#include <WebCore/IntSize.h>
#include <WebCore/TextureMapper.h>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
struct CoordinatedGraphicsState;
}

namespace WebKit {

class CoordinatedGraphicsScene;
class CoordinatedGraphicsSceneClient;

class ThreadedCompositor : public CoordinatedGraphicsSceneClient, public ThreadSafeRefCounted<ThreadedCompositor> {
    WTF_MAKE_NONCOPYABLE(ThreadedCompositor);
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Client {
    public:
        virtual void renderNextFrame() = 0;
        virtual void commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset) = 0;
    };

    enum class ShouldDoFrameSync { No, Yes };

    static Ref<ThreadedCompositor> create(Client&, const WebCore::IntSize&, float scaleFactor, uint64_t nativeSurfaceHandle = 0, ShouldDoFrameSync = ShouldDoFrameSync::Yes, WebCore::TextureMapper::PaintFlags = 0);
    virtual ~ThreadedCompositor();

    void setNativeSurfaceHandleForCompositing(uint64_t);
    void setScaleFactor(float);
    void setScrollPosition(const WebCore::IntPoint&, float scale);
    void setViewportSize(const WebCore::IntSize&, float scale);
    void setDrawsBackground(bool);

    void updateSceneState(const WebCore::CoordinatedGraphicsState&);

    void invalidate();

    void forceRepaint();

private:
    ThreadedCompositor(Client&, const WebCore::IntSize&, float scaleFactor, uint64_t nativeSurfaceHandle, ShouldDoFrameSync, WebCore::TextureMapper::PaintFlags);

    // CoordinatedGraphicsSceneClient
    void renderNextFrame() override;
    void updateViewport() override;
    void commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset) override;

    void renderLayerTree();
    void scheduleDisplayImmediately();

    void createGLContext();

    Client& m_client;
    RefPtr<CoordinatedGraphicsScene> m_scene;
    std::unique_ptr<WebCore::GLContext> m_context;

    WebCore::IntSize m_viewportSize;
    WebCore::IntPoint m_scrollPosition;
    float m_scaleFactor { 1 };
    bool m_drawsBackground { true };
    uint64_t m_nativeSurfaceHandle;
    ShouldDoFrameSync m_doFrameSync;
    WebCore::TextureMapper::PaintFlags m_paintFlags { 0 };
    bool m_needsResize { false };

    std::unique_ptr<CompositingRunLoop> m_compositingRunLoop;
};

} // namespace WebKit

#endif

#endif // ThreadedCompositor_h
