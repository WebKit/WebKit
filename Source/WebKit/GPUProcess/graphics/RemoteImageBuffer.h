/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "Decoder.h"
#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "RemoteDisplayListRecorder.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include <WebCore/ConcreteImageBuffer.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListReplayer.h>

namespace WebKit {

template<typename BackendType>
class RemoteImageBuffer : public WebCore::ConcreteImageBuffer<BackendType> {
    using BaseConcreteImageBuffer = WebCore::ConcreteImageBuffer<BackendType>;
    using BaseConcreteImageBuffer::context;
    using BaseConcreteImageBuffer::m_backend;
    using BaseConcreteImageBuffer::putPixelBuffer;

public:
    static auto create(const WebCore::FloatSize& size, float resolutionScale, const WebCore::DestinationColorSpace& colorSpace, WebCore::PixelFormat pixelFormat, RemoteRenderingBackend& remoteRenderingBackend, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
    {
        return BaseConcreteImageBuffer::template create<RemoteImageBuffer>(size, resolutionScale, colorSpace, pixelFormat, nullptr, remoteRenderingBackend, renderingResourceIdentifier);
    }

    RemoteImageBuffer(const WebCore::ImageBufferBackend::Parameters& parameters, std::unique_ptr<BackendType>&& backend, RemoteRenderingBackend& remoteRenderingBackend, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
        : BaseConcreteImageBuffer(parameters, WTFMove(backend), renderingResourceIdentifier.object())
        , m_remoteRenderingBackend(remoteRenderingBackend)
        , m_renderingResourceIdentifier(renderingResourceIdentifier)
        , m_remoteDisplayList({ RemoteDisplayListRecorder::create(*this, renderingResourceIdentifier, renderingResourceIdentifier.processIdentifier(), remoteRenderingBackend) })
    {
        m_remoteRenderingBackend.didCreateImageBufferBackend(m_backend->createBackendHandle(), renderingResourceIdentifier, *m_remoteDisplayList.get());
    }

    ~RemoteImageBuffer()
    {
        // Unwind the context's state stack before destruction, since calls to restore may not have
        // been flushed yet, or the web process may have terminated.
        while (context().stackSize())
            context().restore();
    }

    void setOwnershipIdentity(const WebCore::ProcessIdentity& resourceOwner)
    {
        if (m_backend)
            m_backend->setOwnershipIdentity(resourceOwner);
    }

private:
    friend class RemoteRenderingBackend;

    RemoteRenderingBackend& m_remoteRenderingBackend;
    QualifiedRenderingResourceIdentifier m_renderingResourceIdentifier;
    IPC::ScopedActiveMessageReceiveQueue<RemoteDisplayListRecorder> m_remoteDisplayList;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
