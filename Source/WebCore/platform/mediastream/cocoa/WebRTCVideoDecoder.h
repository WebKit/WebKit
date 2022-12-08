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

#pragma once

#if USE(LIBWEBRTC)

#include <wtf/UniqueRef.h>

namespace webrtc {
using LocalDecoder = void*;
}

namespace WebCore {

class WebRTCVideoDecoder {
public:
    virtual ~WebRTCVideoDecoder() = default;

#if USE(LIBWEBRTC)
    WEBCORE_EXPORT static UniqueRef<WebRTCVideoDecoder> createFromLocalDecoder(webrtc::LocalDecoder);
#endif

    virtual void flush() = 0;
    virtual void setFormat(const uint8_t*, size_t, uint16_t width, uint16_t height) = 0;
    virtual int32_t decodeFrame(int64_t timeStamp, const uint8_t*, size_t) = 0;
    virtual void setFrameSize(uint16_t width, uint16_t height) = 0;
};

}

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebRTCVideoDecoderAdditions.h>
#endif

#endif // USE(LIBWEBRTC)
