/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "RemoteVideoFrameObjectHeapProxy.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "RemoteVideoFrameObjectHeap.h"
#include "RemoteVideoFrameObjectHeapMessages.h"
#include "RemoteVideoFrameObjectHeapProxyProcessorMessages.h"
#include "RemoteVideoFrameProxy.h"
#include "WebProcess.h"
#include <WebCore/NativeImage.h>
#include <WebCore/PixelBufferConformerCV.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteVideoFrameObjectHeapProxyProcessor> RemoteVideoFrameObjectHeapProxyProcessor::create()
{
    return adoptRef(*new RemoteVideoFrameObjectHeapProxyProcessor());
}

RemoteVideoFrameObjectHeapProxyProcessor::RemoteVideoFrameObjectHeapProxyProcessor()
    : m_queue(WorkQueue::create("RemoteVideoFrameObjectHeapProxy"_s, WorkQueue::QOS::UserInteractive))
{
}

RemoteVideoFrameObjectHeapProxyProcessor::~RemoteVideoFrameObjectHeapProxyProcessor()
{
    ASSERT(isMainRunLoop());
    clearCallbacks();
}

void RemoteVideoFrameObjectHeapProxyProcessor::gpuProcessConnectionDidBecomeAvailable(GPUProcessConnection& gpuProcessConnection)
{
    gpuProcessConnection.addClient(*this);
    Ref connection = gpuProcessConnection.connection();
    connection->addWorkQueueMessageReceiver(Messages::RemoteVideoFrameObjectHeapProxyProcessor::messageReceiverName(), m_queue, *this);
    {
        Locker lock(m_connectionLock);
        m_connection = WTFMove(connection);
    }
}

void RemoteVideoFrameObjectHeapProxyProcessor::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    m_sharedVideoFrameWriter.disable();
    RefPtr<IPC::Connection> connection;
    {
        Locker lock(m_connectionLock);
        connection = std::exchange(m_connection, nullptr);
    }
    connection->removeWorkQueueMessageReceiver(Messages::RemoteVideoFrameObjectHeapProxyProcessor::messageReceiverName());
    clearCallbacks();
}

RefPtr<IPC::Connection> RemoteVideoFrameObjectHeapProxyProcessor::connection()
{
    {
        Locker locker { m_connectionLock };
        if (m_connection)
            return m_connection;
    }
    callOnMainRunLoopAndWait([] {
        WebProcess::singleton().ensureGPUProcessConnection();
    });
    Locker locker { m_connectionLock };
    return m_connection;
}

void RemoteVideoFrameObjectHeapProxyProcessor::clearCallbacks()
{
    HashMap<RemoteVideoFrameIdentifier, Callback> callbacks;
    {
        Locker lock(m_callbacksLock);
        callbacks = std::exchange(m_callbacks, { });
    }

    m_queue->dispatch([callbacks = WTFMove(callbacks)]() mutable {
        for (auto& callback : callbacks.values())
            callback({ });
    });
}

void RemoteVideoFrameObjectHeapProxyProcessor::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTFMove(semaphore));
}

void RemoteVideoFrameObjectHeapProxyProcessor::setSharedVideoFrameMemory(SharedMemory::Handle&& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(WTFMove(handle));
}

RemoteVideoFrameObjectHeapProxyProcessor::Callback RemoteVideoFrameObjectHeapProxyProcessor::takeCallback(RemoteVideoFrameIdentifier identifier)
{
    Locker lock(m_callbacksLock);
    return m_callbacks.take(identifier);
}

void RemoteVideoFrameObjectHeapProxyProcessor::newVideoFrameBuffer(RemoteVideoFrameIdentifier identifier, std::optional<SharedVideoFrame::Buffer>&& sharedVideoFrameBuffer)
{
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    if (sharedVideoFrameBuffer)
        pixelBuffer = m_sharedVideoFrameReader.readBuffer(WTFMove(*sharedVideoFrameBuffer));
    if (auto callback = takeCallback(identifier))
        callback(WTFMove(pixelBuffer));
}

void RemoteVideoFrameObjectHeapProxyProcessor::getVideoFrameBuffer(const RemoteVideoFrameProxy& frame, bool canUseIOSurface, Callback&& callback)
{
    {
        Locker lock(m_callbacksLock);
        ASSERT(!m_callbacks.contains(frame.identifier()));
        m_callbacks.add(frame.identifier(), WTFMove(callback));
    }
    RefPtr<IPC::Connection> connection;
    {
        Locker lock(m_connectionLock);
        connection = m_connection;
    }
    if (!connection) {
        takeCallback(frame.identifier())(nullptr);
        return;
    }
    connection->send(Messages::RemoteVideoFrameObjectHeap::GetVideoFrameBuffer(frame.newReadReference(), canUseIOSurface), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void RemoteVideoFrameObjectHeapProxyProcessor::newConvertedVideoFrameBuffer(std::optional<SharedVideoFrame::Buffer>&& buffer)
{
    ASSERT(!m_convertedBuffer);
    if (buffer)
        m_convertedBuffer = m_sharedVideoFrameReader.readBuffer(WTFMove(*buffer));
    m_conversionSemaphore.signal();
}

RefPtr<NativeImage> RemoteVideoFrameObjectHeapProxyProcessor::getNativeImage(const WebCore::VideoFrame& videoFrame)
{
    RefPtr connection = this->connection();
    if (!connection)
        return nullptr;

    if (m_sharedVideoFrameWriter.isDisabled())
        m_sharedVideoFrameWriter = { };
    auto frame = m_sharedVideoFrameWriter.write(videoFrame,
        [&](auto& semaphore) { connection->send(Messages::RemoteVideoFrameObjectHeap::SetSharedVideoFrameSemaphore { semaphore }, 0); },
        [&](auto&& handle) { connection->send(Messages::RemoteVideoFrameObjectHeap::SetSharedVideoFrameMemory { WTFMove(handle) }, 0); });
    if (!frame)
        return nullptr;

    auto sendResult = connection->sendSync(Messages::RemoteVideoFrameObjectHeap::ConvertFrameBuffer { WTFMove(*frame) }, 0, GPUProcessConnection::defaultTimeout);
    if (!sendResult.succeeded()) {
        m_sharedVideoFrameWriter.disable();
        return nullptr;
    }

    auto [destinationColorSpace] = sendResult.takeReplyOr(DestinationColorSpace { DestinationColorSpace::SRGB().platformColorSpace() });

    m_conversionSemaphore.wait();

    auto pixelBuffer = WTFMove(m_convertedBuffer);
    return pixelBuffer ? NativeImage::create(PixelBufferConformerCV::imageFrom32BGRAPixelBuffer(WTFMove(pixelBuffer), destinationColorSpace.protectedPlatformColorSpace().get())) : nullptr;
}

}

#endif
