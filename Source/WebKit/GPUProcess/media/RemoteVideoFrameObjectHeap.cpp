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
#include "ObjectIdentifierDomains.h"
#include "RemoteSharedResourceCache.h"
#include "RemoteVideoFrameObjectHeapMessages.h"
#include "RemoteVideoFrameObjectHeapProxyProcessorMessages.h"
#include "RemoteVideoFrameProxy.h"
#include <wtf/MainThread.h>
#include <wtf/Scope.h>
#include <wtf/WorkQueue.h>

#if PLATFORM(COCOA)
#include <WebCore/CMUtilities.h>
#include <WebCore/CVUtilities.h>
#include <WebCore/ColorSpaceCG.h>
#include <WebCore/ImageRotationSessionVT.h>
#include <WebCore/PixelBufferConformerCV.h>
#include <WebCore/VideoFrameCV.h>
#include <pal/cf/CoreMediaSoftLink.h>
#include <WebCore/CoreVideoSoftLink.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_connection.get())

namespace WebKit {

using namespace WebCore;

static WorkQueue& remoteVideoFrameObjectHeapQueueSingleton()
{
    static NeverDestroyed queue = WorkQueue::create("org.webkit.RemoteVideoFrameObjectHeap"_s, WorkQueue::QOS::UserInteractive);
    return queue.get();
}

Ref<RemoteVideoFrameObjectHeap> RemoteVideoFrameObjectHeap::create(Ref<IPC::Connection>&& connection, const ProcessIdentity& webProcessIdentity, RemoteSharedResourceCache& sharedResourceCache)
{
    Ref heap = adoptRef(*new RemoteVideoFrameObjectHeap(WTF::move(connection), webProcessIdentity, sharedResourceCache));
    protect(heap->m_connection)->addWorkQueueMessageReceiver(Messages::RemoteVideoFrameObjectHeap::messageReceiverName(), remoteVideoFrameObjectHeapQueueSingleton(), heap);
    return heap;
}

RemoteVideoFrameObjectHeap::RemoteVideoFrameObjectHeap(Ref<IPC::Connection>&& connection, const ProcessIdentity& webProcessIdentity, RemoteSharedResourceCache& sharedResourceCache)
    : m_connection(WTF::move(connection))
#if PLATFORM(COCOA)
    , m_sharedResourceCache(sharedResourceCache)
    // The reader resolves no frame references, so it does not hold the heap.
    , m_sharedVideoFrameReader(nullptr, webProcessIdentity, SharedVideoFrameReader::UseIOSurfaceBufferPool::Yes)
#endif
{
#if !PLATFORM(COCOA)
    UNUSED_PARAM(webProcessIdentity);
    UNUSED_PARAM(sharedResourceCache);
#endif
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

RemoteVideoFrameReference RemoteVideoFrameObjectHeap::add(Ref<WebCore::VideoFrame>&& frame)
{
    auto reference = RemoteVideoFrameReference::generateForAdd();
    add(reference, WTF::move(frame));
    return reference;
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
void RemoteVideoFrameObjectHeap::addVideoFrame(RemoteVideoFrameReference&& reference, SharedVideoFrame&& sharedVideoFrame)
{
    assertIsCurrent(remoteVideoFrameObjectHeapQueueSingleton());
    MESSAGE_CHECK(!isInGPUProcessObjectIdentifierDomain(reference.identifier().toUInt64()));
    MESSAGE_CHECK(!reference.version());

    RefPtr<VideoFrame> frame;
    if (auto* read = std::get_if<RemoteVideoFrameReadReference>(&sharedVideoFrame.buffer)) {
        // A frame derived from a frame in the heap: the same pixels, with the orientation and alpha of the Web process frame.
        if (RefPtr heapFrame = get(WTF::move(*read))) {
            Ref derivedFrame = heapFrame->updateOrientation(sharedVideoFrame.rotation, sharedVideoFrame.mirrored, VideoFrame::ShouldClone::Yes);
            auto shouldClone = derivedFrame.ptr() == heapFrame.get() ? VideoFrame::ShouldClone::Yes : VideoFrame::ShouldClone::No;
            frame = derivedFrame->updateShouldIgnoreAlpha(sharedVideoFrame.shouldIgnoreAlpha, shouldClone);
        }
    } else
        frame = m_sharedVideoFrameReader.read(WTF::move(sharedVideoFrame));

    // Add even a null frame, so that the consumers waiting for the reference get null instead of waiting forever.
    bool success = m_heap.add(reference, WTF::move(frame));
    MESSAGE_CHECK(success);
}

void RemoteVideoFrameObjectHeap::getVideoFrameBuffer(RemoteVideoFrameReadReference&& read)
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
            false);
        // FIXME: We should ASSERT(result) once we support enough pixel buffer types.
    }
    connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::NewVideoFrameBuffer { identifier, WTF::move(buffer) }, 0);
}

