/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "RemoteGraphicsContextGLProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(GRAPHICS_LAYER_WC)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcessConnection.h"
#include "RemoteGraphicsContextGLMessages.h"
#include "WCPlatformLayerGCGL.h"
#include "WebProcess.h"
#include <WebCore/TextureMapperPlatformLayer.h>

namespace WebKit {

namespace {

class RemoteGraphicsContextGLProxyWC final : public RemoteGraphicsContextGLProxy {
public:
    // RemoteGraphicsContextGLProxy overrides.
    void prepareForDisplay() final;
    PlatformLayer* platformLayer() const final { return m_platformLayer.get(); }
#if ENABLE(MEDIA_STREAM)
    RefPtr<WebCore::MediaSample> paintCompositedResultsToMediaSample() final { return nullptr; }
#endif
private:
    RemoteGraphicsContextGLProxyWC(GPUProcessConnection&, const WebCore::GraphicsContextGLAttributes&, RenderingBackendIdentifier);

    PlatformLayerContainer m_platformLayer;
    friend class RemoteGraphicsContextGLProxy;
};

RemoteGraphicsContextGLProxyWC::RemoteGraphicsContextGLProxyWC(GPUProcessConnection& gpuProcessConnection, const WebCore::GraphicsContextGLAttributes& attributes, RenderingBackendIdentifier renderingBackend)
    : RemoteGraphicsContextGLProxy(gpuProcessConnection, attributes, renderingBackend)
    , m_platformLayer(makeUnique<WCPlatformLayerGCGL>(m_graphicsContextGLIdentifier))
{
}

void RemoteGraphicsContextGLProxyWC::prepareForDisplay()
{
    if (isContextLost())
        return;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PrepareForDisplay(), Messages::RemoteGraphicsContextGL::PrepareForDisplay::Reply());
    if (!sendResult) {
        markContextLost();
        return;
    }
    markLayerComposited();
}

}

RefPtr<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::create(const WebCore::GraphicsContextGLAttributes& attributes, RenderingBackendIdentifier renderingBackend)
{
    return adoptRef(new RemoteGraphicsContextGLProxyWC(WebProcess::singleton().ensureGPUProcessConnection(), attributes, renderingBackend));
}

}

#endif
