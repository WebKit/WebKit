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

#include "MessageReceiver.h"
#include "RTCDecoderIdentifier.h"
#include "RTCEncoderIdentifier.h"
#include <WebCore/ImageTransferSessionVT.h>
#include <map>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>

using CVPixelBufferPoolRef = struct __CVPixelBufferPool*;

namespace IPC {
class Connection;
class DataReference;
class Decoder;
}

namespace WebCore {
class RemoteVideoSample;
}

namespace webrtc {
class WebKitRTPFragmentationHeader;
struct WebKitEncodedFrameInfo;
}

namespace WebKit {

class LibWebRTCCodecs : private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LibWebRTCCodecs() = default;

    static void setCallbacks(bool useGPUProcess);

    struct Decoder {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        RTCDecoderIdentifier identifier;
        void* decodedImageCallback { nullptr };
        Lock decodedImageCallbackLock;
        bool hasError { false };
        RefPtr<IPC::Connection> connection;
    };

    Decoder* createDecoder();
    int32_t releaseDecoder(Decoder&);
    int32_t decodeFrame(Decoder&, uint32_t timeStamp, const uint8_t*, size_t);
    void registerDecodeFrameCallback(Decoder&, void* decodedImageCallback);

    struct Encoder {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        RTCEncoderIdentifier identifier;
        void* encodedImageCallback { nullptr };
        Lock encodedImageCallbackLock;
        RefPtr<IPC::Connection> connection;
    };

    Encoder* createEncoder(const std::map<std::string, std::string>&);
    int32_t releaseEncoder(Encoder&);
    int32_t initializeEncoder(Encoder&, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate);
    int32_t encodeFrame(Encoder&, const WebCore::RemoteVideoSample&, bool shouldEncodeAsKeyFrame);
    void registerEncodeFrameCallback(Encoder&, void* encodedImageCallback);
    void setEncodeRates(Encoder&, uint32_t bitRate, uint32_t frameRate);
    
    CVPixelBufferPoolRef pixelBufferPool(size_t width, size_t height);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    void failedDecoding(RTCDecoderIdentifier);
    void completedDecoding(RTCDecoderIdentifier, uint32_t timeStamp, WebCore::RemoteVideoSample&&);
    void completedEncoding(RTCEncoderIdentifier, IPC::DataReference&&, const webrtc::WebKitEncodedFrameInfo&, webrtc::WebKitRTPFragmentationHeader&&);

private:
    HashMap<RTCDecoderIdentifier, std::unique_ptr<Decoder>> m_decoders;
    HashSet<RTCDecoderIdentifier> m_decodingErrors;

    HashMap<RTCEncoderIdentifier, std::unique_ptr<Encoder>> m_encoders;

    std::unique_ptr<WebCore::ImageTransferSessionVT> m_imageTransferSession;
    RetainPtr<CVPixelBufferPoolRef> m_pixelBufferPool;
    size_t m_pixelBufferPoolWidth { 0 };
    size_t m_pixelBufferPoolHeight { 0 };
};

} // namespace WebKit

#endif
