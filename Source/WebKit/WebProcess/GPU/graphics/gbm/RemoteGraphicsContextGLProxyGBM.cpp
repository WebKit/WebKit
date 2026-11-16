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
#include <WebCore/GraphicsLayerContentsDisplayDelegateCoordinated.h>

namespace WebKit {
using namespace WebCore;

class RemoteGraphicsContextGLProxyGBM final : public RemoteGraphicsContextGLProxy {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(RemoteGraphicsContextGLProxyGBM);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteGraphicsContextGLProxyGBM);
public:
    virtual ~RemoteGraphicsContextGLProxyGBM() = default;

private:
    friend class RemoteGraphicsContextGLProxy;
    explicit RemoteGraphicsContextGLProxyGBM(const GraphicsContextGLAttributes& attributes, RemoteRenderingBackendProxy& renderingBackend)
        : RemoteGraphicsContextGLProxy(attributes, renderingBackend)
        , m_layerContentsDisplayDelegate(GraphicsLayerContentsDisplayDelegateCoordinated::create())
    {
    }

    // WebCore::GraphicsContextGL
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final { return m_layerContentsDisplayDelegate.copyRef(); }
    void prepareForDisplay() final;

    const Ref<GraphicsLayerContentsDisplayDelegate> m_layerContentsDisplayDelegate;
    HashMap<uint64_t, Ref<DMABufBuffer>> m_buffers;
};

void RemoteGraphicsContextGLProxyGBM::prepareForDisplay()
{
    if (isContextLost())
        return;

    Vector<uint64_t> inUseBuffers;
    for (const auto& buffer : m_buffers.values()) {
        if (!buffer->hasOneRef())
            inUseBuffers.append(buffer->id());
    }

    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PrepareForDisplay(WTF::move(inUseBuffers)));
    if (!sendResult.succeeded()) {
        markContextLost();
        return;
    }

    auto [bufferID, bufferAttributes, fenceFD, drawingBufferIDs] = sendResult.takeReply();

    m_buffers.removeIf([&](auto& entry) {
        return !drawingBufferIDs.contains(entry.key);
    });

    RefPtr<DMABufBuffer> displayBuffer;
    if (bufferAttributes) {
        displayBuffer = DMABufBuffer::create(bufferID, WTF::move(*bufferAttributes));
        m_buffers.add(bufferID, Ref { *displayBuffer });
    } else
        displayBuffer = m_buffers.get(bufferID);

    if (!displayBuffer)
        return;

    auto alphaMode = contextAttributes().alpha ? CoordinatedPlatformLayerBuffer::AlphaMode::Premultiplied : CoordinatedPlatformLayerBuffer::AlphaMode::Opaque;
    auto origin = CoordinatedPlatformLayerBuffer::Origin::BottomLeft;
    m_layerContentsDisplayDelegate->setDisplayBuffer(CoordinatedPlatformLayerBufferDMABuf::create(protect(*displayBuffer), alphaMode, origin, WTF::move(fenceFD), m_layerContentsDisplayDelegate->threadSafeGrContext()));

    m_hasPreparedForDisplay = true;
}

Ref<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::platformCreate(const GraphicsContextGLAttributes& attributes, RemoteRenderingBackendProxy& renderingBackend)
{
    return adoptRef(*new RemoteGraphicsContextGLProxyGBM(attributes, renderingBackend));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
