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
#include <WebCore/MediaSample.h>
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
}

namespace WebKit {

class RemoteVideoFrameObjectHeap;

struct SharedVideoFrame {
    MediaTime time;
    bool mirrored { false };
    WebCore::MediaSample::VideoRotation rotation { WebCore::MediaSample::VideoRotation::None };
    std::variant<std::nullptr_t, RemoteVideoFrameReadReference, MachSendRight> buffer;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SharedVideoFrame> decode(Decoder&);
};

class SharedVideoFrameWriter {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SharedVideoFrameWriter();

    std::optional<SharedVideoFrame> write(WebCore::MediaSample&, const Function<void(IPC::Semaphore&)>&, const Function<void(const SharedMemory::IPCHandle&)>&);
    bool write(CVPixelBufferRef, const Function<void(IPC::Semaphore&)>&, const Function<void(const SharedMemory::IPCHandle&)>&);
#if USE(LIBWEBRTC)
    bool write(const webrtc::VideoFrame&, const Function<void(IPC::Semaphore&)>&, const Function<void(const SharedMemory::IPCHandle&)>&);
#endif
    void disable();

private:
    bool wait(const Function<void(IPC::Semaphore&)>&);
    bool allocateStorage(size_t, const Function<void(const SharedMemory::IPCHandle&)>&);
    bool prepareWriting(const WebCore::SharedVideoFrameInfo&, const Function<void(IPC::Semaphore&)>&, const Function<void(const SharedMemory::IPCHandle&)>&);

    UniqueRef<IPC::Semaphore> m_semaphore;
    RefPtr<SharedMemory> m_storage;
    bool m_isSemaphoreInUse { false };
    bool m_isDisabled { false };
};

class SharedVideoFrameReader {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class UseIOSurfaceBufferPool { No, Yes };
    explicit SharedVideoFrameReader(RefPtr<RemoteVideoFrameObjectHeap>&&, UseIOSurfaceBufferPool = UseIOSurfaceBufferPool::Yes);

    void setSemaphore(IPC::Semaphore&& semaphore) { m_semaphore = WTFMove(semaphore); }
    bool setSharedMemory(const SharedMemory::IPCHandle&);

    RetainPtr<CVPixelBufferRef> read();
    RefPtr<WebCore::MediaSample> read(SharedVideoFrame&&);

private:
    CVPixelBufferPoolRef pixelBufferPool(const WebCore::SharedVideoFrameInfo&);

    RefPtr<RemoteVideoFrameObjectHeap> m_objectHeap;
    UseIOSurfaceBufferPool m_useIOSurfaceBufferPool;
    IPC::Semaphore m_semaphore;
    RefPtr<SharedMemory> m_storage;

    RetainPtr<CVPixelBufferPoolRef> m_bufferPool;
    OSType m_bufferPoolType { 0 };
    uint32_t m_bufferPoolWidth { 0 };
    uint32_t m_bufferPoolHeight { 0 };
};

template<class Encoder> void SharedVideoFrame::encode(Encoder& encoder) const
{
    encoder << time;
    encoder << mirrored;
    encoder << rotation;

    switchOn(buffer,
    [&](std::nullptr_t representation) {
        encoder << (uint8_t)0;
    }, [&](const RemoteVideoFrameReadReference& reference) {
        encoder << (uint8_t)1;
        encoder << reference;
    } , [&](const MachSendRight& sendRight) {
        encoder << (uint8_t)2;
        encoder << sendRight;
    });
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

    uint8_t bufferType;
    if (!decoder.decode(bufferType))
        return { };

    if (bufferType > 2)
        return { };

    if (bufferType == 1) {
        std::optional<RemoteVideoFrameReadReference> reference;
        decoder >> reference;
        if (!reference)
            return { };
        frame.buffer = WTFMove(*reference);
    } else if (bufferType == 2) {
        MachSendRight sendRight;
        if (!decoder.decode(sendRight))
            return { };
        frame.buffer = WTFMove(sendRight);
    }
    return frame;
}

}

#endif
