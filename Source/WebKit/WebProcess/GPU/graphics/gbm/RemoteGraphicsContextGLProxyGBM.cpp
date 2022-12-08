/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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
#include "RemoteGraphicsContextGLProxy.h"
#include "RemoteRenderingBackendProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(LIBGBM)

// This is a standalone check of additional requirements in this implementation,
// avoiding having to place an overwhelming amount of build guards over the rest of the code.
#if !USE(NICOSIA) || !USE(TEXTURE_MAPPER_DMABUF)
#error RemoteGraphicsContextGLProxyGBM implementation also depends on USE_NICOSIA and USE_TEXTURE_MAPPER_DMABUF
#endif

#include "WebProcess.h"
#include <WebCore/DMABufObject.h>
#include <WebCore/GraphicsLayerContentsDisplayDelegate.h>
#include <WebCore/NicosiaContentLayerTextureMapperImpl.h>
#include <WebCore/TextureMapperPlatformLayerProxyDMABuf.h>

namespace WebKit {

class NicosiaDisplayDelegate final : public WebCore::GraphicsLayerContentsDisplayDelegate, public Nicosia::ContentLayerTextureMapperImpl::Client {
public:
    explicit NicosiaDisplayDelegate(bool isOpaque);
    virtual ~NicosiaDisplayDelegate();

    void present(WebCore::DMABufObject&& dmabufObject)
    {
        m_pending = WTFMove(dmabufObject);
    }

private:
    // WebCore::GraphicsLayerContentsDisplayDelegate
    Nicosia::PlatformLayer* platformLayer() const final;

    // Nicosia::ContentLayerTextureMapperImpl::Client
    void swapBuffersIfNeeded() final;

    bool m_isOpaque { false };
    RefPtr<Nicosia::ContentLayer> m_contentLayer;
    WebCore::DMABufObject m_pending { WebCore::DMABufObject(0) };
};

NicosiaDisplayDelegate::NicosiaDisplayDelegate(bool isOpaque)
    : m_isOpaque(isOpaque)
{
    m_contentLayer = Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this, adoptRef(*new WebCore::TextureMapperPlatformLayerProxyDMABuf)));
}

NicosiaDisplayDelegate::~NicosiaDisplayDelegate()
{ }

Nicosia::PlatformLayer* NicosiaDisplayDelegate::platformLayer() const
{
    return m_contentLayer.get();
}

void NicosiaDisplayDelegate::swapBuffersIfNeeded()
{
    auto& proxy = downcast<Nicosia::ContentLayerTextureMapperImpl>(m_contentLayer->impl()).proxy();
    ASSERT(is<WebCore::TextureMapperPlatformLayerProxyDMABuf>(proxy));

    if (!!m_pending.handle) {
        Locker locker { proxy.lock() };

        WebCore::TextureMapperGL::Flags flags = WebCore::TextureMapperGL::ShouldFlipTexture;
        if (!m_isOpaque)
            flags |= WebCore::TextureMapperGL::ShouldBlend;

        downcast<WebCore::TextureMapperPlatformLayerProxyDMABuf>(proxy).pushDMABuf(WTFMove(m_pending),
            [](auto&& object) { return object; }, flags);
    }
    m_pending = WebCore::DMABufObject(0);
}

class RemoteGraphicsContextGLProxyGBM final : public RemoteGraphicsContextGLProxy {
public:
    RemoteGraphicsContextGLProxyGBM(IPC::Connection&, SerialFunctionDispatcher&, const WebCore::GraphicsContextGLAttributes&, RenderingBackendIdentifier);
    virtual ~RemoteGraphicsContextGLProxyGBM() = default;

private:
    // WebCore::GraphicsContextGL
    RefPtr<WebCore::GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final;
    void prepareForDisplay() final;

    Ref<NicosiaDisplayDelegate> m_layerContentsDisplayDelegate;
};

RemoteGraphicsContextGLProxyGBM::RemoteGraphicsContextGLProxyGBM(IPC::Connection& connection, SerialFunctionDispatcher& dispatcher, const WebCore::GraphicsContextGLAttributes& attributes, RenderingBackendIdentifier renderingBackend)
    : RemoteGraphicsContextGLProxy(connection, dispatcher, attributes, renderingBackend)
    , m_layerContentsDisplayDelegate(adoptRef(*new NicosiaDisplayDelegate(!attributes.alpha)))
{ }

RefPtr<WebCore::GraphicsLayerContentsDisplayDelegate> RemoteGraphicsContextGLProxyGBM::layerContentsDisplayDelegate()
{
    return m_layerContentsDisplayDelegate.copyRef();
}

void RemoteGraphicsContextGLProxyGBM::prepareForDisplay()
{
    if (isContextLost())
        return;

    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PrepareForDisplay());
    if (!sendResult) {
        markContextLost();
        return;
    }

    auto& [dmabufObject] = sendResult.reply();
    m_layerContentsDisplayDelegate->present(WTFMove(dmabufObject));
    markLayerComposited();
}

RefPtr<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::create(IPC::Connection& connection, const WebCore::GraphicsContextGLAttributes& attributes, RemoteRenderingBackendProxy& renderingBackend)
{
    return adoptRef(new RemoteGraphicsContextGLProxyGBM(connection, renderingBackend.dispatcher(), attributes, renderingBackend.ensureBackendCreated()));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(LIBGBM)
