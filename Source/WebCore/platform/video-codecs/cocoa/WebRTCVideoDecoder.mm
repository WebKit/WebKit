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

#include "config.h"
#include "WebRTCVideoDecoder.h"
#include <wtf/TZoneMallocInlines.h>

#if USE(LIBWEBRTC)

#import <WebCore/LibWebRTCMacros.h>
#import "RTCVideoDecoderVTBAV1.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN

#include <webrtc/webkit_sdk/WebKit/WebKitDecoder.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

class WebRTCLocalVideoDecoder final : public WebRTCVideoDecoder {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WebRTCLocalVideoDecoder);
public:
    explicit WebRTCLocalVideoDecoder(webrtc::LocalDecoder decoder)
        : m_decoder(decoder)
    {
    }

    ~WebRTCLocalVideoDecoder()
    {
        webrtc::releaseLocalDecoder(m_decoder);
    }

private:
    void flush() final { webrtc::flushLocalDecoder(m_decoder); }
    void setFormat(std::span<const uint8_t> data, uint16_t width, uint16_t height) final { webrtc::setDecodingFormat(m_decoder, data.data(), data.size(), width, height); }
    int32_t decodeFrame(int64_t timeStamp, std::span<const uint8_t> data) final { return webrtc::decodeFrame(m_decoder, timeStamp, data.data(), data.size()); }
    void setFrameSize(uint16_t width, uint16_t height) final { webrtc::setDecoderFrameSize(m_decoder, width, height); }

    webrtc::LocalDecoder m_decoder;
};

class WebRTCDecoderVTBAV1 final : public WebRTCVideoDecoder {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WebRTCDecoderVTBAV1);
public:
    explicit WebRTCDecoderVTBAV1(RTCVideoDecoderVTBAV1Callback callback)
        : m_decoder(adoptNS([[RTCVideoDecoderVTBAV1 alloc] init]))
    {
        [m_decoder setCallback:callback];
    }

    ~WebRTCDecoderVTBAV1()
    {
        [m_decoder releaseDecoder];
    }

private:
    void flush() final { [m_decoder flush]; }
    void setFormat(std::span<const uint8_t>, uint16_t width, uint16_t height) final { setFrameSize(width, height); }
    int32_t decodeFrame(int64_t timeStamp, std::span<const uint8_t> data) final { return [m_decoder decodeData:data.data() size:data.size() timeStamp:timeStamp]; }
    void setFrameSize(uint16_t width, uint16_t height) final { [m_decoder setWidth:width height:height];; }

    RetainPtr<RTCVideoDecoderVTBAV1> m_decoder;
};

std::unique_ptr<WebRTCVideoDecoder> WebRTCVideoDecoder::create(VideoCodecType decoderType, WebRTCVideoDecoderCallback callback)
{
    switch (decoderType) {
    case VideoCodecType::H264:
        return makeUnique<WebRTCLocalVideoDecoder>(webrtc::createLocalH264Decoder(callback));
    case VideoCodecType::H265:
        return makeUnique<WebRTCLocalVideoDecoder>(webrtc::createLocalH265Decoder(callback));
    case VideoCodecType::VP9:
        return makeUnique<WebRTCLocalVideoDecoder>(webrtc::createLocalVP9Decoder(callback));
    case VideoCodecType::AV1:
        return makeUnique<WebRTCDecoderVTBAV1>(callback);
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

}

#endif //  USE(LIBWEBRTC)
