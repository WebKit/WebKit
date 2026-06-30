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

static WorkQueue& remoteVideoFrameObjectHeapQueueSingleton()
{
    static NeverDestroyed queue = WorkQueue::create("org.webkit.RemoteVideoFrameObjectHeap"_s, WorkQueue::QOS::UserInteractive);
    return queue.get();
}

// Upper bound for waiting on an asynchronously-inserted frame in get(). For accepted frames the
// insertion is already in flight, so this is only an outer safety cap, not a steady-state wait.
static constexpr Seconds defaultTimeout = 10_s;

Ref<RemoteVideoFrameObjectHeap> RemoteVideoFrameObjectHeap::create(Ref<IPC::Connection>&& connection)
{
    Ref heap = adoptRef(*new RemoteVideoFrameObjectHeap(WTF::move(connection)));
    protect(heap->m_connection)->addWorkQueueMessageReceiver(Messages::RemoteVideoFrameObjectHeap::messageReceiverName(), remoteVideoFrameObjectHeapQueueSingleton(), heap);
    return heap;
}

RemoteVideoFrameObjectHeap::RemoteVideoFrameObjectHeap(Ref<IPC::Connection>&& connection)
    : m_connection(WTF::move(connection))
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
    protect(m_connection)->removeWorkQueueMessageReceiver(Messages::RemoteVideoFrameObjectHeap::messageReceiverName());

#if PLATFORM(COCOA)
    m_sharedVideoFrameWriter.disable();
#endif

    // Clients might hold on to the ref after this happens. They should also stop themselves, but if they do not,
    // avoid big memory leaks by clearing the frames. The clients should fail gracefully (do nothing) in case they fail to look up
    // frames.
    m_heap.clear();
    // TODO: add can happen after stopping.
}

void RemoteVideoFrameObjectHeap::add(RemoteVideoFrameReference reference, Ref<WebCore::VideoFrame>&& frame)
{
    auto success = m_heap.add(reference, WTF::move(frame));
    ASSERT_UNUSED(success, success);
}

void RemoteVideoFrameObjectHeap::releaseVideoFrame(RemoteVideoFrameWriteReference&& write)
{
    assertIsCurrent(remoteVideoFrameObjectHeapQueueSingleton());
    m_heap.remove(WTF::move(write));
}

#if PLATFORM(COCOA)
void RemoteVideoFrameObjectHeap::getVideoFrameBuffer(RemoteVideoFrameReadReference&& read, bool canSendIOSurface)
{
    assertIsCurrent(remoteVideoFrameObjectHeapQueueSingleton());

    auto identifier = read.identifier();
    auto videoFrame = get(WTF::move(read));

    std::optional<SharedVideoFrame::Buffer> buffer;
    Ref connection = m_connection;

    if (videoFrame) {
        buffer = m_sharedVideoFrameWriter.writeBuffer(protect(videoFrame->pixelBuffer()).get(),
            [&](auto& semaphore) { connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::SetSharedVideoFrameSemaphore { semaphore }, 0); },
            [&](SharedMemory::Handle&& handle) { connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::SetSharedVideoFrameMemory { WTF::move(handle) }, 0); },
            canSendIOSurface);
        // FIXME: We should ASSERT(result) once we support enough pixel buffer types.
    }
    connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::NewVideoFrameBuffer { identifier, WTF::move(buffer) }, 0);
}

void RemoteVideoFrameObjectHeap::pixelBuffer(RemoteVideoFrameReadReference&& read, CompletionHandler<void(RetainPtr<CVPixelBufferRef>)>&& completionHandler)
{
    assertIsCurrent(remoteVideoFrameObjectHeapQueueSingleton());

    auto videoFrame = get(WTF::move(read));
    if (!videoFrame) {
        ASSERT_IS_TESTING_IPC();
        completionHandler(nullptr);
        return;
    }

    RetainPtr pixelBuffer = videoFrame->pixelBuffer();
    ASSERT(pixelBuffer);
    completionHandler(WTF::move(pixelBuffer));
}

void RemoteVideoFrameObjectHeap::convertFrameBuffer(SharedVideoFrame&& sharedVideoFrame, CompletionHandler<void(WebCore::DestinationColorSpace)>&& callback)
{
    DestinationColorSpace destinationColorSpace { DestinationColorSpace::SRGB().platformColorSpace() };
    auto scope = makeScopeExit([&callback, &destinationColorSpace] { callback(destinationColorSpace); });

    RefPtr<VideoFrame> frame;
    Ref connection = m_connection;

    if (std::holds_alternative<RemoteVideoFrameReadReference>(sharedVideoFrame.buffer))
        frame = get(WTF::move(std::get<RemoteVideoFrameReadReference>(sharedVideoFrame.buffer)));
    else
        frame = m_sharedVideoFrameReader.read(WTF::move(sharedVideoFrame));

    if (!frame) {
        connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::NewConvertedVideoFrameBuffer { { } }, 0);
        return;
    }

    RetainPtr<CVPixelBufferRef> buffer = frame->pixelBuffer();
    destinationColorSpace = DestinationColorSpace(createCGColorSpaceForCVPixelBuffer(buffer.get()));

    if (CVPixelBufferGetPixelFormatType(buffer.get()) != kCVPixelFormatType_32BGRA) {
        Locker locker { m_pixelBufferConformerLock };
        if (!m_pixelBufferConformer)
            m_pixelBufferConformer = makeUnique<WebCore::PixelBufferConformerCV>(kCVPixelFormatType_32BGRA);

        auto convertedBuffer = m_pixelBufferConformer->convert(buffer.get());
        if (!convertedBuffer) {
            RELEASE_LOG_ERROR(WebRTC, "RemoteVideoFrameObjectHeap::convertFrameBuffer conformer failed");
            connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::NewConvertedVideoFrameBuffer { { } }, 0);
            return;
        }
        buffer = WTF::move(convertedBuffer);
    }

    bool canSendIOSurface = false;
    auto result = m_sharedVideoFrameWriter.writeBuffer(buffer.get(),
        [&](auto& semaphore) { connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::SetSharedVideoFrameSemaphore { semaphore }, 0); },
        [&](auto&& handle) { connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::SetSharedVideoFrameMemory { WTF::move(handle) }, 0); },
        canSendIOSurface);
    connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::NewConvertedVideoFrameBuffer { WTF::move(result) }, 0);
}

void RemoteVideoFrameObjectHeap::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTF::move(semaphore));
}

void RemoteVideoFrameObjectHeap::setSharedVideoFrameMemory(SharedMemory::Handle&& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(WTF::move(handle));
}

RefPtr<WebCore::VideoFrame> RemoteVideoFrameObjectHeap::get(RemoteVideoFrameReadReference&& read)
{
    // A frame's heap insertion can complete asynchronously relative to a reader on another
    // connection: for GPU-process-originated frames the Web process allocates the identifier and
    // the matching add() runs only when the offer reply is handled here, while a consumer (e.g.
    // WebGL copyTextureFromVideoFrame on its own stream) may already hold the proxy and present the
    // read reference. Wait briefly for the insertion rather than failing immediately. This cannot
    // deadlock: a proxy only exists once the Web process handler has already sent the reply, so the
    // add() is always in flight on the (independent) connection dispatcher while we wait here.
    return m_heap.read(WTF::move(read), defaultTimeout);
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
