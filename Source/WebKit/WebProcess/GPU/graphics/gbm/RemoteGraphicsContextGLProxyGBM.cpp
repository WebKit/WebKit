/*
 * Copyright (C) 2024 Igalia S.L.
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

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
#include <WebCore/CoordinatedPlatformLayerBufferDMABuf.h>
#include <WebCore/DMABufBuffer.h>
#include <WebCore/GraphicsLayerContentsDisplayDelegateTextureMapper.h>
#include <WebCore/TextureMapperFlags.h>
#include <WebCore/TextureMapperPlatformLayerProxy.h>

namespace WebKit {
using namespace WebCore;

class RemoteGraphicsLayerContentsDisplayDelegateGBM final : public GraphicsLayerContentsDisplayDelegateTextureMapper {
public:
    static Ref<RemoteGraphicsLayerContentsDisplayDelegateGBM> create(bool isOpaque)
    {
        return adoptRef(*new RemoteGraphicsLayerContentsDisplayDelegateGBM(isOpaque));
    }

    virtual ~RemoteGraphicsLayerContentsDisplayDelegateGBM()
    {
        m_proxy->setSwapBuffersFunction(nullptr);
    }

    void setDisplayBuffer(Ref<DMABufBuffer>&& displayBuffer, UnixFileDescriptor&& fenceFD)
    {
        std::swap(m_drawingBuffer, m_displayBuffer);
        m_displayBuffer = WTFMove(displayBuffer);
        m_fenceFD = WTFMove(fenceFD);
    }

    void setDisplayBuffer(uint64_t bufferID, UnixFileDescriptor&& fenceFD)
    {
        if (m_drawingBuffer && m_drawingBuffer->id() == bufferID)
            std::swap(m_drawingBuffer, m_displayBuffer);
        m_fenceFD = WTFMove(fenceFD);
    }

private:
    explicit RemoteGraphicsLayerContentsDisplayDelegateGBM(bool isOpaque)
        : GraphicsLayerContentsDisplayDelegateTextureMapper(TextureMapperPlatformLayerProxy::create(TextureMapperPlatformLayerProxy::ContentType::WebGL))
        , m_isOpaque(isOpaque)
    {
        m_proxy->setSwapBuffersFunction([this](TextureMapperPlatformLayerProxy& proxy) mutable {
            if (!m_displayBuffer)
                return;

            OptionSet<TextureMapperFlags> flags = TextureMapperFlags::ShouldFlipTexture;
            if (!m_isOpaque)
                flags.add(TextureMapperFlags::ShouldBlend);
            proxy.pushNextBuffer(CoordinatedPlatformLayerBufferDMABuf::create(Ref { *m_displayBuffer }, flags, WTFMove(m_fenceFD)));
        });
    }

    bool m_isOpaque { false };
    RefPtr<DMABufBuffer> m_drawingBuffer;
    RefPtr<DMABufBuffer> m_displayBuffer;
    UnixFileDescriptor m_fenceFD;
};

class RemoteGraphicsContextGLProxyGBM final : public RemoteGraphicsContextGLProxy {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteGraphicsContextGLProxyGBM);
public:
    virtual ~RemoteGraphicsContextGLProxyGBM() = default;

private:
    friend class RemoteGraphicsContextGLProxy;
    explicit RemoteGraphicsContextGLProxyGBM(const GraphicsContextGLAttributes& attributes)
        : RemoteGraphicsContextGLProxy(attributes)
        , m_layerContentsDisplayDelegate(RemoteGraphicsLayerContentsDisplayDelegateGBM::create(!attributes.alpha))
    {
    }

    // WebCore::GraphicsContextGL
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final { return m_layerContentsDisplayDelegate.copyRef(); }
    void prepareForDisplay() final;

    Ref<RemoteGraphicsLayerContentsDisplayDelegateGBM> m_layerContentsDisplayDelegate;
};

void RemoteGraphicsContextGLProxyGBM::prepareForDisplay()
{
    if (isContextLost())
        return;

    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PrepareForDisplay());
    if (!sendResult.succeeded()) {
        markContextLost();
        return;
    }

    auto [bufferID, bufferAttributes, fenceFD] = sendResult.takeReply();
    if (bufferAttributes)
        m_layerContentsDisplayDelegate->setDisplayBuffer(DMABufBuffer::create(bufferID, WTFMove(*bufferAttributes)), WTFMove(fenceFD));
    else
        m_layerContentsDisplayDelegate->setDisplayBuffer(bufferID, WTFMove(fenceFD));
}

Ref<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::platformCreate(const GraphicsContextGLAttributes& attributes)
{
    return adoptRef(*new RemoteGraphicsContextGLProxyGBM(attributes));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
