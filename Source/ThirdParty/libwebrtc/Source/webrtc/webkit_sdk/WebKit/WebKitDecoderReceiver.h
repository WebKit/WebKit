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

#include "api/video_codecs/video_decoder.h"
#include "VTVideoDecoderSPI.h"
#include "WebKitUtilities.h"

namespace webrtc {

class WebKitDecoderReceiver final : public DecodedImageCallback {
public:
    explicit WebKitDecoderReceiver(VTVideoDecoderSession);
    ~WebKitDecoderReceiver();

    VTVideoDecoderFrame currentFrame() const { return m_currentFrame; }
    void setCurrentFrame(VTVideoDecoderFrame currentFrame) { m_currentFrame = currentFrame; }
    OSStatus decoderFailed(int error);

    void initializeFromFormatDescription(CMFormatDescriptionRef);

private:
    int32_t Decoded(VideoFrame&) final;
    int32_t Decoded(VideoFrame&, int64_t decode_time_ms) final;
    void Decoded(VideoFrame&, absl::optional<int32_t> decode_time_ms, absl::optional<uint8_t> qp) final;

    CVPixelBufferPoolRef pixelBufferPool(size_t pixelBufferWidth, size_t pixelBufferHeight, bool is10bit);

    VTVideoDecoderSession m_session { nullptr };
    VTVideoDecoderFrame m_currentFrame { nullptr };
    size_t m_pixelBufferWidth { 0 };
    size_t m_pixelBufferHeight { 0 };
    bool m_is10bit { false };
    bool m_isFullRange { false };
    CVPixelBufferPoolRef m_pixelBufferPool { nullptr };
};

}
