/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC) && PLATFORM(COCOA)

#include <WebCore/LibWebRTCMacros.h>
#include <WebCore/VideoDecoder.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/api/video_codecs/video_decoder.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

class LibWebRTCVideoDecoderAdapterBackend;

class LibWebRTCVideoDecoderAdapter final : public webrtc::VideoDecoder {
    WTF_MAKE_TZONE_ALLOCATED(LibWebRTCVideoDecoderAdapter);
public:
    struct Description {
        String codec;
        WebCore::VideoDecoder::HardwareAcceleration hardwareAcceleration { WebCore::VideoDecoder::HardwareAcceleration::No };
        bool isVP9 { false };

        Description isolatedCopy() const
        {
            return {
                .codec = codec.isolatedCopy(),
                .hardwareAcceleration = hardwareAcceleration,
                .isVP9 = isVP9
            };
        }
        Description isolatedCopy() &&
        {
            return {
                .codec = WTF::move(codec).isolatedCopy(),
                .hardwareAcceleration = hardwareAcceleration,
                .isVP9 = isVP9
            };
        }
    };

    LibWebRTCVideoDecoderAdapter(Description&&, Ref<WorkQueue>&&);
    ~LibWebRTCVideoDecoderAdapter();

private:
    // webrtc::VideoDecoder
    bool Configure(const Settings&) final;
    int32_t Decode(const webrtc::EncodedImage&, int64_t renderTimeMs) final;
    int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback*) final;
    int32_t Release() final;
    DecoderInfo GetDecoderInfo() const final;
    const char* ImplementationName() const final;

    const Description m_description;
    const Ref<WorkQueue> m_queue;
    bool m_shouldCreateBackend { false };
    Ref<LibWebRTCVideoDecoderAdapterBackend> m_backend;
};

} // namespace WebCore

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA)
