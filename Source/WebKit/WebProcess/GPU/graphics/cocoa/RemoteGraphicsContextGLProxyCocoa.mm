/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteGraphicsContextGLProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)
#import "GPUConnectionToWebProcess.h"
#import "GPUProcessConnection.h"
#import "RemoteGraphicsContextGLMessages.h"
#import "WebProcess.h"
#import <WebCore/CVUtilities.h>
#import <WebCore/GraphicsLayerContentsDisplayDelegate.h>
#import <WebCore/IOSurface.h>
#import <WebCore/PlatformCALayer.h>

namespace WebKit {

namespace {

class DisplayBufferDisplayDelegate final : public WebCore::GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<DisplayBufferDisplayDelegate> create(bool isOpaque, float contentsScale)
    {
        return adoptRef(*new DisplayBufferDisplayDelegate(isOpaque, contentsScale));
    }

    // WebCore::GraphicsLayerContentsDisplayDelegate overrides.
    void prepareToDelegateDisplay(WebCore::PlatformCALayer& layer) final
    {
        layer.setOpaque(m_isOpaque);
        layer.setContentsScale(m_contentsScale);
    }

    void display(WebCore::PlatformCALayer& layer) final
    {
        if (m_displayBuffer)
            layer.setContents(m_displayBuffer);
        else
            layer.clearContents();
    }

    WebCore::GraphicsLayer::CompositingCoordinatesOrientation orientation() const final
    {
        return WebCore::GraphicsLayer::CompositingCoordinatesOrientation::BottomUp;
    }

    void setDisplayBuffer(const MachSendRight& displayBuffer)
    {
        if (!displayBuffer) {
            m_displayBuffer = { };
            return;
        }
        if (m_displayBuffer && displayBuffer.sendRight() == m_displayBuffer.sendRight())
            return;
        m_displayBuffer = displayBuffer.copySendRight();
    }

private:
    DisplayBufferDisplayDelegate(bool isOpaque, float contentsScale)
        : m_contentsScale(contentsScale)
        , m_isOpaque(isOpaque)
    {
    }

    MachSendRight m_displayBuffer;
    const float m_contentsScale;
    const bool m_isOpaque;
};

class RemoteGraphicsContextGLProxyCocoa final : public RemoteGraphicsContextGLProxy {
public:
    // RemoteGraphicsContextGLProxy overrides.
    RefPtr<WebCore::GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final { return m_layerContentsDisplayDelegate.ptr(); }
    void prepareForDisplay() final;
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    WebCore::GraphicsContextGLCV* asCV() final { return nullptr; }
#endif
private:
    RemoteGraphicsContextGLProxyCocoa(GPUProcessConnection& gpuProcessConnection, const WebCore::GraphicsContextGLAttributes& attributes, RenderingBackendIdentifier renderingBackend)
        : RemoteGraphicsContextGLProxy(gpuProcessConnection, attributes, renderingBackend)
        , m_layerContentsDisplayDelegate(DisplayBufferDisplayDelegate::create(!attributes.alpha, attributes.devicePixelRatio))
    {
    }

    MachSendRight m_displayBuffer;
    Ref<DisplayBufferDisplayDelegate> m_layerContentsDisplayDelegate;
    friend class RemoteGraphicsContextGLProxy;
};

void RemoteGraphicsContextGLProxyCocoa::prepareForDisplay()
{
    if (isContextLost())
        return;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PrepareForDisplay());
    if (!sendResult) {
        markContextLost();
        return;
    }
    auto [displayBufferSendRight] = sendResult.takeReply();
    if (!displayBufferSendRight)
        return;
    m_displayBuffer = WTFMove(displayBufferSendRight);
    markLayerComposited();
    m_layerContentsDisplayDelegate->setDisplayBuffer(m_displayBuffer.copySendRight());
}

}

RefPtr<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::create(const WebCore::GraphicsContextGLAttributes& attributes, RenderingBackendIdentifier renderingBackend)
{
    return adoptRef(new RemoteGraphicsContextGLProxyCocoa(WebProcess::singleton().ensureGPUProcessConnection(), attributes, renderingBackend));
}

}

#endif
