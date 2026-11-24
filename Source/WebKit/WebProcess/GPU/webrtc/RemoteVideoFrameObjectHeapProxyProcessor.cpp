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
#include <WebCore/CVUtilities.h>
#include <WebCore/NativeImage.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteVideoFrameObjectHeapProxyProcessor> RemoteVideoFrameObjectHeapProxyProcessor::create(GPUProcessConnection& connection)
{
    auto processor = adoptRef(*new RemoteVideoFrameObjectHeapProxyProcessor(connection));
    processor->initialize();
    return processor;
}

RemoteVideoFrameObjectHeapProxyProcessor::RemoteVideoFrameObjectHeapProxyProcessor(GPUProcessConnection& connection)
    : m_connection(&connection.connection())
    , m_queue(WorkQueue::create("RemoteVideoFrameObjectHeapProxy"_s, WorkQueue::QOS::UserInteractive))
{
    connection.addClient(*this);
}

RemoteVideoFrameObjectHeapProxyProcessor::~RemoteVideoFrameObjectHeapProxyProcessor()
{
    ASSERT(isMainRunLoop());
    clearCallbacks();
}

void RemoteVideoFrameObjectHeapProxyProcessor::initialize()
{
    RefPtr<IPC::Connection> connection;
    {
        Locker lock(m_connectionLock);
        connection = m_connection;
    }

    connection->addWorkQueueMessageReceiver(Messages::RemoteVideoFrameObjectHeapProxyProcessor::messageReceiverName(), m_queue, *this);
}

void RemoteVideoFrameObjectHeapProxyProcessor::gpuProcessConnectionDidClose(GPUProcessConnection& connection)
{
    disableSharedVideoFrameWriter();
    {
        Locker lock(m_connectionLock);
        m_connection = nullptr;
    }
    connection.connection().removeWorkQueueMessageReceiver(Messages::RemoteVideoFrameObjectHeapProxyProcessor::messageReceiverName());
    clearCallbacks();
}

void RemoteVideoFrameObjectHeapProxyProcessor::clearCallbacks()
{
    HashMap<RemoteVideoFrameIdentifier, Callback> callbacks;
    {
        Locker lock(m_callbacksLock);
        callbacks = std::exchange(m_callbacks, { });
    }

    m_queue->dispatch([callbacks = WTF::move(callbacks)]() mutable {
        for (auto& callback : callbacks.values())
            callback({ });
    });
}

void RemoteVideoFrameObjectHeapProxyProcessor::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTF::move(semaphore));
}

void RemoteVideoFrameObjectHeapProxyProcessor::setSharedVideoFrameMemory(SharedMemory::Handle&& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(WTF::move(handle));
}

RefPtr<IPC::Connection> RemoteVideoFrameObjectHeapProxyProcessor::connection()
{
    Locker lock(m_connectionLock);
    return m_connection;
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
        pixelBuffer = m_sharedVideoFrameReader.readBuffer(WTF::move(*sharedVideoFrameBuffer));
    if (auto callback = takeCallback(identifier))
        callback(WTF::move(pixelBuffer));
}

