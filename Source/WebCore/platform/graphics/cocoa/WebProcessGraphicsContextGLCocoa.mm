
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
#import "GraphicsContextGLOpenGLManager.h"
#import "WebGLLayer.h"
#import <wtf/BlockObjCExceptions.h>

#if PLATFORM(MAC)
#import "DisplayConfigurationMonitor.h"
#endif

namespace WebCore {
namespace {

static RetainPtr<WebGLLayer> createWebGLLayer(const GraphicsContextGLAttributes& attributes)
{
    RetainPtr<WebGLLayer> layer;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    layer = adoptNS([[WebGLLayer alloc] initWithDevicePixelRatio:attributes.devicePixelRatio contentsOpaque:!attributes.alpha]);
#ifndef NDEBUG
    [layer setName:@"WebGL Layer"];
#endif
    END_BLOCK_OBJC_EXCEPTIONS
    return layer;
}

// GraphicsContextGL type that is used when WebGL is run in-process in WebContent process.
class WebProcessGraphicsContextGLCocoa final : public GraphicsContextGLCocoa
#if PLATFORM(MAC)
    , private DisplayConfigurationMonitor::Client
#endif

{
public:
    ~WebProcessGraphicsContextGLCocoa();
    bool isValid() const { return GraphicsContextGLCocoa::isValid() && m_webGLLayer; }
    // GraphicsContextGLCocoa overrides.
    PlatformLayer* platformLayer() const final { return reinterpret_cast<CALayer*>(m_webGLLayer.get()); }
    void prepareForDisplay() final;
private:
#if PLATFORM(MAC)
    // DisplayConfigurationMonitor::Client overrides.
    void displayWasReconfigured() final;
#endif

    WebProcessGraphicsContextGLCocoa(GraphicsContextGLAttributes&&);
    RetainPtr<WebGLLayer> m_webGLLayer;

    friend RefPtr<GraphicsContextGL> WebCore::createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes&);
};

WebProcessGraphicsContextGLCocoa::WebProcessGraphicsContextGLCocoa(GraphicsContextGLAttributes&& attributes)
    : GraphicsContextGLCocoa(WTFMove(attributes))
    , m_webGLLayer(createWebGLLayer(contextAttributes()))
{
#if PLATFORM(MAC)
    DisplayConfigurationMonitor::singleton().addClient(*this);
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
    GraphicsContextGLCocoa::prepareForDisplay();
    auto surface = m_swapChain.displayBuffer().surface.get();
    if (!surface)
        return;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_webGLLayer setContents:surface->asLayerContents()];
    [m_webGLLayer setNeedsDisplay];
    END_BLOCK_OBJC_EXCEPTIONS
}

#if PLATFORM(MAC)
void WebProcessGraphicsContextGLCocoa::displayWasReconfigured()
{
    updateContextOnDisplayReconfiguration();
}
#endif

}

RefPtr<GraphicsContextGL> createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes& attributes)
{
    // Make space for the incoming context if we're full.
    GraphicsContextGLOpenGLManager::sharedManager().recycleContextIfNecessary();
    if (GraphicsContextGLOpenGLManager::sharedManager().hasTooManyContexts())
        return nullptr;
    auto context = adoptRef(new WebProcessGraphicsContextGLCocoa(GraphicsContextGLAttributes { attributes }));
    if (!context->isValid())
        return nullptr;
    GraphicsContextGLOpenGLManager::sharedManager().addContext(context.get());
    return context;
}

}

#endif
