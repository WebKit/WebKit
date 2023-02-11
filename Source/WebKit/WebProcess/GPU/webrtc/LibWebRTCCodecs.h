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
#include <map>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>

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
}

namespace WebKit {

class RemoteVideoFrameObjectHeapProxy;

class LibWebRTCCodecs : public IPC::WorkQueueMessageReceiver, public GPUProcessConnection::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<LibWebRTCCodecs> create();
    ~LibWebRTCCodecs();

    static void setCallbacks(bool useGPUProcess, bool useRemoteFrames);
    static void setWebRTCMediaPipelineAdditionalLoggingEnabled(bool);
    static void initializeIfNeeded();

    std::optional<VideoCodecType> videoCodecTypeFromWebCodec(const String&);

    using DecoderCallback = Function<void(RefPtr<WebCore::VideoFrame>&&, int64_t timestamp)>;
    struct Decoder {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        VideoDecoderIdentifier identifier;
        VideoCodecType type;
        void* decodedImageCallback WTF_GUARDED_BY_LOCK(decodedImageCallbackLock) { nullptr };
        DecoderCallback decoderCallback;
        Lock decodedImageCallbackLock;
        bool hasError { false };
        RefPtr<IPC::Connection> connection;
        Deque<Function<void()>> flushCallbacks WTF_GUARDED_BY_LOCK(flushCallbacksLock);
        Lock flushCallbacksLock;
    };

    Decoder* createDecoder(VideoCodecType);
    void createDecoderAndWaitUntilReady(VideoCodecType, Function<void(Decoder&)>&&);

    int32_t releaseDecoder(Decoder&);
    void flushDecoder(Decoder&, Function<void()>&&);
    void setDecoderFormatDescription(Decoder&, const uint8_t*, size_t, uint16_t width, uint16_t height);
    int32_t decodeFrame(Decoder&, int64_t timeStamp, const uint8_t*, size_t, uint16_t width, uint16_t height);
    void registerDecodeFrameCallback(Decoder&, void* decodedImageCallback);
    void registerDecodedVideoFrameCallback(Decoder&, DecoderCallback&&);

    using DescriptionCallback = Function<void(Span<const uint8_t>&&)>;
    using EncoderCallback = Function<void(Span<const uint8_t>&&, bool isKeyFrame, int64_t timestamp)>;
    struct EncoderInitializationData {
        uint16_t width;
        uint16_t height;
        unsigned startBitRate;
        unsigned maxBitRate;
        unsigned minBitRate;
        uint32_t maxFrameRate;
    };
    struct Encoder {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        VideoEncoderIdentifier identifier;
        VideoCodecType type;
        Vector<std::pair<String, String>> parameters;
        std::optional<EncoderInitializationData> initializationData;
        void* encodedImageCallback WTF_GUARDED_BY_LOCK(encodedImageCallbackLock) { nullptr };
        EncoderCallback encoderCallback;
        DescriptionCallback descriptionCallback;
        Lock encodedImageCallbackLock;
        RefPtr<IPC::Connection> connection;
        SharedVideoFrameWriter sharedVideoFrameWriter;
        bool hasSentInitialEncodeRates { false };
        bool useAnnexB { true };
        bool isRealtime { true };
        Deque<Function<void()>> flushCallbacks WTF_GUARDED_BY_LOCK(flushCallbacksLock);
        Lock flushCallbacksLock;
    };

    Encoder* createEncoder(VideoCodecType, const std::map<std::string, std::string>&);
    void createEncoderAndWaitUntilReady(VideoCodecType, const std::map<std::string, std::string>&, bool isRealtime, bool useAnnexB, Function<void(Encoder&)>&&);
    int32_t releaseEncoder(Encoder&);
    int32_t initializeEncoder(Encoder&, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate);
    int32_t encodeFrame(Encoder&, const WebCore::VideoFrame&, uint32_t timestamp, bool shouldEncodeAsKeyFrame);
    int32_t encodeFrame(Encoder&, const webrtc::VideoFrame&, bool shouldEncodeAsKeyFrame);
    void flushEncoder(Encoder&, Function<void()>&&);
    void registerEncodeFrameCallback(Encoder&, void* encodedImageCallback);
    void registerEncodedVideoFrameCallback(Encoder&, EncoderCallback&&);
    void registerEncoderDescriptionCallback(Encoder&, DescriptionCallback&&);
    void setEncodeRates(Encoder&, uint32_t bitRate, uint32_t frameRate);

    CVPixelBufferPoolRef pixelBufferPool(size_t width, size_t height, OSType);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void setVP9VTBSupport(bool supportVP9VTB) { m_supportVP9VTB = supportVP9VTB; }
    bool supportVP9VTB() const { return m_supportVP9VTB; }
    void setLoggingLevel(WTFLogLevel);

    void setHasVP9ExtensionSupport(bool);
    bool hasVP9ExtensionSupport() const { return m_hasVP9ExtensionSupport; }

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
    void completedEncoding(VideoEncoderIdentifier, IPC::DataReference&&, const webrtc::WebKitEncodedFrameInfo&);
    void flushEncoderCompleted(VideoEncoderIdentifier);
    void setEncodingDescription(WebKit::VideoEncoderIdentifier, IPC::DataReference&&);
    RetainPtr<CVPixelBufferRef> convertToBGRA(CVPixelBufferRef);

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&);

    IPC::Connection* encoderConnection(Encoder&) WTF_REQUIRES_LOCK(m_encodersConnectionLock);
    void setEncoderConnection(Encoder&, RefPtr<IPC::Connection>&&) WTF_REQUIRES_LOCK(m_encodersConnectionLock);
    IPC::Connection* decoderConnection(Decoder&) WTF_REQUIRES_LOCK(m_connectionLock);
    void setDecoderConnection(Decoder&, RefPtr<IPC::Connection>&&) WTF_REQUIRES_LOCK(m_connectionLock);

    template<typename Buffer> bool copySharedVideoFrame(LibWebRTCCodecs::Encoder&, IPC::Connection&, Buffer&&);
    WorkQueue& workQueue() const { return m_queue; }

    Decoder* createDecoderInternal(VideoCodecType, Function<void(Decoder&)>&&);
    Encoder* createEncoderInternal(VideoCodecType, const std::map<std::string, std::string>&, bool isRealtime, bool useAnnexB, Function<void(Encoder&)>&&);
    template<typename Frame> int32_t encodeFrameInternal(Encoder&, const Frame&, bool shouldEncodeAsKeyFrame, WebCore::VideoFrameRotation, MediaTime, uint32_t);

private:
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
    bool m_supportVP9VTB { false };
    std::optional<WTFLogLevel> m_loggingLevel;
    bool m_useGPUProcess { false };
    bool m_useRemoteFrames { false };
    bool m_hasVP9ExtensionSupport { false };
    bool m_enableAdditionalLogging { false };
};

} // namespace WebKit

#endif