void RemoteVideoFrameObjectHeapProxyProcessor::getVideoFrameBuffer(const RemoteVideoFrameProxy& frame, Callback&& callback)
{
    {
        Locker lock(m_callbacksLock);
        ASSERT(!m_callbacks.contains(frame.identifier()));
        m_callbacks.add(frame.identifier(), WTF::move(callback));
    }
    RefPtr<IPC::Connection> connection;
    {
        Locker lock(m_connectionLock);
        connection = m_connection;
    }
    auto reference = connection ? frame.newReadReference(*connection) : std::nullopt;
    if (!reference) {
        takeCallback(frame.identifier())(nullptr);
        return;
    }
    connection->send(Messages::RemoteVideoFrameObjectHeap::GetVideoFrameBuffer(WTF::move(*reference)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void RemoteVideoFrameObjectHeapProxyProcessor::newConvertedVideoFrameBuffer(std::optional<SharedVideoFrame::Buffer>&& buffer)
{
    ASSERT(!m_convertedBuffer);
    if (buffer)
        m_convertedBuffer = m_sharedVideoFrameReader.readBuffer(WTF::move(*buffer));
    m_conversionSemaphore.signal();
}

RefPtr<NativeImage> RemoteVideoFrameObjectHeapProxyProcessor::getNativeImage(const WebCore::VideoFrame& videoFrame, ShouldApplyRotation shouldApplyRotation)
{
    Ref connection = WebProcess::singleton().ensureGPUProcessConnection().connection();

    std::optional<ColorSpace> destinationColorSpace;
    {
        Locker locker { m_sharedVideoFrameWriterLock };
        if (m_sharedVideoFrameWriter.isDisabled())
            return nullptr;

        auto frame = m_sharedVideoFrameWriter.write(videoFrame, connection,
            [&](auto& semaphore) {
                connection->send(Messages::RemoteVideoFrameObjectHeap::SetSharedVideoFrameSemaphore { semaphore }, 0);
            },
            [&](auto&& handle) {
                connection->send(Messages::RemoteVideoFrameObjectHeap::SetSharedVideoFrameMemory { WTF::move(handle) }, 0);
            });
        if (!frame)
            return nullptr;

        auto sendResult = connection->sendSync(Messages::RemoteVideoFrameObjectHeap::ConvertFrameBuffer { WTF::move(*frame), shouldApplyRotation == ShouldApplyRotation::Yes }, 0, GPUProcessConnection::defaultTimeout);
        if (!sendResult.succeeded()) {
            // Other threads upload frames with the same writer. Continue with a new writer, which does not wait for
            // the GPU process to read the last frame. Closing the connection disables the writer.
            m_sharedVideoFrameWriter = { };
            return nullptr;
        }
        auto [colorSpace] = sendResult.takeReplyOr(ColorSpace { ColorSpace::SRGB().platformColorSpace() });
        destinationColorSpace = WTF::move(colorSpace);
    }

    m_conversionSemaphore.wait();

    RetainPtr pixelBuffer = std::exchange(m_convertedBuffer, { });
    return pixelBuffer ? NativeImage::create(WTF::move(pixelBuffer), videoFrame.shouldIgnoreAlpha() ? kCGImageAlphaNoneSkipFirst : kCGImageAlphaFirst, destinationColorSpace->platformColorSpace()) : nullptr;
}

bool RemoteVideoFrameObjectHeapProxyProcessor::addVideoFrame(IPC::Connection& connection, RemoteVideoFrameReference reference, const VideoFrame& videoFrame)
{
    Locker locker { m_sharedVideoFrameWriterLock };
    if (m_sharedVideoFrameWriter.isDisabled())
        return false;

    auto frame = m_sharedVideoFrameWriter.write(videoFrame, connection,
        [&](auto& semaphore) {
            connection.send(Messages::RemoteVideoFrameObjectHeap::SetSharedVideoFrameSemaphore { semaphore }, 0);
        },
        [&](auto&& handle) {
            connection.send(Messages::RemoteVideoFrameObjectHeap::SetSharedVideoFrameMemory { WTF::move(handle) }, 0);
        });
    if (!frame)
        return false;

    if (connection.send(Messages::RemoteVideoFrameObjectHeap::AddVideoFrame { reference, WTF::move(*frame) }, 0) != IPC::Error::NoError) {
        m_sharedVideoFrameWriter.disable();
        return false;
    }
    return true;
}

RefPtr<RemoteVideoFrameProxy> RemoteVideoFrameObjectHeapProxyProcessor::remoteFrame(RemoteVideoFrameObjectHeapProxy& heapProxy, const VideoFrame& videoFrame)
{
    RefPtr connection = this->connection();
    if (!connection)
        return nullptr;

    if (RefPtr frameProxy = const_cast<RemoteVideoFrameProxy*>(dynamicDowncast<RemoteVideoFrameProxy>(&videoFrame))) {
        if (!frameProxy->belongsToConnection(*connection))
            return nullptr;
        if (frameProxy->matchesHeapFrame())
            return frameProxy;
        return frameProxy->derivedFrame([&] -> RefPtr<RemoteVideoFrameProxy> {
            auto read = frameProxy->newReadReference(*connection);
            if (!read)
                return nullptr;
            auto reference = RemoteVideoFrameReference::generateForAdd();
            SharedVideoFrame frame { videoFrame.presentationTime(), videoFrame.isMirrored(), videoFrame.rotation(), videoFrame.colorSpace(), videoFrame.shouldIgnoreAlpha(), WTF::move(*read) };
            if (connection->send(Messages::RemoteVideoFrameObjectHeap::AddVideoFrame { reference, WTF::move(frame) }, 0) != IPC::Error::NoError)
                return nullptr;
            return RemoteVideoFrameProxy::create(*connection, heapProxy, reference, RemoteVideoFrameProxy::properties(videoFrame));
        });
    }

    auto reference = RemoteVideoFrameReference::generateForAdd();
    if (!addVideoFrame(*connection, reference, videoFrame))
        return nullptr;
    Ref uploadedFrame = RemoteVideoFrameProxy::create(*connection, heapProxy, reference, RemoteVideoFrameProxy::properties(videoFrame));
    if (videoFrame.isCV())
        uploadedFrame->setLocalPixelBuffer(videoFrame.pixelBuffer());
    return uploadedFrame;
}

#if USE(LIBWEBRTC)
RefPtr<RemoteVideoFrameProxy> RemoteVideoFrameObjectHeapProxyProcessor::remoteFrame(RemoteVideoFrameObjectHeapProxy& heapProxy, const webrtc::VideoFrame& videoFrame, IntSize size)
{
    RefPtr connection = this->connection();
    if (!connection)
        return nullptr;

    auto reference = RemoteVideoFrameReference::generateForAdd();
    {
        Locker locker { m_sharedVideoFrameWriterLock };
        if (m_sharedVideoFrameWriter.isDisabled())
            return nullptr;

        auto buffer = m_sharedVideoFrameWriter.writeBuffer(videoFrame, *connection,
            [&](auto& semaphore) {
                connection->send(Messages::RemoteVideoFrameObjectHeap::SetSharedVideoFrameSemaphore { semaphore }, 0);
            },
            [&](auto&& handle) {
                connection->send(Messages::RemoteVideoFrameObjectHeap::SetSharedVideoFrameMemory { WTF::move(handle) }, 0);
            });
        if (!buffer)
            return nullptr;

        SharedVideoFrame frame { { }, false, VideoFrameRotation::None, { }, false, WTF::move(*buffer) };
        if (connection->send(Messages::RemoteVideoFrameObjectHeap::AddVideoFrame { reference, WTF::move(frame) }, 0) != IPC::Error::NoError) {
            m_sharedVideoFrameWriter.disable();
            return nullptr;
        }
    }
    return RemoteVideoFrameProxy::create(*connection, heapProxy, reference, RemoteVideoFrameProxy::Properties { { }, false, VideoFrameRotation::None, size, 0, { }, false });
}
#endif

RefPtr<RemoteVideoFrameProxy> RemoteVideoFrameObjectHeapProxyProcessor::remoteBlackFrame(RemoteVideoFrameObjectHeapProxy& heapProxy, MediaTime presentationTime, VideoFrameRotation rotation, IntSize size)
{
    RefPtr connection = this->connection();
    if (!connection)
        return nullptr;

    auto reference = RemoteVideoFrameReference::generateForAdd();
    SharedVideoFrame frame { presentationTime, false, rotation, { }, false, size };
    if (connection->send(Messages::RemoteVideoFrameObjectHeap::AddVideoFrame { reference, WTF::move(frame) }, 0) != IPC::Error::NoError)
        return nullptr;
    return RemoteVideoFrameProxy::create(*connection, heapProxy, reference, RemoteVideoFrameProxy::Properties { presentationTime, false, rotation, size, 0, { }, false });
}

}

#endif
