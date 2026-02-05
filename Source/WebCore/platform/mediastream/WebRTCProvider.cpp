/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebRTCProvider.h"

#include "AV1Utilities.h"
#include "ContentType.h"
#include "HEVCUtilities.h"
#include "PlatformMediaCapabilitiesDecodingInfo.h"
#include "PlatformMediaCapabilitiesEncodingInfo.h"
#include "PlatformMediaDecodingConfiguration.h"
#include "PlatformMediaEncodingConfiguration.h"
#include "PlatformMediaEngineConfigurationFactory.h"
#include "VP9Utilities.h"

#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebRTCProvider);

#if !USE(LIBWEBRTC) && !USE(GSTREAMER_WEBRTC)
UniqueRef<WebRTCProvider> WebRTCProvider::create()
{
    return makeUniqueRef<WebRTCProvider>();
}

bool WebRTCProvider::webRTCAvailable()
{
    return false;
}
#endif

RefPtr<RTCDataChannelRemoteHandlerConnection> WebRTCProvider::createRTCDataChannelRemoteHandlerConnection()
{
    return nullptr;
}

void WebRTCProvider::setH265Support(bool value)
{
    m_supportsH265 = value;
#if ENABLE(WEB_RTC)
    m_videoDecodingCapabilities = { };
    m_videoEncodingCapabilities = { };
#endif
}

void WebRTCProvider::setVP9Support(bool supportsVP9Profile0, bool supportsVP9Profile2)
{
    m_supportsVP9Profile0 = supportsVP9Profile0;
    m_supportsVP9Profile2 = supportsVP9Profile2;

#if ENABLE(WEB_RTC)
    m_videoDecodingCapabilities = { };
    m_videoEncodingCapabilities = { };
#endif
}

void WebRTCProvider::setAV1Support(bool supportsAV1)
{
    m_supportsAV1 = supportsAV1;

#if ENABLE(WEB_RTC)
    m_videoDecodingCapabilities = { };
    m_videoEncodingCapabilities = { };
#endif
}

bool WebRTCProvider::isSupportingAV1() const
{
    return m_supportsAV1;
}

bool WebRTCProvider::isSupportingH265() const
{
    return m_supportsH265;
}

bool WebRTCProvider::isSupportingVP9Profile0() const
{
    return m_supportsVP9Profile0;
}

bool WebRTCProvider::isSupportingVP9Profile2() const
{
    return m_supportsVP9Profile2;
}

bool WebRTCProvider::isSupportingMDNS() const
{
    return m_supportsMDNS;
}

void WebRTCProvider::setLoggingLevel(WTFLogLevel)
{

}

void WebRTCProvider::clearFactory()
{

}

#if ENABLE(WEB_RTC)

std::optional<RTCRtpCapabilities> WebRTCProvider::receiverCapabilities(const String&)
{
    return { };
}

std::optional<RTCRtpCapabilities> WebRTCProvider::senderCapabilities(const String&)
{
    return { };
}

std::optional<RTCRtpCodecCapability> WebRTCProvider::codecCapability(const ContentType& contentType, const std::optional<RTCRtpCapabilities>& capabilities)
{
    if (!capabilities)
        return { };

    auto containerType = contentType.containerType();
    for (auto& codec : capabilities->codecs) {
        if (equalIgnoringASCIICase(containerType, codec.mimeType))
            return codec;
    }
    return { };
}

std::optional<RTCRtpCapabilities>& WebRTCProvider::audioDecodingCapabilities()
{
    if (!m_audioDecodingCapabilities)
        initializeAudioDecodingCapabilities();
    return m_audioDecodingCapabilities;
}

std::optional<RTCRtpCapabilities>& WebRTCProvider::videoDecodingCapabilities()
{
    if (!m_videoDecodingCapabilities)
        initializeVideoDecodingCapabilities();
    return m_videoDecodingCapabilities;
}

