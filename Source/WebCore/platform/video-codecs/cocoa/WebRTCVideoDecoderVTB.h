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

#include "VideoDecoderVTB.h"
#include <WebCore/WebRTCVideoDecoder.h>
#include <wtf/BlockPtr.h>

namespace WebCore {

class WebRTCVideoDecoderVTB : public WebRTCVideoDecoder {
public:
    ~WebRTCVideoDecoderVTB() = default;

protected:
    WebRTCVideoDecoderVTB(VideoDecoderVTB::CallbackMultiImage&& callback)
        : m_callback(WTF::move(callback))
    {
    }

    static BlockPtr<void(OSStatus, VTDecodeInfoFlags, CVImageBufferRef pixelBuffer, CMTaggedBufferGroupRef, CMTime presentationTime, CMTime)> createMultiImageCallback(WebRTCVideoDecoderCallback);

    int32_t decodeFrameInternal(int64_t timeStamp, std::span<const uint8_t> data);
    void setVideoFormat(RetainPtr<CMVideoFormatDescriptionRef>&&);

    uint16_t width() const { return m_width; }
    uint16_t height() const { return m_height; }

private:
    void flush() final;
    void setFormat(std::span<const uint8_t>, uint16_t width, uint16_t height) override;
    void setFrameSize(uint16_t width, uint16_t height) final;

    RetainPtr<CMVideoFormatDescriptionRef> m_format;
    RefPtr<VideoDecoderVTB> m_decoder;
    const VideoDecoderVTB::CallbackMultiImage m_callback;
    uint16_t m_width { 0 };
    uint16_t m_height { 0 };
};

}

#endif // USE(LIBWEBRTC)
