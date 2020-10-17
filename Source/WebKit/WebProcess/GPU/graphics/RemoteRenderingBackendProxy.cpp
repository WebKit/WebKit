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
#include "RemoteRenderingBackendProxy.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcessConnection.h"
#include "PlatformRemoteImageBufferProxy.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "WebProcess.h"

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteRenderingBackendProxy> RemoteRenderingBackendProxy::create()
{
    return std::unique_ptr<RemoteRenderingBackendProxy>(new RemoteRenderingBackendProxy());
}

RemoteRenderingBackendProxy::RemoteRenderingBackendProxy()
{
    // Register itself as a MessageReceiver in the GPUProcessConnection.
    IPC::MessageReceiverMap& messageReceiverMap = WebProcess::singleton().ensureGPUProcessConnection().messageReceiverMap();
    messageReceiverMap.addMessageReceiver(Messages::RemoteRenderingBackendProxy::messageReceiverName(), m_renderingBackendIdentifier.toUInt64(), *this);

    // Create the RemoteRenderingBackend
    send(Messages::GPUConnectionToWebProcess::CreateRenderingBackend(m_renderingBackendIdentifier), 0);
}

RemoteRenderingBackendProxy::~RemoteRenderingBackendProxy()
{
    // Un-register itself as a MessageReceiver.
    IPC::MessageReceiverMap& messageReceiverMap = WebProcess::singleton().ensureGPUProcessConnection().messageReceiverMap();
    messageReceiverMap.removeMessageReceiver(*this);

    // Release the RemoteRenderingBackend.
    send(Messages::GPUConnectionToWebProcess::ReleaseRenderingBackend(m_renderingBackendIdentifier), 0);
}

IPC::Connection* RemoteRenderingBackendProxy::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureGPUProcessConnection().connection();
}

uint64_t RemoteRenderingBackendProxy::messageSenderDestinationID() const
{
    return m_renderingBackendIdentifier.toUInt64();
}

bool RemoteRenderingBackendProxy::waitForImageBufferBackendWasCreated()
{
    Ref<IPC::Connection> connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
    return connection->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::ImageBufferBackendWasCreated>(m_renderingBackendIdentifier, 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
}

bool RemoteRenderingBackendProxy::waitForFlushDisplayListWasCommitted()
{
    Ref<IPC::Connection> connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
    return connection->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::FlushDisplayListWasCommitted>(m_renderingBackendIdentifier, 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
}

std::unique_ptr<ImageBuffer> RemoteRenderingBackendProxy::createImageBuffer(const FloatSize& size, ShouldAccelerate shouldAccelerate, float resolutionScale, ColorSpace colorSpace)
{
    if (shouldAccelerate == ShouldAccelerate::Yes) {
        if (auto imageBuffer = AcceleratedRemoteImageBufferProxy::create(size, RenderingMode::RemoteAccelerated, resolutionScale, colorSpace, *this)) {
            m_imageBufferMessageHandlerMap.add(imageBuffer->remoteResourceIdentifier(), imageBuffer.get());
            return imageBuffer;
        }
    }

    if (auto imageBuffer = UnacceleratedRemoteImageBufferProxy::create(size, RenderingMode::RemoteUnaccelerated, resolutionScale, colorSpace, *this)) {
        m_imageBufferMessageHandlerMap.add(imageBuffer->remoteResourceIdentifier(), imageBuffer.get());
        return imageBuffer;
    }

    return nullptr;
}

void RemoteRenderingBackendProxy::releaseRemoteResource(RemoteResourceIdentifier remoteResourceIdentifier)
{
    // CreateImageBuffer message should have been received before this one.
    bool found = m_imageBufferMessageHandlerMap.remove(remoteResourceIdentifier);
    ASSERT_UNUSED(found, found);
}

void RemoteRenderingBackendProxy::imageBufferBackendWasCreated(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace colorSpace, ImageBufferBackendHandle handle, RemoteResourceIdentifier remoteResourceIdentifier)
{
    if (auto imageBuffer = m_imageBufferMessageHandlerMap.get(remoteResourceIdentifier))
        imageBuffer->createBackend(logicalSize, backendSize, resolutionScale, colorSpace, WTFMove(handle));
}

void RemoteRenderingBackendProxy::flushDisplayListWasCommitted(DisplayListFlushIdentifier flushIdentifier, RemoteResourceIdentifier remoteResourceIdentifier)
{
    if (auto imageBuffer = m_imageBufferMessageHandlerMap.get(remoteResourceIdentifier))
        imageBuffer->commitFlushContext(flushIdentifier);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