std::optional<RTCRtpCapabilities>& WebRTCProvider::audioEncodingCapabilities()
{
    if (!m_audioEncodingCapabilities)
        initializeAudioEncodingCapabilities();
    return m_audioEncodingCapabilities;
}

std::optional<RTCRtpCapabilities>& WebRTCProvider::videoEncodingCapabilities()
{
    if (!m_videoEncodingCapabilities)
        initializeVideoEncodingCapabilities();
    return m_videoEncodingCapabilities;
}

#endif // ENABLE(WEB_RTC)

std::optional<PlatformMediaCapabilitiesInfo> WebRTCProvider::computeVPParameters(const PlatformMediaCapabilitiesVideoConfiguration&)
{
    return { };
}

bool WebRTCProvider::isVPSoftwareDecoderSmooth(const PlatformMediaCapabilitiesVideoConfiguration&)
{
    return true;
}

bool WebRTCProvider::isVPXEncoderSmooth(const PlatformMediaCapabilitiesVideoConfiguration&)
{
    return false;
}

bool WebRTCProvider::isH264EncoderSmooth(const PlatformMediaCapabilitiesVideoConfiguration&)
{
    return true;
}

static String contentTypeFromRTPVideoMimeType(const String& mimeType)
{
    ContentType contentType { mimeType };
    auto containerType = contentType.containerType();
    if (equalLettersIgnoringASCIICase(containerType, "video/h264"_s)) {
        // https://datatracker.ietf.org/doc/html/rfc6184#section-8.1
        auto profileLevelId = contentType.parameter("profile-level-id"_s);
        if (profileLevelId.isEmpty())
            profileLevelId = "42001f"_s;
        return makeString("video/mp4;codecs=avc1."_s, profileLevelId);
    }

    if (equalLettersIgnoringASCIICase(containerType, "video/h265"_s)) {
        // https://datatracker.ietf.org/doc/html/rfc7798#section-7.1
        HEVCParameters parameters;
        auto profileSpace = contentType.parameter("profile-space"_s);
        if (!profileSpace.isEmpty()) {
            auto result = parseInteger<uint16_t>(profileSpace);
            if (!result || result > 3)
                return { };
            parameters.generalProfileSpace = *result;
        }

        auto profileId = contentType.parameter("profile-id"_s);
        if (!profileId.isEmpty()) {
            auto result = parseInteger<uint16_t>(profileId);
            if (!result)
                return { };
            parameters.generalProfileIDC = *result;
        } else
            parameters.generalProfileIDC = 1;

        auto profileCompatibilityIndicator = contentType.parameter("profile-compatibility-indicator"_s);
        if (!profileCompatibilityIndicator.isEmpty()) {
            auto compatibilityFlags = parseInteger<uint32_t>(profileCompatibilityIndicator, 16);
            if (!compatibilityFlags)
                return { };
            parameters.generalProfileCompatibilityFlags = reverseBits32(*compatibilityFlags);
        } else
            parameters.generalProfileCompatibilityFlags = 1 << parameters.generalProfileIDC;

        auto tierFlag = contentType.parameter("tier-flag"_s);
        if (tierFlag.isEmpty() || tierFlag == "0"_s)
            parameters.generalTierFlag = 0;
        else if (tierFlag == "1"_s)
            parameters.generalTierFlag = 1;
        else
            return { };

        auto levelId = contentType.parameter("level-id"_s);
        if (!levelId.isEmpty()) {
            auto result = parseInteger<uint16_t>(levelId);
            if (!result)
                return { };
            parameters.generalLevelIDC = *result;
        } else
            parameters.generalLevelIDC = 93;

        return makeString("video/mp4;codecs="_s, createHEVCCodecParametersString(parameters));
    }

    if (equalLettersIgnoringASCIICase(containerType, "video/vp9"_s)) {
        // https://www.rfc-editor.org/rfc/rfc9628.html#payloadFormatParameters
        VPCodecConfigurationRecord parameters;
        parameters.codecName = "vp09"_s;

        auto profileId = contentType.parameter("profile-id"_s);
        if (!profileId.isEmpty()) {
            auto result = parseInteger<uint8_t>(profileId);
            if (!result || *result > 3)
                return { };
            parameters.profile = *result;
        }

        // We use level 5 as the default.
        parameters.level = VPConfigurationLevel::Level_5;

        parameters.bitDepth = parameters.profile < 2 ? 8 : 10;

        return makeString("video/mp4;codecs="_s, createVPCodecParametersString(parameters));
    }

    if (equalLettersIgnoringASCIICase(containerType, "video/av1"_s)) {
        // https://www.iana.org/assignments/media-types/video/AV1
        AV1CodecConfigurationRecord parameters;
        parameters.codecName = "av01"_s;

        auto profile = contentType.parameter("profile"_s);
        if (!profile.isEmpty()) {
            auto result = parseEnumFromStringView<AV1ConfigurationProfile>(profile);
            if (!result)
                return { };
            parameters.profile = *result;
        }

        auto levelIdx = contentType.parameter("level-idx"_s);
        if (!levelIdx.isEmpty()) {
            auto result = parseEnumFromStringView<AV1ConfigurationLevel>(levelIdx);
            if (!result)
                return { };
            parameters.level = *result;
        } else
            parameters.level = AV1ConfigurationLevel::Level_3_1;

        auto tier = contentType.parameter("tier"_s);
        if (!tier.isEmpty()) {
            if (tier == "M"_s)
                parameters.tier = AV1ConfigurationTier::Main;
            else if (tier == "H"_s)
                parameters.tier = AV1ConfigurationTier::High;
            else
                return { };
        }

        parameters.bitDepth = parameters.profile < AV1ConfigurationProfile::Professional ? 8 : 10;

        return makeString("video/mp4;codecs="_s, createAV1CodecParametersString(parameters));
    }

    if (equalLettersIgnoringASCIICase(containerType, "video/vp8"_s))
        return "video/webm;codecs=vp08.00.50.08"_s;

    return { };
}

