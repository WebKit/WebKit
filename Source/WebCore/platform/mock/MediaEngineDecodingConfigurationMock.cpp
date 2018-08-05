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
#include "MediaEngineDecodingConfigurationMock.h"

#include "ContentType.h"
#include "IntSize.h"

namespace WebCore {

bool MediaEngineDecodingConfigurationMock::canDecodeMedia()
{
    // The mock implementation supports only local file playback.
    if (decodingType() == MediaDecodingType::MediaSource)
        return false;

    // Maxing out video decoding support at 720P.
    auto videoConfig = videoConfiguration();
    if (videoConfig) {
        IntSize size = videoConfig->size();
        if (size.width() > 1280 && size.height() > 720)
            return false;
    }

    // Audio decoding support limited to audio/mp4.
    auto audioConfig = audioConfiguration();
    if (audioConfig)
        return audioConfig->contentType().containerType() == "audio/mp4";

    return true;
}

bool MediaEngineDecodingConfigurationMock::canSmoothlyDecodeMedia()
{
    auto videoConfig = videoConfiguration();
    if (videoConfig) {
        if (videoConfig->framerate() > 30)
            return false;
    }

    RefPtr<MediaEngineAudioConfiguration> audioConfig = audioConfiguration();
    if (audioConfig)
        return audioConfig->channels() == "2";

    return true;
}

bool MediaEngineDecodingConfigurationMock::canPowerEfficientlyDecodeMedia()
{
    auto videoConfig = videoConfiguration();
    if (videoConfig) {
        if (videoConfig->contentType().containerType() != "video/mp4")
            return false;
    }

    auto audioConfig = audioConfiguration();
    if (audioConfig)
        return audioConfig->bitrate() <= 1000;

    return true;
}

} // namespace WebCore
