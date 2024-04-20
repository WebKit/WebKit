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
#include "RemoteVideoFrameObjectHeap.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
#include "Logging.h"
#include "RemoteVideoFrameObjectHeapMessages.h"
#include "RemoteVideoFrameObjectHeapProxyProcessorMessages.h"
#include "RemoteVideoFrameProxy.h"
#include <wtf/MainThread.h>
#include <wtf/Scope.h>
#include <wtf/WorkQueue.h>

#if PLATFORM(COCOA)
#include <WebCore/ColorSpaceCG.h>
#include <WebCore/PixelBufferConformerCV.h>
#include <WebCore/VideoFrameCV.h>
#include <pal/cf/CoreMediaSoftLink.h>
#include <WebCore/CoreVideoSoftLink.h>
#endif

namespace WebKit {

using namespace WebCore;

static WorkQueue& remoteVideoFrameObjectHeapQueue()
{
    static NeverDestroyed queue = WorkQueue::create("org.webkit.RemoteVideoFrameObjectHeap"_s, WorkQueue::QOS::UserInteractive);
    return queue.get();
}

Ref<RemoteVideoFrameObjectHeap> RemoteVideoFrameObjectHeap::create(Ref<IPC::Connection>&& connection)
{
    auto heap = adoptRef(*new RemoteVideoFrameObjectHeap(WTFMove(connection)));
    heap->m_connection->addWorkQueueMessageReceiver(Messages::RemoteVideoFrameObjectHeap::messageReceiverName(), remoteVideoFrameObjectHeapQueue(), heap);
    return heap;
}

RemoteVideoFrameObjectHeap::RemoteVideoFrameObjectHeap(Ref<IPC::Connection>&& connection)
    : m_connection(WTFMove(connection))
{
}

RemoteVideoFrameObjectHeap::~RemoteVideoFrameObjectHeap()
{
    ASSERT(m_isClosed);
}

void RemoteVideoFrameObjectHeap::close()
{
    assertIsMainThread();

    if (m_isClosed)
        return;

    m_isClosed = true;
    m_connection->removeWorkQueueMessageReceiver(Messages::RemoteVideoFrameObjectHeap::messageReceiverName());

#if PLATFORM(COCOA)
    m_sharedVideoFrameWriter.disable();
#endif

    // Clients might hold on to the ref after this happens. They should also stop themselves, but if they do not,
    // avoid big memory leaks by clearing the frames. The clients should fail gracefully (do nothing) in case they fail to look up
    // frames.
    m_heap.clear();
    // TODO: add can happen after stopping.
}

RemoteVideoFrameProxy::Properties RemoteVideoFrameObjectHeap::add(Ref<WebCore::VideoFrame>&& frame)
{
    auto newFrameReference = RemoteVideoFrameReference::generateForAdd();
    auto properties = RemoteVideoFrameProxy::properties(newFrameReference, frame);
    auto success = m_heap.add(newFrameReference, WTFMove(frame));
    ASSERT_UNUSED(success, success);
    return properties;
}

void RemoteVideoFrameObjectHeap::releaseVideoFrame(RemoteVideoFrameWriteReference&& write)
{
    assertIsCurrent(remoteVideoFrameObjectHeapQueue());
    m_heap.remove(WTFMove(write));
}

#if PLATFORM(COCOA)
void RemoteVideoFrameObjectHeap::getVideoFrameBuffer(RemoteVideoFrameReadReference&& read, bool canSendIOSurface)
{
    assertIsCurrent(remoteVideoFrameObjectHeapQueue());

    auto identifier = read.identifier();
    auto videoFrame = get(WTFMove(read));

    std::optional<SharedVideoFrame::Buffer> buffer;
    if (videoFrame) {
        buffer = m_sharedVideoFrameWriter.writeBuffer(videoFrame->pixelBuffer(),
            [&](auto& semaphore) { m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::SetSharedVideoFrameSemaphore { semaphore }, 0); },
            [&](SharedMemory::Handle&& handle) { m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::SetSharedVideoFrameMemory { WTFMove(handle) }, 0); },
            canSendIOSurface);
        // FIXME: We should ASSERT(result) once we support enough pixel buffer types.
    }
    m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::NewVideoFrameBuffer { identifier, WTFMove(buffer) }, 0);
}

void RemoteVideoFrameObjectHeap::pixelBuffer(RemoteVideoFrameReadReference&& read, CompletionHandler<void(RetainPtr<CVPixelBufferRef>)>&& completionHandler)
{
    assertIsCurrent(remoteVideoFrameObjectHeapQueue());

    auto videoFrame = get(WTFMove(read));
    if (!videoFrame) {
        ASSERT_IS_TESTING_IPC();
        completionHandler(nullptr);
        return;
    }

    auto pixelBuffer = videoFrame->pixelBuffer();
    ASSERT(pixelBuffer);
    completionHandler(WTFMove(pixelBuffer));
}

void RemoteVideoFrameObjectHeap::convertFrameBuffer(SharedVideoFrame&& sharedVideoFrame, CompletionHandler<void(WebCore::DestinationColorSpace)>&& callback)
{
    DestinationColorSpace destinationColorSpace { DestinationColorSpace::SRGB().platformColorSpace() };
    auto scope = makeScopeExit([&callback, &destinationColorSpace] { callback(destinationColorSpace); });

    RefPtr<VideoFrame> frame;
    if (std::holds_alternative<RemoteVideoFrameReadReference>(sharedVideoFrame.buffer))
        frame = get(WTFMove(std::get<RemoteVideoFrameReadReference>(sharedVideoFrame.buffer)));
    else
        frame = m_sharedVideoFrameReader.read(WTFMove(sharedVideoFrame));

    if (!frame) {
        m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::NewConvertedVideoFrameBuffer { { } }, 0);
        return;
    }

    RetainPtr<CVPixelBufferRef> buffer = frame->pixelBuffer();
    destinationColorSpace = DestinationColorSpace(createCGColorSpaceForCVPixelBuffer(buffer.get()));

    if (CVPixelBufferGetPixelFormatType(buffer.get()) != kCVPixelFormatType_32BGRA) {
        Locker locker { m_pixelBufferConformerLock };
        if (!m_pixelBufferConformer)
            m_pixelBufferConformer = createPixelConformer().moveToUniquePtr();

        auto convertedBuffer = m_pixelBufferConformer->convert(buffer.get());
        if (!convertedBuffer) {
            RELEASE_LOG_ERROR(WebRTC, "RemoteVideoFrameObjectHeap::convertFrameBuffer conformer failed");
            m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::NewConvertedVideoFrameBuffer { { } }, 0);
            return;
        }
        buffer = WTFMove(convertedBuffer);
    }

    bool canSendIOSurface = false;
    auto result = m_sharedVideoFrameWriter.writeBuffer(buffer.get(),
        [&](auto& semaphore) { m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::SetSharedVideoFrameSemaphore { semaphore }, 0); },
        [&](auto&& handle) { m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::SetSharedVideoFrameMemory { WTFMove(handle) }, 0); },
        canSendIOSurface);
    m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::NewConvertedVideoFrameBuffer { WTFMove(result) }, 0);
}

void RemoteVideoFrameObjectHeap::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTFMove(semaphore));
}

void RemoteVideoFrameObjectHeap::setSharedVideoFrameMemory(SharedMemory::Handle&& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(WTFMove(handle));
}

#endif

void RemoteVideoFrameObjectHeap::lowMemoryHandler()
{
#if PLATFORM(COCOA)
    Locker locker { m_pixelBufferConformerLock };
    m_pixelBufferConformer = nullptr;
#endif
}

}

#endif