void WebRTCProvider::createDecodingConfiguration(PlatformMediaDecodingConfiguration&& configuration, DecodingConfigurationCallback&& callback)
{
    ASSERT(configuration.type == PlatformMediaDecodingType::WebRTC);

    // FIXME: Validate additional parameters, in particular mime type parameters.
    PlatformMediaCapabilitiesDecodingInfo info { { }, WTF::move(configuration) };

#if ENABLE(WEB_RTC)
    if (info.configuration.video) {
        ContentType contentType { info.configuration.video->contentType };
        auto codec = codecCapability(contentType, videoDecodingCapabilities());
        if (!codec) {
            callback({ { }, WTF::move(info.configuration) });
            return;
        }
        if (auto infoOverride = videoDecodingCapabilitiesOverride(*info.configuration.video)) {
            if (!infoOverride->supported) {
                callback({ { }, WTF::move(info.configuration) });
                return;
            }
            info.smooth = infoOverride->smooth;
            info.powerEfficient = infoOverride->powerEfficient;
        }
    }
    if (info.configuration.audio) {
        ContentType contentType { info.configuration.audio->contentType };
        auto codec = codecCapability(contentType, audioDecodingCapabilities());
        if (!codec) {
            callback({ { }, WTF::move(info.configuration) });
            return;
        }
    }
#endif
    // For power efficient decoders, we use the regular media engine MC code path which has more fine grained checks.
    if (info.powerEfficient && info.configuration.video) {
        auto videoConfiguration = info.configuration;
        videoConfiguration.audio = { };
        videoConfiguration.video->contentType = contentTypeFromRTPVideoMimeType(info.configuration.video->contentType);
        videoConfiguration.type = PlatformMediaDecodingType::MediaSource;

        PlatformMediaEngineConfigurationFactory::createDecodingConfiguration(WTF::move(videoConfiguration), [info = WTF::move(info), callback = WTF::move(callback)](auto&& result) mutable {
            info.supported = result.supported;
            info.smooth = result.smooth;
            info.powerEfficient = result.powerEfficient;
            if (!info.supported)
                info.configuration = { };
            callback(WTF::move(info));
        });
        return;
    }

    info.supported = true;
    callback(WTF::move(info));
}

