
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
#import "ProcessIdentity.h"

#if PLATFORM(MAC)
#import "DisplayConfigurationMonitor.h"
#endif

namespace WebCore {
namespace {

class DisplayBufferDisplayDelegate final : public GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<DisplayBufferDisplayDelegate> create(bool isOpaque, float contentsScale)
    {
        return adoptRef(*new DisplayBufferDisplayDelegate(isOpaque, contentsScale));
    }

    // GraphicsLayerContentsDisplayDelegate overrides.
    void prepareToDelegateDisplay(PlatformCALayer& layer) final
    {
        layer.setOpaque(m_isOpaque);
        layer.setContentsScale(m_contentsScale);
    }

    void display(PlatformCALayer& layer) final
    {
        if (m_displayBuffer)
            layer.setContents(*m_displayBuffer);
        else
            layer.clearContents();
    }

    GraphicsLayer::CompositingCoordinatesOrientation orientation() const final
    {
        return GraphicsLayer::CompositingCoordinatesOrientation::BottomUp;
    }

    void setDisplayBuffer(IOSurface* displayBuffer)
    {
        if (!displayBuffer) {
            m_displayBuffer.reset();
            return;
        }
        if (m_displayBuffer && displayBuffer->surface() == m_displayBuffer->surface())
            return;
        m_displayBuffer = IOSurface::createFromSurface(displayBuffer->surface(), { });
    }

private:
    DisplayBufferDisplayDelegate(bool isOpaque, float contentsScale)
        : m_contentsScale(contentsScale)
        , m_isOpaque(isOpaque)
    {
    }

    std::unique_ptr<IOSurface> m_displayBuffer;
    const float m_contentsScale;
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
    WebProcessGraphicsContextGLCocoa(GraphicsContextGLAttributes&&);

    Ref<DisplayBufferDisplayDelegate> m_layerContentsDisplayDelegate;
    friend RefPtr<GraphicsContextGL> WebCore::createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes&);
    friend class GraphicsContextGLOpenGL;
};

WebProcessGraphicsContextGLCocoa::WebProcessGraphicsContextGLCocoa(GraphicsContextGLAttributes&& attributes)
    : GraphicsContextGLCocoa(WTFMove(attributes), { })
    , m_layerContentsDisplayDelegate(DisplayBufferDisplayDelegate::create(!attributes.alpha, attributes.devicePixelRatio))
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
    m_layerContentsDisplayDelegate->setDisplayBuffer(displayBuffer());
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
    auto context = adoptRef(new WebProcessGraphicsContextGLCocoa(GraphicsContextGLAttributes { attributes }));
    if (!context->initialize())
        return nullptr;
    return context;
}

}

#endif
