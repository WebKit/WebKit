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
#include "SharedMemory.h"
#include <WebCore/ProcessIdentity.h>
#include <WebCore/VideoFrame.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/UniqueRef.h>

typedef struct __CVBuffer* CVPixelBufferRef;
typedef struct __CVPixelBufferPool* CVPixelBufferPoolRef;

namespace WebCore {
class SharedVideoFrameInfo;
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
    WebCore::VideoFrame::Rotation rotation { WebCore::VideoFrame::Rotation::None };
    using Buffer = std::variant<std::nullptr_t, RemoteVideoFrameReadReference, MachSendRight, WebCore::IntSize>;
    Buffer buffer;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SharedVideoFrame> decode(Decoder&);
};

class SharedVideoFrameWriter {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SharedVideoFrameWriter();

    std::optional<SharedVideoFrame> write(const WebCore::VideoFrame&, const Function<void(IPC::Semaphore&)>&, const Function<void(const SharedMemory::Handle&)>&);
    std::optional<SharedVideoFrame::Buffer> writeBuffer(CVPixelBufferRef, const Function<void(IPC::Semaphore&)>&, const Function<void(const SharedMemory::Handle&)>&, bool canSendIOSurface = true);
#if USE(LIBWEBRTC)
    std::optional<SharedVideoFrame::Buffer> writeBuffer(const webrtc::VideoFrame&, const Function<void(IPC::Semaphore&)>&, const Function<void(const SharedMemory::Handle&)>&);
#endif
    std::optional<SharedVideoFrame::Buffer> writeBuffer(const WebCore::VideoFrame&, const Function<void(IPC::Semaphore&)>&, const Function<void(const SharedMemory::Handle&)>&);

    void disable();
    bool isDisabled() const { return m_isDisabled; }

private:
    static constexpr Seconds defaultTimeout = 3_s;

    bool wait(const Function<void(IPC::Semaphore&)>&);
    bool allocateStorage(size_t, const Function<void(const SharedMemory::Handle&)>&);
    bool prepareWriting(const WebCore::SharedVideoFrameInfo&, const Function<void(IPC::Semaphore&)>&, const Function<void(const SharedMemory::Handle&)>&);

#if USE(LIBWEBRTC)
    std::optional<SharedVideoFrame::Buffer> writeBuffer(webrtc::VideoFrameBuffer&, const Function<void(IPC::Semaphore&)>&, const Function<void(const SharedMemory::Handle&)>&);
#endif
    void signalInCaseOfError();

    UniqueRef<IPC::Semaphore> m_semaphore;
    RefPtr<SharedMemory> m_storage;
    bool m_isSemaphoreInUse { false };
    bool m_isDisabled { false };
    bool m_shouldSignalInCaseOfError { false };
};

class SharedVideoFrameReader {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~SharedVideoFrameReader();

    enum class UseIOSurfaceBufferPool { No, Yes };
    explicit SharedVideoFrameReader(RefPtr<RemoteVideoFrameObjectHeap>&&, const WebCore::ProcessIdentity& = { }, UseIOSurfaceBufferPool = UseIOSurfaceBufferPool::Yes);
    SharedVideoFrameReader();

    void setSemaphore(IPC::Semaphore&& semaphore) { m_semaphore = WTFMove(semaphore); }
    bool setSharedMemory(const SharedMemory::Handle&);

    RefPtr<WebCore::VideoFrame> read(SharedVideoFrame&&);
    RetainPtr<CVPixelBufferRef> readBuffer(SharedVideoFrame::Buffer&&);

private:
    CVPixelBufferPoolRef pixelBufferPool(const WebCore::SharedVideoFrameInfo&);
    RetainPtr<CVPixelBufferRef> readBufferFromSharedMemory();

    RefPtr<RemoteVideoFrameObjectHeap> m_objectHeap;
    WebCore::ProcessIdentity m_resourceOwner;
    UseIOSurfaceBufferPool m_useIOSurfaceBufferPool { UseIOSurfaceBufferPool::No };
    IPC::Semaphore m_semaphore;
    RefPtr<SharedMemory> m_storage;

    RetainPtr<CVPixelBufferPoolRef> m_bufferPool;
    OSType m_bufferPoolType { 0 };
    uint32_t m_bufferPoolWidth { 0 };
    uint32_t m_bufferPoolHeight { 0 };
    WebCore::IntSize m_blackFrameSize;
    RetainPtr<CVPixelBufferRef> m_blackFrame;
};

template<class Encoder> void SharedVideoFrame::encode(Encoder& encoder) const
{
    encoder << time;
    encoder << mirrored;
    encoder << rotation;
    encoder << buffer;
}

template<class Decoder> std::optional<SharedVideoFrame> SharedVideoFrame::decode(Decoder& decoder)
{
    SharedVideoFrame frame;
    if (!decoder.decode(frame.time))
        return { };

    if (!decoder.decode(frame.mirrored))
        return { };

    if (!decoder.decode(frame.rotation))
        return { };

    if (!decoder.decode(frame.buffer))
        return { };

    return frame;
}

}

#endif
