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
#include "GPUProcessConnection.h"
#include "IPCSemaphore.h"
#include "MessageReceiver.h"
#include "RemoteVideoFrameIdentifier.h"
#include "RemoteVideoFrameProxy.h"
#include "SharedVideoFrame.h"
#include "VideoCodecType.h"
#include "VideoDecoderIdentifier.h"
#include "VideoEncoderIdentifier.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/VideoEncoder.h>
#include <WebCore/VideoEncoderScalabilityMode.h>
#include <map>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>

using CVPixelBufferPoolRef = struct __CVPixelBufferPool*;

namespace IPC {
class Connection;
class Decoder;
}

namespace webrtc {
class VideoFrame;
struct WebKitEncodedFrameInfo;
}

namespace WebCore {
enum class VideoFrameRotation : uint16_t;
struct VideoEncoderActiveConfiguration;
}

namespace WebKit {

class RemoteVideoFrameObjectHeapProxy;

class LibWebRTCCodecs : public IPC::WorkQueueMessageReceiver, public GPUProcessConnection::Client {
    WTF_MAKE_TZONE_ALLOCATED(LibWebRTCCodecs);
public:
    static Ref<LibWebRTCCodecs> create();
    ~LibWebRTCCodecs();

    static void setCallbacks(bool useGPUProcess, bool useRemoteFrames);
    static void setWebRTCMediaPipelineAdditionalLoggingEnabled(bool);
    static void initializeIfNeeded();

    std::optional<VideoCodecType> videoCodecTypeFromWebCodec(const String&);
    std::optional<VideoCodecType> videoEncoderTypeFromWebCodec(const String&);

    using DecoderCallback = Function<void(RefPtr<WebCore::VideoFrame>&&, int64_t timestamp)>;
    struct Decoder {
        WTF_MAKE_TZONE_ALLOCATED(Decoder);
    public:
        struct EncodedFrame {
            int64_t timeStamp { 0 };
            Vector<uint8_t> data;
            uint16_t width { 0 };
            uint16_t height { 0 };
        };

        VideoDecoderIdentifier identifier;
        VideoCodecType type;
        String codec;
        void* decodedImageCallback WTF_GUARDED_BY_LOCK(decodedImageCallbackLock) { nullptr };
        DecoderCallback decoderCallback;
        Lock decodedImageCallbackLock;
        bool hasError { false };
        RefPtr<IPC::Connection> connection;
        Vector<EncodedFrame> pendingFrames;
        Deque<Function<void()>> flushCallbacks WTF_GUARDED_BY_LOCK(flushCallbacksLock);
        Lock flushCallbacksLock;
    };

    Decoder* createDecoder(VideoCodecType);
    void createDecoderAndWaitUntilReady(VideoCodecType, const String& codec, Function<void(Decoder*)>&&);

    int32_t releaseDecoder(Decoder&);
    void flushDecoder(Decoder&, Function<void()>&&);
    void setDecoderFormatDescription(Decoder&, std::span<const uint8_t>, uint16_t width, uint16_t height);
    int32_t decodeFrame(Decoder&, int64_t timeStamp, std::span<const uint8_t>, uint16_t width, uint16_t height);
    void registerDecodeFrameCallback(Decoder&, void* decodedImageCallback);
    void registerDecodedVideoFrameCallback(Decoder&, DecoderCallback&&);

#if ENABLE(WEB_CODECS)
    using DescriptionCallback = Function<void(WebCore::VideoEncoderActiveConfiguration&&)>;
#endif
    using EncoderCallback = Function<void(std::span<const uint8_t>, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration, std::optional<unsigned> temporalIndex)>;
    struct EncoderInitializationData {
        uint16_t width;
        uint16_t height;
        unsigned startBitRate;
        unsigned maxBitRate;
        unsigned minBitRate;
        uint32_t maxFrameRate;
    };
    struct Encoder {
        WTF_MAKE_TZONE_ALLOCATED(Encoder);
    public:
        VideoEncoderIdentifier identifier;
        VideoCodecType type;
        String codec;
        Vector<std::pair<String, String>> parameters;
        std::optional<EncoderInitializationData> initializationData;
        void* encodedImageCallback WTF_GUARDED_BY_LOCK(encodedImageCallbackLock) { nullptr };
        EncoderCallback encoderCallback;
#if ENABLE(WEB_CODECS)
        DescriptionCallback descriptionCallback;
#endif
        Lock encodedImageCallbackLock;
        RefPtr<IPC::Connection> connection;
        SharedVideoFrameWriter sharedVideoFrameWriter;
        bool hasSentInitialEncodeRates { false };
        bool useAnnexB { true };
        bool isRealtime { true };
        WebCore::VideoEncoderScalabilityMode scalabilityMode { WebCore::VideoEncoderScalabilityMode::L1T1 };
    };

