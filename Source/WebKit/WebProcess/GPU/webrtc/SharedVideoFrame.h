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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA) && ENABLE(VIDEO)

#include "IPCSemaphore.h"
#include "RemoteVideoFrameIdentifier.h"
#include <WebCore/IntSize.h>
#include <WebCore/PixelBufferConformerCV.h>
#include <WebCore/PlatformVideoColorSpace.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/SharedMemory.h>
#include <wtf/MediaTime.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

typedef struct CF_BRIDGED_TYPE(id) __CVBuffer* CVPixelBufferRef;
typedef struct __CVPixelBufferPool* CVPixelBufferPoolRef;

namespace WebCore {
class SharedVideoFrameInfo;
class VideoFrame;
enum class VideoFrameRotation : uint16_t;
}

namespace webrtc {
class VideoFrame;
class VideoFrameBuffer;
}

namespace WebKit {

class RemoteVideoFrameObjectHeap;

struct SharedVideoFrame {
    MediaTime time;
    bool mirrored { false };
    WebCore::VideoFrameRotation rotation { };
    WebCore::PlatformVideoColorSpace colorSpace;
    using Buffer = Variant<std::nullptr_t, RemoteVideoFrameReadReference, MachSendRight, WebCore::IntSize>;
    Buffer buffer;
};

class SharedVideoFrameWriter {
    WTF_MAKE_TZONE_ALLOCATED(SharedVideoFrameWriter);
public:
    SharedVideoFrameWriter();

    std::optional<SharedVideoFrame> write(const WebCore::VideoFrame&, NOESCAPE const Function<void(IPC::Semaphore&)>&, NOESCAPE const Function<void(WebCore::SharedMemory::Handle&&)>&);
    std::optional<SharedVideoFrame::Buffer> writeBuffer(CVPixelBufferRef, NOESCAPE const Function<void(IPC::Semaphore&)>&, NOESCAPE const Function<void(WebCore::SharedMemory::Handle&&)>&, bool canSendIOSurface = true);
#if USE(LIBWEBRTC)
    std::optional<SharedVideoFrame::Buffer> writeBuffer(const webrtc::VideoFrame&, NOESCAPE const Function<void(IPC::Semaphore&)>&, NOESCAPE const Function<void(WebCore::SharedMemory::Handle&&)>&);
#endif
    std::optional<SharedVideoFrame::Buffer> writeBuffer(const WebCore::VideoFrame&, NOESCAPE const Function<void(IPC::Semaphore&)>&, NOESCAPE const Function<void(WebCore::SharedMemory::Handle&&)>&);

    void disable();
    bool isDisabled() const { return m_isDisabled; }

private:
    static constexpr Seconds defaultTimeout = 3_s;

    bool wait(const Function<void(IPC::Semaphore&)>&);
    bool allocateStorage(size_t, NOESCAPE const Function<void(WebCore::SharedMemory::Handle&&)>&);
    bool prepareWriting(const WebCore::SharedVideoFrameInfo&, NOESCAPE const Function<void(IPC::Semaphore&)>&, NOESCAPE const Function<void(WebCore::SharedMemory::Handle&&)>&);

#if USE(LIBWEBRTC)
    std::optional<SharedVideoFrame::Buffer> writeBuffer(webrtc::VideoFrameBuffer&, NOESCAPE const Function<void(IPC::Semaphore&)>&, NOESCAPE const Function<void(WebCore::SharedMemory::Handle&&)>&);
#endif
    void signalInCaseOfError();

    UniqueRef<IPC::Semaphore> m_semaphore;
    RefPtr<WebCore::SharedMemory> m_storage;
    std::unique_ptr<WebCore::PixelBufferConformerCV> m_compressedPixelBufferConformer;
    bool m_isSemaphoreInUse { false };
    bool m_isDisabled { false };
    bool m_shouldSignalInCaseOfError { false };
};

class SharedVideoFrameReader {
    WTF_MAKE_TZONE_ALLOCATED(SharedVideoFrameReader);
public:
    ~SharedVideoFrameReader();

    enum class UseIOSurfaceBufferPool : bool { No, Yes };
    explicit SharedVideoFrameReader(RefPtr<RemoteVideoFrameObjectHeap>&&, const WebCore::ProcessIdentity& = { }, UseIOSurfaceBufferPool = UseIOSurfaceBufferPool::Yes);
    SharedVideoFrameReader();

    void setSemaphore(IPC::Semaphore&& semaphore) { m_semaphore = WTF::move(semaphore); }
    bool setSharedMemory(WebCore::SharedMemory::Handle&&);

    RefPtr<WebCore::VideoFrame> read(SharedVideoFrame&&);
    RetainPtr<CVPixelBufferRef> readBuffer(SharedVideoFrame::Buffer&&);

private:
    CVPixelBufferPoolRef pixelBufferPool(const WebCore::SharedVideoFrameInfo&);
    RetainPtr<CVPixelBufferRef> readBufferFromSharedMemory();

    const RefPtr<RemoteVideoFrameObjectHeap> m_objectHeap;
    WebCore::ProcessIdentity m_resourceOwner;
    UseIOSurfaceBufferPool m_useIOSurfaceBufferPool { UseIOSurfaceBufferPool::No };
    std::optional<IPC::Semaphore> m_semaphore;
    RefPtr<WebCore::SharedMemory> m_storage;

    RetainPtr<CVPixelBufferPoolRef> m_bufferPool;
    OSType m_bufferPoolType { 0 };
    uint32_t m_bufferPoolWidth { 0 };
    uint32_t m_bufferPoolHeight { 0 };
    WebCore::IntSize m_blackFrameSize;
    RetainPtr<CVPixelBufferRef> m_blackFrame;
};

}

#endif
