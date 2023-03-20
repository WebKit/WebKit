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

#include "config.h"
#include "RemoteImageBuffer.h"

#if ENABLE(GPU_PROCESS)

#include "PlatformImageBufferShareableBackend.h"
#include "RemoteDisplayListRecorder.h"

namespace WebKit {
using namespace WebCore;

RemoteImageBuffer::RemoteImageBuffer(const ImageBufferBackend::Parameters& parameters, const ImageBufferBackend::Info& info, std::unique_ptr<ImageBufferBackend>&& backend, RemoteRenderingBackend& remoteRenderingBackend, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
    : ImageBuffer(parameters, info, WTFMove(backend), renderingResourceIdentifier.object())
    , m_renderingResourceIdentifier(renderingResourceIdentifier)
    , m_remoteDisplayList({ RemoteDisplayListRecorder::create(*this, renderingResourceIdentifier, renderingResourceIdentifier.processIdentifier(), remoteRenderingBackend) })
    , m_renderingResourcesRequest(ScopedRenderingResourcesRequest::acquire())
{
}

RemoteImageBuffer::~RemoteImageBuffer()
{
    // Volatile image buffers do not have contexts.
    if (this->volatilityState() == VolatilityState::Volatile)
        return;
    if (!m_backend)
        return;
    // Unwind the context's state stack before destruction, since calls to restore may not have
    // been flushed yet, or the web process may have terminated.
    while (context().stackSize())
        context().restore();
}

void RemoteImageBuffer::setOwnershipIdentity(const ProcessIdentity& resourceOwner)
{
    if (m_backend)
        m_backend->setOwnershipIdentity(resourceOwner);
}

RefPtr<RemoteImageBuffer> RemoteImageBuffer::createTransfer(std::unique_ptr<ImageBufferBackend>&& backend, const ImageBufferBackend::Info& backendInfo, RemoteRenderingBackend& remoteRenderingBackend, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    auto context = WebCore::ImageBufferCreationContext { nullptr
#if HAVE(IOSURFACE)
        , &remoteRenderingBackend.ioSurfacePool()
#endif
    };
    backend->transferToNewContext(context);
    auto* sharing = backend->toBackendSharing();
    ASSERT(sharing && is<ImageBufferBackendHandleSharing>(sharing));

    auto backendHandle = downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();

    auto imageBuffer = adoptRef(new RemoteImageBuffer(backend->parameters() , backendInfo, WTFMove(backend), remoteRenderingBackend, renderingResourceIdentifier));

    remoteRenderingBackend.didCreateImageBufferBackend(WTFMove(backendHandle), renderingResourceIdentifier, *imageBuffer->m_remoteDisplayList.get());
    return imageBuffer;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