void RemoteVideoFrameObjectHeap::createNativeImage(RemoteVideoFrameReadReference&& read, RemoteNativeImageReference reference, ShouldApplyRotation shouldApplyRotation)
{
    assertIsCurrent(remoteVideoFrameObjectHeapQueueSingleton());
    MESSAGE_CHECK(!isInGPUProcessObjectIdentifierDomain(reference.identifier().toUInt64()));
    MESSAGE_CHECK(!reference.version());

    // The Web process sends the frame before this message on the same connection, so get() does not wait.
    RefPtr<NativeImage> image;
    if (RefPtr frame = get(WTF::move(read)))
        image = convertToNativeImage(*frame, shouldApplyRotation);

    // Publish even a null image, so that the readers get null instead of waiting.
    bool success = protect(m_sharedResourceCache)->addNativeImage(reference, WTF::move(image));
    MESSAGE_CHECK(success);
}

RefPtr<NativeImage> RemoteVideoFrameObjectHeap::convertToNativeImage(VideoFrame& frame, ShouldApplyRotation shouldApplyRotation)
{
    RetainPtr<CVPixelBufferRef> sourceBuffer = frame.pixelBuffer();
    if (!sourceBuffer)
        return nullptr;

    // The color space as VideoFrame::copyNativeImage() tags it. The Web process makes the same choice for the
    // RemoteNativeImageProxy when the frame describes its color space fully.
    RetainPtr<CGColorSpaceRef> colorSpace;
    const auto& frameColorSpace = frame.colorSpace();
    if (frameColorSpace.primaries && frameColorSpace.transfer)
        colorSpace = createCGColorSpace(frameColorSpace);
    else
        colorSpace = createCGColorSpaceForCVPixelBuffer(sourceBuffer.get());
    auto alphaInfo = frame.shouldIgnoreAlpha() ? kCGImageAlphaNoneSkipFirst : kCGImageAlphaFirst;

    RetainPtr buffer = sourceBuffer;
    // When requested, the native image is oriented and callers must not apply the rotation and mirroring of the frame again.
    if (shouldApplyRotation == ShouldApplyRotation::Yes && !frame.hasNoTransformation()) {
        RefPtr<VideoFrame> rotatedFrame;
        {
            Locker locker { m_pixelBufferConformerLock };
            if (!m_imageRotationSession)
                m_imageRotationSession = makeUnique<WebCore::ImageRotationSessionVT>();
            rotatedFrame = m_imageRotationSession->applyRotation(frame);
        }
        if (!rotatedFrame) {
            RELEASE_LOG_ERROR(WebRTC, "RemoteVideoFrameObjectHeap::convertToNativeImage rotation failed");
            return nullptr;
        }
        buffer = rotatedFrame->pixelBuffer();
    }

    if (CVPixelBufferGetPixelFormatType(buffer.get()) != kCVPixelFormatType_32BGRA) {
        Locker locker { m_pixelBufferConformerLock };
        if (!m_pixelBufferConformer)
            m_pixelBufferConformer = makeUnique<WebCore::PixelBufferConformerCV>(kCVPixelFormatType_32BGRA);
        buffer = m_pixelBufferConformer->convert(buffer.get());
        if (!buffer) {
            RELEASE_LOG_ERROR(WebRTC, "RemoteVideoFrameObjectHeap::convertToNativeImage conformer failed");
            return nullptr;
        }
    }

    return NativeImage::create(WTF::move(buffer), alphaInfo, WTF::move(colorSpace));
}

void RemoteVideoFrameObjectHeap::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTF::move(semaphore));
}

void RemoteVideoFrameObjectHeap::setSharedVideoFrameMemory(SharedMemory::Handle&& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(WTF::move(handle));
}

// Upper bound for waiting in get() for a frame under a Web process allocated reference. This is only
// an outer safety cap for a frame whose adding request is still in flight, not a steady-state wait.
static constexpr Seconds defaultTimeout = 10_s;

RefPtr<WebCore::VideoFrame> RemoteVideoFrameObjectHeap::get(RemoteVideoFrameReadReference&& read)
{
    // The GPU process adds a frame under a reference it allocates before it sends the reference anywhere,
    // so a miss for such a reference is final. The Web process allocates the references for the frames it
    // requests. The frame of a synchronous request is added before the Web process gets the reply, but an
    // asynchronous request can let the reference reach a consumer on another connection (e.g. WebGL
    // copyTextureFromVideoFrame on its own stream) before the request that adds the frame is handled here.
    auto timeout = isInGPUProcessObjectIdentifierDomain(read.identifier().toUInt64()) ? 0_s : defaultTimeout;
    return m_heap.read(WTF::move(read), timeout);
}

std::optional<RefPtr<WebCore::VideoFrame>> RemoteVideoFrameObjectHeap::read(IPC::StreamServerConnection& connection, RemoteVideoFrameReadReference&& read)
{
    return m_heap.read(connection, WTF::move(read));
}

#endif

void RemoteVideoFrameObjectHeap::lowMemoryHandler()
{
#if PLATFORM(COCOA)
    Locker locker { m_pixelBufferConformerLock };
    m_pixelBufferConformer = nullptr;
    m_imageRotationSession = nullptr;
#endif
}

}

#undef MESSAGE_CHECK

#endif
