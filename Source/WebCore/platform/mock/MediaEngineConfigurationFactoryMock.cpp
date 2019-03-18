/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaEngineConfigurationFactoryMock.h"

#include "ContentType.h"
#include "MediaCapabilitiesDecodingInfo.h"
#include "MediaCapabilitiesEncodingInfo.h"
#include "MediaDecodingConfiguration.h"
#include "MediaEncodingConfiguration.h"

namespace WebCore {

static bool canDecodeMedia(const MediaDecodingConfiguration& configuration)
{
    // The mock implementation supports only local file playback.
    if (configuration.type == MediaDecodingType::MediaSource)
        return false;

    // Maxing out video decoding support at 720P.
    auto videoConfig = configuration.video;
    if (videoConfig && videoConfig->width > 1280 && videoConfig->height > 720)
        return false;

    // Only the "mock-with-alpha" codec supports alphaChannel
    if (videoConfig && videoConfig->alphaChannel && videoConfig->alphaChannel.value()) {
        if (ContentType(videoConfig->contentType).codecsParameter() != "mock-with-alpha")
            return false;
    }

    // Audio decoding support limited to audio/mp4.
    auto audioConfig = configuration.audio;
    if (audioConfig)
        return ContentType(audioConfig->contentType).containerType() == "audio/mp4";

    return true;
}

static bool canSmoothlyDecodeMedia(const MediaDecodingConfiguration& configuration)
{
    auto videoConfig = configuration.video;
    if (videoConfig && videoConfig->framerate > 30)
        return false;

    auto audioConfig = configuration.audio;
    if (audioConfig)
        return audioConfig->channels == "2";

    return true;
}

static bool canPowerEfficientlyDecodeMedia(const MediaDecodingConfiguration& configuration)
{
    auto videoConfig = configuration.video;
    if (videoConfig && ContentType(videoConfig->contentType).containerType() != "video/mp4")
        return false;

    auto audioConfig = configuration.audio;
    if (audioConfig)
        return audioConfig->bitrate <= 1000;

    return true;
}

static bool canEncodeMedia(const MediaEncodingConfiguration& configuration)
{
    // The mock implementation supports only local file playback.
    if (configuration.type == MediaEncodingType::Record)
        return false;

    // Maxing out video encoding support at 720P.
    auto videoConfig = configuration.video;
    if (videoConfig && videoConfig->width > 1280 && videoConfig->height > 720)
        return false;

    // Only the "mock-with-alpha" codec supports alphaChannel
    if (videoConfig && videoConfig->alphaChannel && videoConfig->alphaChannel.value()) {
        if (ContentType(videoConfig->contentType).codecsParameter() != "mock-with-alpha")
            return false;
    }

    // Audio encoding support limited to audio/mp4.
    auto audioConfig = configuration.audio;
    if (audioConfig && ContentType(audioConfig->contentType).containerType() != "audio/mp4")
        return false;

    return true;
}

static bool canSmoothlyEncodeMedia(const MediaEncodingConfiguration& configuration)
{
    auto videoConfig = configuration.video;
    if (videoConfig && videoConfig->framerate > 30)
        return false;

    auto audioConfig = configuration.audio;
    if (audioConfig && audioConfig->channels != "2")
        return false;

    return true;
}

static bool canPowerEfficientlyEncodeMedia(const MediaEncodingConfiguration& configuration)
{
    auto videoConfig = configuration.video;
    if (videoConfig && ContentType(videoConfig->contentType).containerType() != "video/mp4")
        return false;

    auto audioConfig = configuration.audio;
    if (audioConfig && audioConfig->bitrate > 1000)
        return false;

    return true;
}

void MediaEngineConfigurationFactoryMock::createDecodingConfiguration(MediaDecodingConfiguration&& configuration, DecodingConfigurationCallback&& callback)
{
    if (!canDecodeMedia(configuration)) {
        MediaCapabilitiesDecodingInfo info { WTFMove(configuration) };
        callback(WTFMove(info));
        return;
    }
    callback({{ true, canSmoothlyDecodeMedia(configuration), canPowerEfficientlyDecodeMedia(configuration) }, WTFMove(configuration)});
}

void MediaEngineConfigurationFactoryMock::createEncodingConfiguration(MediaEncodingConfiguration&& configuration, EncodingConfigurationCallback&& callback)
{
    if (!canEncodeMedia(configuration)) {
        callback({{ }, WTFMove(configuration) });
        return;
    }
    callback({{ true, canSmoothlyEncodeMedia(configuration), canPowerEfficientlyEncodeMedia(configuration) }, WTFMove(configuration)});
}

} // namespace WebCore