    Encoder* createEncoder(VideoCodecType, const std::map<std::string, std::string>&);
#if ENABLE(WEB_CODECS)
    void createEncoderAndWaitUntilInitialized(VideoCodecType, const String& codec, const std::map<std::string, std::string>&, const WebCore::VideoEncoder::Config&, Function<void(Encoder*)>&&);
#endif
    int32_t releaseEncoder(Encoder&);
    int32_t initializeEncoder(Encoder&, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate);
    int32_t encodeFrame(Encoder&, const WebCore::VideoFrame&, int64_t timestamp, std::optional<uint64_t> duration, bool shouldEncodeAsKeyFrame, Function<void(bool)>&&);
    int32_t encodeFrame(Encoder&, const webrtc::VideoFrame&, bool shouldEncodeAsKeyFrame);
    void flushEncoder(Encoder&, Function<void()>&&);
    void registerEncodeFrameCallback(Encoder&, void* encodedImageCallback);
    void registerEncodedVideoFrameCallback(Encoder&, EncoderCallback&&);
#if ENABLE(WEB_CODECS)
    void registerEncoderDescriptionCallback(Encoder&, DescriptionCallback&&);
#endif
    void setEncodeRates(Encoder&, uint32_t bitRateInKbps, uint32_t frameRate);

    CVPixelBufferPoolRef pixelBufferPool(size_t width, size_t height, OSType);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void setVP9HardwareSupportForTesting(std::optional<bool> value) { m_vp9HardwareSupportForTesting = value; }
    void setVP9VTBSupport(bool isSupportingVP9HardwareDecoder) { m_isSupportingVP9HardwareDecoder = isSupportingVP9HardwareDecoder; }
    bool isSupportingVP9HardwareDecoder() const { return m_vp9HardwareSupportForTesting.value_or(m_isSupportingVP9HardwareDecoder); }
    void setLoggingLevel(WTFLogLevel);

#if ENABLE(AV1)
    void setHasAV1HardwareDecoder(bool);
#endif
    bool hasAV1HardwareDecoder() const { return m_hasAV1HardwareDecoder; }

    void ref() const final { return IPC::WorkQueueMessageReceiver::ref(); }
    void deref() const final { return IPC::WorkQueueMessageReceiver::deref(); }
    ThreadSafeWeakPtrControlBlock& controlBlock() const final { return IPC::WorkQueueMessageReceiver::controlBlock(); }

    WorkQueue& workQueue() const { return m_queue; }

private:
    LibWebRTCCodecs();
    void ensureGPUProcessConnectionAndDispatchToThread(Function<void()>&&);
    void ensureGPUProcessConnectionOnMainThreadWithLock() WTF_REQUIRES_LOCK(m_connectionLock);
    void gpuProcessConnectionMayNoLongerBeNeeded();

