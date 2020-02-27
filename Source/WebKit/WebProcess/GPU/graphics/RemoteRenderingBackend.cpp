/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteRenderingBackend.h"

#if ENABLE(GPU_PROCESS)

#include "GPUProcessConnection.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteRenderingBackend> RemoteRenderingBackend::create()
{
    return std::unique_ptr<RemoteRenderingBackend>(new RemoteRenderingBackend());
}

RemoteRenderingBackend::RemoteRenderingBackend()
{
    // Register itself as a MessageReceiver in the GPUProcessConnection.
    IPC::MessageReceiverMap& messageReceiverMap = WebProcess::singleton().ensureGPUProcessConnection().messageReceiverMap();
    messageReceiverMap.addMessageReceiver(Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64(), *this);

    // Create the RemoteRenderingBackendProxy.
    send(Messages::GPUConnectionToWebProcess::CreateRenderingBackend(m_renderingBackendIdentifier), 0);
}

RemoteRenderingBackend::~RemoteRenderingBackend()
{
    // Un-register itself as a MessageReceiver.
    IPC::MessageReceiverMap& messageReceiverMap = WebProcess::singleton().ensureGPUProcessConnection().messageReceiverMap();
    messageReceiverMap.removeMessageReceiver(*this);

    // Release the RemoteRenderingBackendProxy.
    send(Messages::GPUConnectionToWebProcess::ReleaseRenderingBackend(m_renderingBackendIdentifier), 0);
}

IPC::Connection* RemoteRenderingBackend::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureGPUProcessConnection().connection();
}

uint64_t RemoteRenderingBackend::messageSenderDestinationID() const
{
    return m_renderingBackendIdentifier.toUInt64();
}

std::unique_ptr<ImageBuffer> RemoteRenderingBackend::createImageBuffer(const FloatSize&, RenderingMode renderingMode, float, ColorSpace)
{
    switch (renderingMode) {
    case RenderingMode::RemoteAccelerated:
    case RenderingMode::RemoteUnaccelerated:
        // FIXME: Create the remote ImageBuffer
        return nullptr;

    default:
        ASSERT_NOT_REACHED();
    }

    return nullptr;
}

void RemoteRenderingBackend::releaseImageBuffer(ImageBufferIdentifier imageBufferIdentifier)
{
    // CreateImageBuffer message should have been received before this one.
    bool found = m_imageBufferMap.remove(imageBufferIdentifier);
    ASSERT_UNUSED(found, found);

    send(Messages::RemoteRenderingBackendProxy::ReleaseImageBuffer(imageBufferIdentifier), m_renderingBackendIdentifier);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
