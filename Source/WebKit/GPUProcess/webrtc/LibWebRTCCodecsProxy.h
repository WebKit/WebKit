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

#pragma once

#if USE(LIBWEBRTC) && PLATFORM(COCOA) && ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "DataReference.h"
#include "RemoteVideoFrameIdentifier.h"
#include "SharedMemory.h"
#include "SharedVideoFrame.h"
#include "VideoDecoderIdentifier.h"
#include "VideoEncoderIdentifier.h"
#include "VideoCodecType.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/ProcessIdentity.h>
#include <WebCore/WebRTCVideoDecoder.h>
#include <atomic>
#include <wtf/ThreadAssertions.h>

namespace IPC {
class Connection;
class Decoder;
class Semaphore;
}

namespace webrtc {
using LocalEncoder = void*;
}

namespace WebCore {
class PixelBufferConformerCV;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteVideoFrameObjectHeap;
struct SharedVideoFrame;
class SharedVideoFrameReader;

class LibWebRTCCodecsProxy final : public IPC::WorkQueueMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<LibWebRTCCodecsProxy> create(GPUConnectionToWebProcess&);
    ~LibWebRTCCodecsProxy();
    void stopListeningForIPC(Ref<LibWebRTCCodecsProxy>&& refFromConnection);
    bool allowsExitUnderMemoryPressure() const;

    class Decoder {
    public:
        virtual ~Decoder() = default;

        virtual void flush() = 0;
        virtual void setFormat(const uint8_t*, size_t, uint16_t width, uint16_t height) = 0;
        virtual int32_t decodeFrame(int64_t timeStamp, const uint8_t*, size_t) = 0;
        virtual void setFrameSize(uint16_t width, uint16_t height) = 0;
    };

private:
    explicit LibWebRTCCodecsProxy(GPUConnectionToWebProcess&);
    void initialize();
    auto createDecoderCallback(VideoDecoderIdentifier, bool useRemoteFrames);
    std::unique_ptr<WebCore::WebRTCVideoDecoder> createLocalDecoder(VideoDecoderIdentifier, VideoCodecType, bool useRemoteFrames);
    WorkQueue& workQueue() const { return m_queue; }

    // IPC::WorkQueueMessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void createDecoder(VideoDecoderIdentifier, VideoCodecType, bool useRemoteFrames);
    void releaseDecoder(VideoDecoderIdentifier);
    void flushDecoder(VideoDecoderIdentifier);
    void setDecoderFormatDescription(VideoDecoderIdentifier, const IPC::DataReference&, uint16_t width, uint16_t height);
    void decodeFrame(VideoDecoderIdentifier, int64_t timeStamp, const IPC::DataReference&);
    void setFrameSize(VideoDecoderIdentifier, uint16_t width, uint16_t height);

    void createEncoder(VideoEncoderIdentifier, VideoCodecType, const Vector<std::pair<String, String>>&, bool useLowLatency, bool useAnnexB);
    void releaseEncoder(VideoEncoderIdentifier);
    void initializeEncoder(VideoEncoderIdentifier, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate);
    void encodeFrame(VideoEncoderIdentifier, SharedVideoFrame&&, uint32_t timeStamp, bool shouldEncodeAsKeyFrame);
    void flushEncoder(VideoEncoderIdentifier);
    void setEncodeRates(VideoEncoderIdentifier, uint32_t bitRate, uint32_t frameRate);
    void setSharedVideoFrameSemaphore(VideoEncoderIdentifier, IPC::Semaphore&&);
    void setSharedVideoFrameMemory(VideoEncoderIdentifier, const SharedMemory::Handle&);
    void setRTCLoggingLevel(WTFLogLevel);

    struct Encoder {
        webrtc::LocalEncoder webrtcEncoder { nullptr };
        std::unique_ptr<SharedVideoFrameReader> frameReader;
    };
    Encoder* findEncoder(VideoEncoderIdentifier) WTF_REQUIRES_CAPABILITY(workQueue());

    Ref<IPC::Connection> m_connection;
    Ref<WorkQueue> m_queue;
    Ref<RemoteVideoFrameObjectHeap> m_videoFrameObjectHeap;
    WebCore::ProcessIdentity m_resourceOwner;
    HashMap<VideoDecoderIdentifier, UniqueRef<WebCore::WebRTCVideoDecoder>> m_decoders WTF_GUARDED_BY_CAPABILITY(workQueue());
    HashMap<VideoEncoderIdentifier, Encoder> m_encoders WTF_GUARDED_BY_CAPABILITY(workQueue());
    std::atomic<bool> m_hasEncodersOrDecoders { false };

    std::unique_ptr<WebCore::PixelBufferConformerCV> m_pixelBufferConformer;
};

}

#endif
