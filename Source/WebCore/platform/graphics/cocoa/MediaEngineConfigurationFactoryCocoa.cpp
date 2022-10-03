/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "MediaEngineConfigurationFactoryCocoa.h"

#if PLATFORM(COCOA)

#include "HEVCUtilitiesCocoa.h"
#include "MediaCapabilitiesDecodingInfo.h"
#include "MediaDecodingConfiguration.h"
#include "MediaPlayer.h"
#include "VP9UtilitiesCocoa.h"
#include <pal/avfoundation/OutputContext.h>
#include <pal/avfoundation/OutputDevice.h>
#include <wtf/Algorithms.h>

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/MediaCapabilitiesAdditions.h>
#endif

#include "VideoToolboxSoftLink.h"

namespace WebCore {

static CMVideoCodecType videoCodecTypeFromRFC4281Type(StringView type)
{
    if (type.startsWith("mp4v"_s))
        return kCMVideoCodecType_MPEG4Video;
    if (type.startsWith("avc1"_s) || type.startsWith("avc3"_s))
        return kCMVideoCodecType_H264;
    if (type.startsWith("hvc1"_s) || type.startsWith("hev1"_s))
        return kCMVideoCodecType_HEVC;
#if ENABLE(VP9)
    if (type.startsWith("vp09"_s))
        return kCMVideoCodecType_VP9;
#endif
    return 0;
}

static std::optional<MediaCapabilitiesInfo> computeMediaCapabilitiesInfo(const MediaDecodingConfiguration& configuration)
{
    MediaCapabilitiesInfo info;

    if (configuration.video) {
        auto& videoConfiguration = configuration.video.value();
        MediaEngineSupportParameters parameters { };
        parameters.allowedMediaContainerTypes = configuration.allowedMediaContainerTypes;
        parameters.allowedMediaCodecTypes = configuration.allowedMediaCodecTypes;

        switch (configuration.type) {
        case MediaDecodingType::File:
            parameters.isMediaSource = false;
            break;
        case MediaDecodingType::MediaSource:
            parameters.isMediaSource = true;
            break;
        case MediaDecodingType::WebRTC:
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }

        parameters.type = ContentType(videoConfiguration.contentType);
        if (MediaPlayer::supportsType(parameters) != MediaPlayer::SupportsType::IsSupported)
            return std::nullopt;

        auto codecs = parameters.type.codecs();
        if (codecs.size() != 1)
            return std::nullopt;

        info.supported = true;
        auto& codec = codecs[0];
        auto videoCodecType = videoCodecTypeFromRFC4281Type(codec);

        bool hdrSupported = videoConfiguration.colorGamut || videoConfiguration.hdrMetadataType || videoConfiguration.transferFunction;
        bool alphaChannel = videoConfiguration.alphaChannel && videoConfiguration.alphaChannel.value();

        if (videoCodecType == kCMVideoCodecType_HEVC) {
            auto parameters = parseHEVCCodecParameters(codec);
            if (!parameters)
                return std::nullopt;
            auto parsedInfo = validateHEVCParameters(*parameters, alphaChannel, hdrSupported);
            if (!parsedInfo)
                return std::nullopt;
            info = *parsedInfo;
        } else if (codec.startsWith("dvh1"_s) || codec.startsWith("dvhe"_s)) {
            auto parameters = parseDoViCodecParameters(codec);
            if (!parameters)
                return std::nullopt;
            auto parsedInfo = validateDoViParameters(*parameters, alphaChannel, hdrSupported);
            if (!parsedInfo)
                return std::nullopt;
            info = *parsedInfo;
#if ENABLE(VP9)
        } else if (videoCodecType == kCMVideoCodecType_VP9) {
            if (!configuration.canExposeVP9)
                return std::nullopt;
            auto parameters = parseVPCodecParameters(codec);
            if (!parameters)
                return std::nullopt;
            auto parsedInfo = validateVPParameters(*parameters, videoConfiguration);
            if (!parsedInfo)
                return std::nullopt;
            info = *parsedInfo;
#endif
#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/MediaCapabilitiesAdditions.cpp>
#endif
        } else if (videoCodecType) {
            if (alphaChannel || hdrSupported)
                return std::nullopt;

            if (canLoad_VideoToolbox_VTIsHardwareDecodeSupported()) {
                info.powerEfficient = VTIsHardwareDecodeSupported(videoCodecType);
                info.smooth = true;
            }
        } else
            return std::nullopt;
    }

    if (configuration.audio) {
        MediaEngineSupportParameters parameters { };
        parameters.type = ContentType(configuration.audio.value().contentType);
        parameters.isMediaSource = configuration.type == MediaDecodingType::MediaSource;
        parameters.allowedMediaContainerTypes = configuration.allowedMediaContainerTypes;
        parameters.allowedMediaCodecTypes = configuration.allowedMediaCodecTypes;

        if (MediaPlayer::supportsType(parameters) != MediaPlayer::SupportsType::IsSupported)
            return std::nullopt;

        if (configuration.audio->spatialRendering.value_or(false)) {
            auto context = PAL::OutputContext::sharedAudioPresentationOutputContext();
            if (!context)
                return std::nullopt;

            auto devices = context->outputDevices();
            if (devices.isEmpty() || !WTF::allOf(devices, [](auto& device) {
                return device.supportsSpatialAudio();
            }))
                return std::nullopt;

            // Only multichannel audio can be spatially rendered.
            if (!configuration.audio->channels.isNull() && configuration.audio->channels.toDouble() <= 2)
                return std::nullopt;
        }
        info.supported = true;
    }

    return info;
}

void createMediaPlayerDecodingConfigurationCocoa(MediaDecodingConfiguration&& configuration, Function<void(MediaCapabilitiesDecodingInfo&&)>&& callback)
{
    auto info = computeMediaCapabilitiesInfo(configuration);
    if (!info)
        callback({ { }, WTFMove(configuration) });
    else {
        MediaCapabilitiesDecodingInfo infoWithConfiguration = { WTFMove(*info), WTFMove(configuration) };
        callback(WTFMove(infoWithConfiguration));
    }
}

}
#endif
