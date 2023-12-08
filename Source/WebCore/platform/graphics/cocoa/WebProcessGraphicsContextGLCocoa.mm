
/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if ENABLE(WEBGL)
#import "GraphicsContextGLCocoa.h" // NOLINT
#import "GraphicsLayerContentsDisplayDelegate.h"
#import "PlatformCALayer.h"
#import "PlatformCALayerDelegatedContents.h"
#import "ProcessIdentity.h"
#import <wtf/Condition.h>
#import <wtf/FastMalloc.h>
#import <wtf/Lock.h>
#import <wtf/Noncopyable.h>

#if PLATFORM(MAC)
#import "DisplayConfigurationMonitor.h"
#endif

namespace WebCore {

constexpr Seconds frameFinishedTimeout = 5_s;

namespace {

class DisplayBufferFence final : public PlatformCALayerDelegatedContentsFence {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DisplayBufferFence);
public:
    static RefPtr<DisplayBufferFence> create()
    {
        return adoptRef(new DisplayBufferFence);
    }

    bool waitFor(Seconds timeout) final
    {
        Locker locker { m_lock };
        auto absoluteTime = MonotonicTime::timePointFromNow(timeout);
        return m_condition.waitUntil(m_lock, absoluteTime, [&] {
            assertIsHeld(m_lock);
            return m_isSet;
        });
    }

    void signalAll()
    {
        Locker locker { m_lock };
        if (m_isSet)
            return;
        m_isSet = true;
        m_condition.notifyAll();
    }

private:
    DisplayBufferFence() = default;
    Lock m_lock;
    bool m_isSet WTF_GUARDED_BY_LOCK(m_lock) { false };
    Condition m_condition;
};

class DisplayBufferDisplayDelegate final : public GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<DisplayBufferDisplayDelegate> create(bool isOpaque)
    {
        return adoptRef(*new DisplayBufferDisplayDelegate(isOpaque));
    }

    // GraphicsLayerContentsDisplayDelegate overrides.
    void prepareToDelegateDisplay(PlatformCALayer& layer) final
    {
        layer.setOpaque(m_isOpaque);
    }

    void display(PlatformCALayer& layer) final
    {
        if (m_displayBuffer)
            layer.setDelegatedContents({ *m_displayBuffer, m_finishedFence });
        else
            layer.clearContents();
    }

    GraphicsLayer::CompositingCoordinatesOrientation orientation() const final
    {
        return GraphicsLayer::CompositingCoordinatesOrientation::BottomUp;
    }

    void setDisplayBuffer(IOSurface* displayBuffer, RefPtr<DisplayBufferFence> finishedFence)
    {
        if (!displayBuffer) {
            m_finishedFence = nullptr;
            m_displayBuffer.reset();
            return;
        }
        if (m_displayBuffer && displayBuffer->surface() == m_displayBuffer->surface())
            return;
        m_displayBuffer = IOSurface::createFromSurface(displayBuffer->surface(), { });
        m_finishedFence = WTFMove(finishedFence);
    }

private:
    DisplayBufferDisplayDelegate(bool isOpaque)
        : m_isOpaque(isOpaque)
    {
    }

    std::unique_ptr<IOSurface> m_displayBuffer;
    RefPtr<DisplayBufferFence> m_finishedFence;
    const bool m_isOpaque;
};

// GraphicsContextGL type that is used when WebGL is run in-process in WebContent process.
class WebProcessGraphicsContextGLCocoa final : public GraphicsContextGLCocoa
#if PLATFORM(MAC)
    , private DisplayConfigurationMonitor::Client
#endif
{
public:
    ~WebProcessGraphicsContextGLCocoa();

    // GraphicsContextGLCocoa overrides.
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final { return m_layerContentsDisplayDelegate.ptr(); }
    void prepareForDisplay() final;
private:
#if PLATFORM(MAC)
    // DisplayConfigurationMonitor::Client overrides.
    void displayWasReconfigured() final;
#endif
    WebProcessGraphicsContextGLCocoa(GraphicsContextGLAttributes&&, SerialFunctionDispatcher*);
    Ref<DisplayBufferDisplayDelegate> m_layerContentsDisplayDelegate;

    friend RefPtr<GraphicsContextGL> WebCore::createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes&, SerialFunctionDispatcher*);
    friend class GraphicsContextGLOpenGL;
};

WebProcessGraphicsContextGLCocoa::WebProcessGraphicsContextGLCocoa(GraphicsContextGLAttributes&& attributes, SerialFunctionDispatcher* dispatcher)
    : GraphicsContextGLCocoa(WTFMove(attributes), { })
    , m_layerContentsDisplayDelegate(DisplayBufferDisplayDelegate::create(!attributes.alpha))
{
#if PLATFORM(MAC)
    DisplayConfigurationMonitor::singleton().addClient(*this, dispatcher);
#else
    UNUSED_PARAM(dispatcher);
#endif
}

WebProcessGraphicsContextGLCocoa::~WebProcessGraphicsContextGLCocoa()
{
#if PLATFORM(MAC)
    DisplayConfigurationMonitor::singleton().removeClient(*this);
#endif
}

void WebProcessGraphicsContextGLCocoa::prepareForDisplay()
{
    auto finishedFence = DisplayBufferFence::create();
    prepareForDisplayWithFinishedSignal([finishedFence] {
        finishedFence->signalAll();
    });
    // Here we do not record the finishedFence to be force signalled when context is lost.
    // Currently there's no mechanism to detect if scheduled commands were lost, so we
    // assume that scheduled fence will always be signalled. 
    // Here we trust that compositor does not advance too far with multiple frames.
    m_layerContentsDisplayDelegate->setDisplayBuffer(displayBufferSurface(), WTFMove(finishedFence));
}

#if PLATFORM(MAC)
void WebProcessGraphicsContextGLCocoa::displayWasReconfigured()
{
    updateContextOnDisplayReconfiguration();
}
#endif

}

RefPtr<GraphicsContextGL> createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes& attributes, SerialFunctionDispatcher* dispatcher)
{
    auto context = adoptRef(new WebProcessGraphicsContextGLCocoa(GraphicsContextGLAttributes { attributes }, dispatcher));
    if (!context->initialize())
        return nullptr;
    return context;
}

}

#endif