    void failedDecoding(VideoDecoderIdentifier);
    void flushDecoderCompleted(VideoDecoderIdentifier);
    void completedDecoding(VideoDecoderIdentifier, int64_t timeStamp, int64_t timeStampNs, RemoteVideoFrameProxy::Properties&&);
    // FIXME: Will be removed once RemoteVideoFrameProxy providers are the only ones sending data.
    void completedDecodingCV(VideoDecoderIdentifier, int64_t timeStamp, int64_t timeStampNs, RetainPtr<CVPixelBufferRef>&&);
    void completedEncoding(VideoEncoderIdentifier, std::span<const uint8_t>, const webrtc::WebKitEncodedFrameInfo&);
    void flushEncoderCompleted(VideoEncoderIdentifier);
    void setEncodingConfiguration(WebKit::VideoEncoderIdentifier, std::span<const uint8_t>, std::optional<WebCore::PlatformVideoColorSpace>);
    RetainPtr<CVPixelBufferRef> convertToBGRA(CVPixelBufferRef);

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&);

    IPC::Connection* encoderConnection(Encoder&) WTF_REQUIRES_LOCK(m_encodersConnectionLock);
    void setEncoderConnection(Encoder&, RefPtr<IPC::Connection>&&) WTF_REQUIRES_LOCK(m_encodersConnectionLock);
    IPC::Connection* decoderConnection(Decoder&) WTF_REQUIRES_LOCK(m_connectionLock);
    void setDecoderConnection(Decoder&, RefPtr<IPC::Connection>&&) WTF_REQUIRES_LOCK(m_connectionLock);

    template<typename Buffer> bool copySharedVideoFrame(LibWebRTCCodecs::Encoder&, IPC::Connection&, Buffer&&);

    Decoder* createDecoderInternal(VideoCodecType, const String& codec, Function<void(Decoder(*))>&&);
    Encoder* createEncoderInternal(VideoCodecType, const String& codec, const std::map<std::string, std::string>&, bool isRealtime, bool useAnnexB, WebCore::VideoEncoderScalabilityMode, Function<void(Encoder*)>&&);
    template<typename Frame> int32_t encodeFrameInternal(Encoder&, const Frame&, bool shouldEncodeAsKeyFrame, WebCore::VideoFrameRotation, MediaTime, int64_t timestamp, std::optional<uint64_t> duration, Function<void(bool)>&&);
    void initializeEncoderInternal(Encoder&, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate);

    RefPtr<IPC::Connection> protectedConnection() const WTF_REQUIRES_LOCK(m_connectionLock) { return m_connection; }
    RefPtr<RemoteVideoFrameObjectHeapProxy> protectedVideoFrameObjectHeapProxy() const WTF_REQUIRES_LOCK(m_connectionLock);

    HashMap<VideoDecoderIdentifier, std::unique_ptr<Decoder>> m_decoders WTF_GUARDED_BY_CAPABILITY(workQueue());
    Lock m_encodersConnectionLock;
    HashMap<VideoEncoderIdentifier, std::unique_ptr<Encoder>> m_encoders WTF_GUARDED_BY_CAPABILITY(workQueue());

    std::atomic<bool> m_needsGPUProcessConnection;

    Lock m_connectionLock;
    RefPtr<IPC::Connection> m_connection WTF_GUARDED_BY_LOCK(m_connectionLock);
    RefPtr<RemoteVideoFrameObjectHeapProxy> m_videoFrameObjectHeapProxy WTF_GUARDED_BY_LOCK(m_connectionLock);
    Vector<Function<void()>> m_tasksToDispatchAfterEstablishingConnection;

    Ref<WorkQueue> m_queue;
    RetainPtr<CVPixelBufferPoolRef> m_pixelBufferPool;
    size_t m_pixelBufferPoolWidth { 0 };
    size_t m_pixelBufferPoolHeight { 0 };
    std::optional<bool> m_vp9HardwareSupportForTesting;
    bool m_isSupportingVP9HardwareDecoder { false };
    std::optional<WTFLogLevel> m_loggingLevel;
    bool m_useGPUProcess { false };
    bool m_useRemoteFrames { false };
    bool m_hasAV1HardwareDecoder { false };
    bool m_enableAdditionalLogging { false };
};

} // namespace WebKit

#endif
