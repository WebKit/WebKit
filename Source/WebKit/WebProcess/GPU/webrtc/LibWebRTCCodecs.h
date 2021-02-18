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
#include "MessageReceiver.h"
#include "RTCDecoderIdentifier.h"
#include "RTCEncoderIdentifier.h"
#include <WebCore/ImageTransferSessionVT.h>
#include <WebCore/PixelBufferConformerCV.h>
#include <map>
#include <webrtc/api/video/video_codec_type.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>

using CVPixelBufferPoolRef = struct __CVPixelBufferPool*;

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class RemoteVideoSample;
}

namespace webrtc {
class VideoFrame;
struct WebKitEncodedFrameInfo;
}

namespace WebKit {

class LibWebRTCCodecs : public IPC::Connection::ThreadMessageReceiverRefCounted, public GPUProcessConnection::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<LibWebRTCCodecs> create()
    {
        auto instance = std::unique_ptr<LibWebRTCCodecs>(new LibWebRTCCodecs);
        instance->startListeningForIPC();
        return instance;
    }
    ~LibWebRTCCodecs();

    static void setCallbacks(bool useGPUProcess);

    enum class Type { H264, H265, VP9 };
    struct Decoder {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        RTCDecoderIdentifier identifier;
        Type type;
        void* decodedImageCallback { nullptr };
        Lock decodedImageCallbackLock;
        bool hasError { false };
        RefPtr<IPC::Connection> connection;
    };

    Decoder* createDecoder(Type);
    int32_t releaseDecoder(Decoder&);
    int32_t decodeFrame(Decoder&, uint32_t timeStamp, const uint8_t*, size_t, uint16_t width, uint16_t height);
    void registerDecodeFrameCallback(Decoder&, void* decodedImageCallback);

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
        RTCEncoderIdentifier identifier;
        webrtc::VideoCodecType codecType { webrtc::kVideoCodecGeneric };
        Vector<std::pair<String, String>> parameters;
        Optional<EncoderInitializationData> initializationData;
        void* encodedImageCallback { nullptr };
        Lock encodedImageCallbackLock;
        RefPtr<IPC::Connection> connection;
    };

    Encoder* createEncoder(Type, const std::map<std::string, std::string>&);
    int32_t releaseEncoder(Encoder&);
    int32_t initializeEncoder(Encoder&, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate);
    int32_t encodeFrame(Encoder&, const webrtc::VideoFrame&, bool shouldEncodeAsKeyFrame);
    void registerEncodeFrameCallback(Encoder&, void* encodedImageCallback);
    void setEncodeRates(Encoder&, uint32_t bitRate, uint32_t frameRate);

    CVPixelBufferPoolRef pixelBufferPool(size_t width, size_t height, OSType);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void setVP9VTBSupport(bool supportVP9VTB) { m_supportVP9VTB = supportVP9VTB; }
    bool supportVP9VTB() const { return m_supportVP9VTB; }

private:
    LibWebRTCCodecs();
    void startListeningForIPC();

    void failedDecoding(RTCDecoderIdentifier);
    void completedDecoding(RTCDecoderIdentifier, uint32_t timeStamp, WebCore::RemoteVideoSample&&);
    void completedEncoding(RTCEncoderIdentifier, IPC::DataReference&&, const webrtc::WebKitEncodedFrameInfo&);
    RetainPtr<CVPixelBufferRef> convertToBGRA(CVPixelBufferRef);

    // IPC::Connection::ThreadMessageReceiver
    void dispatchToThread(Function<void()>&&) final;

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&);

private:
    HashMap<RTCDecoderIdentifier, std::unique_ptr<Decoder>> m_decoders;
    HashSet<RTCDecoderIdentifier> m_decodingErrors;

    HashMap<RTCEncoderIdentifier, std::unique_ptr<Encoder>> m_encoders;

    Lock m_connectionLock;
    RefPtr<IPC::Connection> m_connection;
    Ref<WorkQueue> m_queue;
    std::unique_ptr<WebCore::ImageTransferSessionVT> m_imageTransferSession;
    std::unique_ptr<WebCore::PixelBufferConformerCV> m_pixelBufferConformer;
    RetainPtr<CVPixelBufferPoolRef> m_pixelBufferPool;
    size_t m_pixelBufferPoolWidth { 0 };
    size_t m_pixelBufferPoolHeight { 0 };
    bool m_supportVP9VTB { false };
};

} // namespace WebKit

#endif