void WebRTCProvider::createEncodingConfiguration(PlatformMediaEncodingConfiguration&& configuration, EncodingConfigurationCallback&& callback)
{
    ASSERT(configuration.type == PlatformMediaEncodingType::WebRTC);

    // FIXME: Validate additional parameters, in particular mime type parameters.
    PlatformMediaCapabilitiesEncodingInfo info { { }, WTF::move(configuration) };

#if ENABLE(WEB_RTC)
    if (info.configuration.video) {
        ContentType contentType { info.configuration.video->contentType };
        auto codec = codecCapability(contentType, videoEncodingCapabilities());
        if (!codec) {
            callback({ { }, WTF::move(info.configuration) });
            return;
        }
        if (auto infoOverride = videoEncodingCapabilitiesOverride(*info.configuration.video)) {
            if (!infoOverride->supported) {
                callback({ { }, WTF::move(info.configuration) });
                return;
            }
            info.smooth = infoOverride->smooth;
            info.powerEfficient = infoOverride->powerEfficient;
        }
    }
    if (info.configuration.audio) {
        ContentType contentType { info.configuration.audio->contentType };
        auto codec = codecCapability(contentType, audioEncodingCapabilities());
        if (!codec) {
            callback({ { }, WTF::move(info.configuration) });
            return;
        }
    }
#endif
    info.supported = true;
    callback(WTF::move(info));
}

void WebRTCProvider::initializeAudioDecodingCapabilities()
{

}

void WebRTCProvider::initializeVideoDecodingCapabilities()
{

}

void WebRTCProvider::initializeAudioEncodingCapabilities()
{

}

void WebRTCProvider::initializeVideoEncodingCapabilities()
{

}

std::optional<PlatformMediaCapabilitiesDecodingInfo> WebRTCProvider::videoDecodingCapabilitiesOverride(const PlatformMediaCapabilitiesVideoConfiguration&)
{
    return { };
}

std::optional<PlatformMediaCapabilitiesEncodingInfo> WebRTCProvider::videoEncodingCapabilitiesOverride(const PlatformMediaCapabilitiesVideoConfiguration&)
{
    return { };
}

void WebRTCProvider::setPortAllocatorRange(StringView range)
{
    if (range.isEmpty())
        return;

    if (range == "0:0"_s)
        return;

    auto components = range.toStringWithoutCopying().split(':');
    if (components.size() != 2) [[unlikely]] {
        WTFLogAlways("Invalid format for UDP port range. Should be \"min-port:max-port\"");
        ASSERT_NOT_REACHED();
        return;
    }

    auto minPort = WTF::parseInteger<int>(components[0]);
    auto maxPort = WTF::parseInteger<int>(components[1]);
    if (!minPort || !maxPort) {
        WTFLogAlways("Invalid format for UDP port range. Should be \"min-port:max-port\"");
        ASSERT_NOT_REACHED();
        return;
    }

    if (*minPort < 0) {
        WTFLogAlways("Invalid value for UDP minimum port value: %d", *minPort);
        return;
    }

    if (*maxPort < 0) {
        WTFLogAlways("Invalid value for UDP maximum port value: %d", *maxPort);
        return;
    }

    m_portAllocatorRange = { { *minPort, *maxPort } };
}

std::optional<std::pair<int, int>> WebRTCProvider::portAllocatorRange() const
{
    return m_portAllocatorRange;
}

} // namespace WebCore
