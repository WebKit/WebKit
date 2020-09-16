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
#include "RTCDecoderIdentifier.h"
#include "RTCEncoderIdentifier.h"
#include <WebCore/ImageTransferSessionVT.h>

namespace IPC {
class Connection;
class Decoder;
class DataReference;
}

namespace WebCore {
class RemoteVideoSample;
}

namespace webrtc {
using LocalDecoder = void*;
using LocalEncoder = void*;
}

namespace WebKit {

class GPUConnectionToWebProcess;

class LibWebRTCCodecsProxy : public IPC::Connection::ThreadMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<LibWebRTCCodecsProxy> create(GPUConnectionToWebProcess& process) { return adoptRef(*new LibWebRTCCodecsProxy(process)); }
    ~LibWebRTCCodecsProxy();

    void close();

private:
    explicit LibWebRTCCodecsProxy(GPUConnectionToWebProcess&);

    // IPC::Connection::ThreadMessageReceiver
    void dispatchToThread(Function<void()>&&) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void createH264Decoder(RTCDecoderIdentifier);
    void createH265Decoder(RTCDecoderIdentifier);
    void releaseDecoder(RTCDecoderIdentifier);
    void decodeFrame(RTCDecoderIdentifier, uint32_t timeStamp, const IPC::DataReference&);

    void createEncoder(RTCEncoderIdentifier, const String&, const Vector<std::pair<String, String>>&);
    void releaseEncoder(RTCEncoderIdentifier);
    void initializeEncoder(RTCEncoderIdentifier, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate);
    void encodeFrame(RTCEncoderIdentifier, WebCore::RemoteVideoSample&&, bool shouldEncodeAsKeyFrame);
    void setEncodeRates(RTCEncoderIdentifier, uint32_t bitRate, uint32_t frameRate);

    CFDictionaryRef ioSurfacePixelBufferCreationOptions(IOSurfaceRef);

    GPUConnectionToWebProcess& m_gpuConnectionToWebProcess;
    HashMap<RTCDecoderIdentifier, webrtc::LocalDecoder> m_decoders;
    HashMap<RTCEncoderIdentifier, webrtc::LocalEncoder> m_encoders;

    Ref<WorkQueue> m_queue;
    std::unique_ptr<WebCore::ImageTransferSessionVT> m_imageTransferSession;
};

}

#endif
