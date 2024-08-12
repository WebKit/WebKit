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
#include "RemoteVideoFrameIdentifier.h"
#include "SharedVideoFrame.h"
#include "VideoDecoderIdentifier.h"
#include "VideoEncoderIdentifier.h"
#include "VideoCodecType.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/ProcessIdentity.h>
#include <WebCore/SharedMemory.h>
#include <WebCore/VideoEncoderScalabilityMode.h>
#include <WebCore/WebRTCVideoDecoder.h>
#include <atomic>
#include <wtf/TZoneMalloc.h>
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
class FrameRateMonitor;
class PixelBufferConformerCV;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteVideoFrameObjectHeap;
struct SharedVideoFrame;
class SharedVideoFrameReader;

class LibWebRTCCodecsProxy final : public IPC::WorkQueueMessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(LibWebRTCCodecsProxy);
public:
    static Ref<LibWebRTCCodecsProxy> create(GPUConnectionToWebProcess&);
    ~LibWebRTCCodecsProxy();
    void stopListeningForIPC(Ref<LibWebRTCCodecsProxy>&& refFromConnection);
    bool allowsExitUnderMemoryPressure() const;

private:
private:
    explicit LibWebRTCCodecsProxy(GPUConnectionToWebProcess&);
    void initialize();
    auto createDecoderCallback(VideoDecoderIdentifier, bool useRemoteFrames, bool enableAdditionalLogging);
    std::unique_ptr<WebCore::WebRTCVideoDecoder> createLocalDecoder(VideoDecoderIdentifier, VideoCodecType, bool useRemoteFrames, bool enableAdditionalLogging);
    WorkQueue& workQueue() const { return m_queue; }

    // IPC::WorkQueueMessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void createDecoder(VideoDecoderIdentifier, VideoCodecType, const String& codecString, bool useRemoteFrames, bool enableAdditionalLogging, CompletionHandler<void(bool)>&&);
    void releaseDecoder(VideoDecoderIdentifier);
    void flushDecoder(VideoDecoderIdentifier);
    void setDecoderFormatDescription(VideoDecoderIdentifier, std::span<const uint8_t>, uint16_t width, uint16_t height);
    void decodeFrame(VideoDecoderIdentifier, int64_t timeStamp, std::span<const uint8_t>);
    void setFrameSize(VideoDecoderIdentifier, uint16_t width, uint16_t height);

    void createEncoder(VideoEncoderIdentifier, VideoCodecType, const String& codecString, const Vector<std::pair<String, String>>&, bool useLowLatency, bool useAnnexB, WebCore::VideoEncoderScalabilityMode, CompletionHandler<void(bool)>&&);
    void releaseEncoder(VideoEncoderIdentifier);
    void initializeEncoder(VideoEncoderIdentifier, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate);
    void encodeFrame(VideoEncoderIdentifier, SharedVideoFrame&&, int64_t timeStamp, std::optional<uint64_t> duration, bool shouldEncodeAsKeyFrame, CompletionHandler<void(bool)>&&);
    void flushEncoder(VideoEncoderIdentifier, CompletionHandler<void()>&&);
    void setEncodeRates(VideoEncoderIdentifier, uint32_t bitRate, uint32_t frameRate);
    void setSharedVideoFrameSemaphore(VideoEncoderIdentifier, IPC::Semaphore&&);
    void setSharedVideoFrameMemory(VideoEncoderIdentifier, WebCore::SharedMemory::Handle&&);
    void setRTCLoggingLevel(WTFLogLevel);

    void notifyEncoderResult(VideoEncoderIdentifier, bool);

    struct Decoder {
        std::unique_ptr<WebCore::WebRTCVideoDecoder> webrtcDecoder;
        std::unique_ptr<WebCore::FrameRateMonitor> frameRateMonitor;
    };
    void doDecoderTask(VideoDecoderIdentifier, Function<void(Decoder&)>&&);

    struct Encoder {
        webrtc::LocalEncoder webrtcEncoder { nullptr };
        std::unique_ptr<SharedVideoFrameReader> frameReader;
        Deque<CompletionHandler<void(bool)>> encodingCallbacks;
    };
    Encoder* findEncoder(VideoEncoderIdentifier) WTF_REQUIRES_CAPABILITY(workQueue());

    Ref<IPC::Connection> m_connection;
    Ref<WorkQueue> m_queue;
    Ref<RemoteVideoFrameObjectHeap> m_videoFrameObjectHeap;
    WebCore::ProcessIdentity m_resourceOwner;
    HashMap<VideoDecoderIdentifier, Decoder> m_decoders WTF_GUARDED_BY_CAPABILITY(workQueue());
    HashMap<VideoEncoderIdentifier, Encoder> m_encoders WTF_GUARDED_BY_CAPABILITY(workQueue());
    std::atomic<bool> m_hasEncodersOrDecoders { false };

    std::unique_ptr<WebCore::PixelBufferConformerCV> m_pixelBufferConformer;
};

}

#endif
