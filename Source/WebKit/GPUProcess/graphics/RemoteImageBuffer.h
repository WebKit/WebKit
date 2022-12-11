/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "QualifiedRenderingResourceIdentifier.h"
#include "RemoteRenderingBackend.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "ScopedRenderingResourcesRequest.h"
#include <WebCore/ImageBuffer.h>

namespace WebKit {

class RemoteDisplayListRecorder;

class RemoteImageBuffer : public WebCore::ImageBuffer {
public:
    template<typename BackendType>
    static RefPtr<RemoteImageBuffer> create(const WebCore::FloatSize& size, float resolutionScale, const WebCore::DestinationColorSpace& colorSpace, WebCore::PixelFormat pixelFormat, WebCore::RenderingPurpose purpose, RemoteRenderingBackend& remoteRenderingBackend, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
    {
        auto context = ImageBuffer::CreationContext { nullptr
#if HAVE(IOSURFACE)
            , &remoteRenderingBackend.ioSurfacePool()
#endif
        };
        
        auto imageBuffer = ImageBuffer::create<BackendType, RemoteImageBuffer>(size, resolutionScale, colorSpace, pixelFormat, purpose, context, remoteRenderingBackend, renderingResourceIdentifier);
        if (!imageBuffer)
            return nullptr;

        auto backend = static_cast<BackendType*>(imageBuffer->backend());
        ASSERT(backend);

        remoteRenderingBackend.didCreateImageBufferBackend(backend->createBackendHandle(), renderingResourceIdentifier, *imageBuffer->m_remoteDisplayList.get());
        return imageBuffer;
    }

    RemoteImageBuffer(const WebCore::ImageBufferBackend::Parameters&, const WebCore::ImageBufferBackend::Info&, std::unique_ptr<WebCore::ImageBufferBackend>&&, RemoteRenderingBackend&, QualifiedRenderingResourceIdentifier);
    ~RemoteImageBuffer();

    void setOwnershipIdentity(const WebCore::ProcessIdentity& resourceOwner);

private:
    QualifiedRenderingResourceIdentifier m_renderingResourceIdentifier;
    IPC::ScopedActiveMessageReceiveQueue<RemoteDisplayListRecorder> m_remoteDisplayList;
    ScopedRenderingResourcesRequest m_renderingResourcesRequest;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
