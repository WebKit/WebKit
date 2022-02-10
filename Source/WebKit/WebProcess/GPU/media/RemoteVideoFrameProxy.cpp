/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "RemoteVideoFrameProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
#include "GPUConnectionToWebProcess.h"
#include "RemoteVideoFrameObjectHeapMessages.h"
#include "RemoteVideoFrameProxyMessages.h"

namespace WebKit {

RefPtr<RemoteVideoFrameProxy> RemoteVideoFrameProxy::create(GPUProcessConnection& gpuProcessConnection)
{
    return adoptRef(new RemoteVideoFrameProxy(gpuProcessConnection, RemoteVideoFrameIdentifier::generate()));
}

RemoteVideoFrameProxy::Properties RemoteVideoFrameProxy::properties(const WebCore::MediaSample& mediaSample)
{
    return {
        mediaSample.presentationTime(),
        mediaSample.videoMirrored(),
        mediaSample.videoRotation()
    };
}

RemoteVideoFrameProxy::RemoteVideoFrameProxy(GPUProcessConnection& gpuProcessConnection, RemoteVideoFrameIdentifier identifier)
    : m_gpuProcessConnection(&gpuProcessConnection)
    , m_referenceTracker(identifier)
{
    m_gpuProcessConnection->addClient(*this);
    m_gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::RemoteVideoFrameProxy::messageReceiverName(), identifier.toUInt64(), *this);
}

RemoteVideoFrameProxy::RemoteVideoFrameProxy()
    : m_gpuProcessConnection(nullptr)
    , m_referenceTracker(RemoteVideoFrameIdentifier::generate())
{
}

RemoteVideoFrameProxy::~RemoteVideoFrameProxy()
{
    disconnectGPUProcessIfNeeded();
}

RemoteVideoFrameIdentifier RemoteVideoFrameProxy::identifier() const
{
    return m_referenceTracker.identifier();
}

RemoteVideoFrameWriteReference RemoteVideoFrameProxy::write() const
{
    return m_referenceTracker.write();
}

RemoteVideoFrameReadReference RemoteVideoFrameProxy::read() const
{
    return m_referenceTracker.read();
}

MediaTime RemoteVideoFrameProxy::presentationTime() const
{
    (void) waitForWasInitialized();
    return m_presentationTime;
}

WebCore::PlatformSample RemoteVideoFrameProxy::platformSample() const
{
    return { WebCore::PlatformSample::RemoteVideoFrameProxyType, { } };
}

MediaSample::VideoRotation RemoteVideoFrameProxy::videoRotation() const
{
    (void) waitForWasInitialized();
    return m_videoRotation;
}

bool RemoteVideoFrameProxy::videoMirrored() const
{
    (void) waitForWasInitialized();
    return m_videoIsMirrored;
}

uint32_t RemoteVideoFrameProxy::videoPixelFormat() const
{
    // FIXME: Remove from the base class.
    ASSERT_NOT_REACHED();
    return 0;
}

bool RemoteVideoFrameProxy::handleMessageToRemovedDestination(IPC::Connection&, IPC::Decoder& decoder)
{
    // Skip messages intended for already removed messageReceiverMap() destinations.
    // These are business as usual. These can happen for example by:
    //  - The object is created and immediately destroyed, before WasInitialized was handled.
    ASSERT(decoder.messageName() == Messages::RemoteVideoFrameProxy::WasInitialized::name());
    return true;
}

std::optional<WebCore::MediaSampleVideoFrame> RemoteVideoFrameProxy::videoFrame() const
{
    return std::nullopt;
}

void RemoteVideoFrameProxy::disconnectGPUProcessIfNeeded() const
{
    if (auto gpuProcessConnection = std::exchange(m_gpuProcessConnection, nullptr)) {
        gpuProcessConnection->removeClient(*this);
        gpuProcessConnection->messageReceiverMap().removeMessageReceiver(Messages::RemoteVideoFrameProxy::messageReceiverName(), identifier().toUInt64());
        gpuProcessConnection->connection().send(Messages::RemoteVideoFrameObjectHeap::ReleaseVideoFrame(write()), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    }
}

bool RemoteVideoFrameProxy::waitForWasInitialized() const
{
    if (m_wasInitialized)
        return true;
    if (m_gpuProcessConnection) {
        if (m_gpuProcessConnection->connection().waitForAndDispatchImmediately<Messages::RemoteVideoFrameProxy::WasInitialized>(identifier().toUInt64(), defaultSendTimeout))
            return true;
        disconnectGPUProcessIfNeeded();
    }
    return false;
}

void RemoteVideoFrameProxy::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    auto gpuProcessConnection = std::exchange(m_gpuProcessConnection, nullptr);
    gpuProcessConnection->removeClient(*this);
    gpuProcessConnection->messageReceiverMap().removeMessageReceiver(Messages::RemoteVideoFrameProxy::messageReceiverName(), identifier().toUInt64());
}

void RemoteVideoFrameProxy::wasInitialized(std::optional<Properties>&& properties)
{
    m_wasInitialized = true;
    if (!properties)
        return;
    // FIXME: When source fails to produce a sample due to some error, the properties are disengaged.
    // We should perhaps check this at the submit time.
    m_presentationTime = properties->presentationTime;
    m_videoIsMirrored = properties->videoIsMirrored;
    m_videoRotation = properties->videoRotation;
}

}
#endif
