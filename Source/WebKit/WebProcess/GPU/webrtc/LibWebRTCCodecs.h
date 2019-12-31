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
#include <WebCore/ImageTransferSessionVT.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class RemoteVideoSample;
}

namespace WebKit {

class LibWebRTCCodecs : private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LibWebRTCCodecs() = default;

    struct Decoder {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        RTCDecoderIdentifier identifier;
        void* decodedImageCallback { nullptr };
        Lock decodedImageCallbackLock;
        bool hasError { false };
        RefPtr<IPC::Connection> connection;
    };

    static void setVideoDecoderCallbacks(bool useGPUProcess);

    Decoder* createDecoder();
    int32_t releaseDecoder(Decoder&);
    int32_t decodeFrame(Decoder&, uint32_t timeStamp, const uint8_t*, size_t);
    void registerDecodeFrameCallback(Decoder&, void* decodedImageCallback);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    void failedDecoding(RTCDecoderIdentifier);
    void completedDecoding(RTCDecoderIdentifier, uint32_t timeStamp, WebCore::RemoteVideoSample&&);

    HashMap<RTCDecoderIdentifier, std::unique_ptr<Decoder>> m_decodeCallbacks;
    HashSet<RTCDecoderIdentifier> m_decodingErrors;

    std::unique_ptr<WebCore::ImageTransferSessionVT> m_imageTransferSession;
};

} // namespace WebKit

#endif
