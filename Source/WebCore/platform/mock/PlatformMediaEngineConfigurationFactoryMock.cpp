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
#include "PlatformMediaEngineConfigurationFactoryMock.h"

#include "ContentType.h"
#include "PlatformMediaCapabilitiesDecodingInfo.h"
#include "PlatformMediaCapabilitiesEncodingInfo.h"
#include "PlatformMediaDecodingConfiguration.h"
#include "PlatformMediaEncodingConfiguration.h"

namespace WebCore {

static bool canDecodeMedia(const PlatformMediaDecodingConfiguration& configuration)
{
    // The mock implementation supports only local file playback.
    if (configuration.type == PlatformMediaDecodingType::MediaSource)
        return false;

    // Maxing out video decoding support at 720P.
    auto videoConfig = configuration.video;
    if (videoConfig && videoConfig->width > 1280 && videoConfig->height > 720)
        return false;

    // Only the "mock-with-alpha" codec supports alphaChannel
    if (videoConfig && videoConfig->alphaChannel && videoConfig->alphaChannel.value()) {
        if (ContentType(videoConfig->contentType).parameter(ContentType::codecsParameter()) != "mock-with-alpha"_s)
            return false;
    }

    // Only the "mock-with-hdr" codec supports HDR)
    if (videoConfig && (videoConfig->colorGamut || videoConfig->hdrMetadataType || videoConfig->transferFunction)) {
        if (ContentType(videoConfig->contentType).parameter(ContentType::codecsParameter()) != "mock-with-hdr"_s)
            return false;
    }

    // Audio decoding support limited to audio/mp4.
    auto audioConfig = configuration.audio;
    if (audioConfig) {
        if (ContentType(audioConfig->contentType).containerType() != "audio/mp4"_s)
            return false;

        // Can only support spatial rendering of tracks with multichannel audio:
        if (audioConfig->spatialRendering.value_or(false) && audioConfig->channels.toDouble() <= 2)
            return false;
    }

    return true;
}

static bool canSmoothlyDecodeMedia(const PlatformMediaDecodingConfiguration& configuration)
{
    auto videoConfig = configuration.video;
    if (videoConfig && videoConfig->framerate > 30)
        return false;

    auto audioConfig = configuration.audio;
    if (audioConfig && !audioConfig->channels.isNull())
        return audioConfig->channels == "2"_s;

    return true;
}

static bool canPowerEfficientlyDecodeMedia(const PlatformMediaDecodingConfiguration& configuration)
{
    auto videoConfig = configuration.video;
    if (videoConfig && ContentType(videoConfig->contentType).containerType() != "video/mp4"_s)
        return false;

    auto audioConfig = configuration.audio;
    if (audioConfig && audioConfig->bitrate)
        return audioConfig->bitrate.value() <= 1000;

    return true;
}

static bool canEncodeMedia(const PlatformMediaEncodingConfiguration& configuration)
{
    ASSERT(configuration.type == PlatformMediaEncodingType::Record);
    if (configuration.type != PlatformMediaEncodingType::Record)
        return false;

    // Maxing out video encoding support at 720P.
    auto videoConfig = configuration.video;
    if (videoConfig && videoConfig->width > 1280 && videoConfig->height > 720)
        return false;

    // Only the "mock-with-alpha" codec supports alphaChannel
    if (videoConfig && videoConfig->alphaChannel && videoConfig->alphaChannel.value()) {
        if (ContentType(videoConfig->contentType).parameter(ContentType::codecsParameter()) != "mock-with-alpha"_s)
            return false;
    }

    // Audio encoding support limited to audio/mp4.
    auto audioConfig = configuration.audio;
    if (audioConfig && ContentType(audioConfig->contentType).containerType() != "audio/mp4"_s)
        return false;

    return true;
}

static bool canSmoothlyEncodeMedia(const PlatformMediaEncodingConfiguration& configuration)
{
    auto videoConfig = configuration.video;
    if (videoConfig && videoConfig->framerate > 30)
        return false;

    auto audioConfig = configuration.audio;
    if (audioConfig && !audioConfig->channels.isNull() && audioConfig->channels != "2"_s)
        return false;

    return true;
}

static bool canPowerEfficientlyEncodeMedia(const PlatformMediaEncodingConfiguration& configuration)
{
    auto videoConfig = configuration.video;
    if (videoConfig && ContentType(videoConfig->contentType).containerType() != "video/mp4"_s)
        return false;

    auto audioConfig = configuration.audio;
    if (audioConfig && audioConfig->bitrate && audioConfig->bitrate.value() > 1000)
        return false;

    return true;
}

void PlatformMediaEngineConfigurationFactoryMock::createDecodingConfiguration(PlatformMediaDecodingConfiguration&& configuration, DecodingConfigurationCallback&& callback)
{
    if (!canDecodeMedia(configuration)) {
        callback({ { }, WTF::move(configuration) });
        return;
    }
    callback({{ true, canSmoothlyDecodeMedia(configuration), canPowerEfficientlyDecodeMedia(configuration) }, WTF::move(configuration)});
}

void PlatformMediaEngineConfigurationFactoryMock::createEncodingConfiguration(PlatformMediaEncodingConfiguration&& configuration, EncodingConfigurationCallback&& callback)
{
    if (!canEncodeMedia(configuration)) {
        callback({ { }, WTF::move(configuration) });
        return;
    }
    callback({{ true, canSmoothlyEncodeMedia(configuration), canPowerEfficientlyEncodeMedia(configuration) }, WTF::move(configuration)});
}

} // namespace WebCore
