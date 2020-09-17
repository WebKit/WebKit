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

#include "VideoToolboxSoftLink.h"

namespace WebCore {

static CMVideoCodecType videoCodecTypeFromRFC4281Type(String type)
{
    if (type.startsWith("mp4v"))
        return kCMVideoCodecType_MPEG4Video;
    if (type.startsWith("avc1") || type.startsWith("avc3"))
        return kCMVideoCodecType_H264;
    if (type.startsWith("hvc1") || type.startsWith("hev1"))
        return kCMVideoCodecType_HEVC;
#if ENABLE(VP9)
    if (type.startsWith("vp09"))
        return kCMVideoCodecType_VP9;
#endif
    return 0;
}

void createMediaPlayerDecodingConfigurationCocoa(MediaDecodingConfiguration&& configuration, WTF::Function<void(MediaCapabilitiesDecodingInfo&&)>&& callback)
{
    MediaCapabilitiesDecodingInfo info;

    if (configuration.video) {
        auto& videoConfiguration = configuration.video.value();
        MediaEngineSupportParameters parameters { };
        parameters.type = ContentType(videoConfiguration.contentType);
        parameters.isMediaSource = configuration.type == MediaDecodingType::MediaSource;
        if (MediaPlayer::supportsType(parameters) != MediaPlayer::SupportsType::IsSupported) {
            callback({{ }, WTFMove(configuration)});
            return;
        }

        auto codecs = parameters.type.codecs();
        if (codecs.size() != 1) {
            callback({{ }, WTFMove(configuration)});
            return;
        }

        info.supported = true;
        auto& codec = codecs[0];
        auto videoCodecType = videoCodecTypeFromRFC4281Type(codec);
        if (!videoCodecType && !(codec.startsWith("dvh1") || codec.startsWith("dvhe"))) {
            callback({{ }, WTFMove(configuration)});
            return;
        }

        bool hdrSupported = videoConfiguration.colorGamut || videoConfiguration.hdrMetadataType || videoConfiguration.transferFunction;
        bool alphaChannel = videoConfiguration.alphaChannel && videoConfiguration.alphaChannel.value();

        if (videoCodecType == kCMVideoCodecType_HEVC) {
            auto parameters = parseHEVCCodecParameters(codec);
            if (!parameters || !validateHEVCParameters(parameters.value(), info, alphaChannel, hdrSupported)) {
                callback({{ }, WTFMove(configuration)});
                return;
            }
        } else if (codec.startsWith("dvh1") || codec.startsWith("dvhe")) {
            auto parameters = parseDoViCodecParameters(codec);
            if (!parameters || !validateDoViParameters(parameters.value(), info, alphaChannel, hdrSupported)) {
                callback({{ }, WTFMove(configuration)});
                return;
            }
#if ENABLE(VP9)
        } else if (videoCodecType == kCMVideoCodecType_VP9) {
            auto codecConfiguration = parseVPCodecParameters(codec);
            if (!codecConfiguration || !validateVPParameters(*codecConfiguration, info, videoConfiguration)) {
                callback({{ }, WTFMove(configuration)});
                return;
            }
#endif
        } else {
            if (alphaChannel || hdrSupported) {
                callback({{ }, WTFMove(configuration)});
                return;
            }

            if (canLoad_VideoToolbox_VTIsHardwareDecodeSupported()) {
                info.powerEfficient = VTIsHardwareDecodeSupported(videoCodecType);
                info.smooth = true;
            }
        }
    }

    if (configuration.audio) {
        MediaEngineSupportParameters parameters { };
        parameters.type = ContentType(configuration.audio.value().contentType);
        parameters.isMediaSource = configuration.type == MediaDecodingType::MediaSource;
        if (MediaPlayer::supportsType(parameters) != MediaPlayer::SupportsType::IsSupported) {
            callback({{ }, WTFMove(configuration)});
            return;
        }
        info.supported = true;
    }

    info.supportedConfiguration = WTFMove(configuration);

    callback(WTFMove(info));
}

}
#endif
