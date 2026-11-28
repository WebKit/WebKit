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

#include "config.h"
#include "LibWebRTCVideoDecoderFactoryCocoa.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA)

#include "LibWebRTCMacros.h"
#include "LibWebRTCVideoDecoderAdapter.h"
#include <algorithm>
#include <wtf/ASCIICType.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN

#include <webrtc/api/environment/environment.h>
#include <webrtc/api/video_codecs/sdp_video_format.h>
#include <webrtc/api/video_codecs/video_decoder_factory.h>
#include <webrtc/api/video_codecs/video_decoder_software_fallback_wrapper.h>
#include <webrtc/modules/video_coding/codecs/vp9/include/vp9.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

class LibWebRTCVideoDecoderFactoryCocoa final : public webrtc::VideoDecoderFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(LibWebRTCVideoDecoderFactoryCocoa);
public:
    explicit LibWebRTCVideoDecoderFactoryCocoa(const LibWebRTCVideoDecoderFactoryConfiguration& configuration)
        : m_configuration(configuration)
        , m_queue(WorkQueue::create("LibWebRTCVideoDecoderAdapter queue"_s, WorkQueue::QOS::UserInteractive))
    {
    }

private:
    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const final;
    std::unique_ptr<webrtc::VideoDecoder> Create(const webrtc::Environment&, const webrtc::SdpVideoFormat&) final;

    const LibWebRTCVideoDecoderFactoryConfiguration m_configuration;
    const Ref<WorkQueue> m_queue;
};

static std::optional<LibWebRTCVideoDecoderAdapter::Description> videoDecoderDescription(const webrtc::SdpVideoFormat& format, const LibWebRTCVideoDecoderFactoryConfiguration& configuration)
{
    auto parameter = [&](const char* key) -> std::string {
        auto iterator = format.parameters.find(key);
        return iterator == format.parameters.end() ? std::string { } : iterator->second;
    };

    auto name = String::fromUTF8(format.name);

    // RTP H.264 and H.265 payloads are depacketized as Annex B, which means no codec description is needed.
    if (equalLettersIgnoringASCIICase(name, "h264"_s)) {
        auto profileLevelId = parameter("profile-level-id");
        if (profileLevelId.size() != 6 || !std::ranges::all_of(profileLevelId, [](char character) { return isASCIIHexDigit(character); }))
            profileLevelId = "42e01f";
        return LibWebRTCVideoDecoderAdapter::Description { makeString("avc1."_s, String::fromLatin1(profileLevelId.c_str())), VideoDecoder::HardwareAcceleration::Yes, false };
    }

    if (equalLettersIgnoringASCIICase(name, "h265"_s))
        return LibWebRTCVideoDecoderAdapter::Description { "hev1.1.6.L93"_s, VideoDecoder::HardwareAcceleration::Yes, false };

    if (equalLettersIgnoringASCIICase(name, "vp8"_s))
        return LibWebRTCVideoDecoderAdapter::Description { "vp8"_s, VideoDecoder::HardwareAcceleration::No, false };

    if (equalLettersIgnoringASCIICase(name, "vp9"_s)) {
        bool isProfile2 = parameter("profile-id") == "2";
        return LibWebRTCVideoDecoderAdapter::Description { isProfile2 ? "vp09.02.10.10"_s : "vp09.00.10.08"_s, configuration.supportsHWVP9 ? VideoDecoder::HardwareAcceleration::Yes : VideoDecoder::HardwareAcceleration::No, true };
    }

#if ENABLE(AV1)
    if (equalLettersIgnoringASCIICase(name, "av1"_s))
        return LibWebRTCVideoDecoderAdapter::Description { "av01.0.04M.08"_s, VideoDecoder::HardwareAcceleration::Yes, false };
#endif

    return { };
}

std::vector<webrtc::SdpVideoFormat> LibWebRTCVideoDecoderFactoryCocoa::GetSupportedFormats() const
{
    std::vector<webrtc::SdpVideoFormat> formats;
    formats.reserve(9);

    auto addH264Format = [&](const char* profileLevelId, const char* packetizationMode) {
        formats.emplace_back("H264", webrtc::CodecParameterMap {
            { "profile-level-id", profileLevelId },
            { "level-asymmetry-allowed", "1" },
            { "packetization-mode", packetizationMode },
        });
    };
    addH264Format("640c1f", "1");
    addH264Format("42e01f", "1");
    addH264Format("640c1f", "0");
    addH264Format("42e01f", "0");

    if (m_configuration.supportsH265)
        formats.emplace_back("H265", webrtc::CodecParameterMap { });

    formats.emplace_back("VP8", webrtc::CodecParameterMap { });

    if (m_configuration.supportsVP9Profile0)
        formats.emplace_back("VP9", webrtc::CodecParameterMap { { "profile-id", "0" } });
    if (m_configuration.supportsVP9Profile2)
        formats.emplace_back("VP9", webrtc::CodecParameterMap { { "profile-id", "2" } });

#if ENABLE(AV1)
    if (m_configuration.supportsAV1)
        formats.emplace_back("AV1", webrtc::CodecParameterMap { });
#endif

    return formats;
}

std::unique_ptr<webrtc::VideoDecoder> LibWebRTCVideoDecoderFactoryCocoa::Create(const webrtc::Environment& environment, const webrtc::SdpVideoFormat& format)
{
    auto description = videoDecoderDescription(format, m_configuration);
    if (!description)
        return nullptr;

    bool isVP9 = description->isVP9;
    auto decoder = makeUnique<LibWebRTCVideoDecoderAdapter>(WTF::move(*description), m_queue.copyRef());
    if (!isVP9)
        return decoder;

    // We only fall back for VP9, to libvpx, for instance in case of spatial scalability.
    return webrtc::CreateVideoDecoderSoftwareFallbackWrapper(environment, webrtc::VP9Decoder::Create(), WTF::move(decoder));
}

std::unique_ptr<webrtc::VideoDecoderFactory> createLibWebRTCVideoDecoderFactoryCocoa(const LibWebRTCVideoDecoderFactoryConfiguration& configuration)
{
    return makeUnique<LibWebRTCVideoDecoderFactoryCocoa>(configuration);
}

} // namespace WebCore

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA)
