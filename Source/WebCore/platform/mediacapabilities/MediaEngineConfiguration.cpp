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
#include "MediaEngineConfiguration.h"

namespace WebCore {

MediaEngineVideoConfiguration::MediaEngineVideoConfiguration(VideoConfiguration&& config)
    : m_type(config.contentType)
    , m_width(config.width)
    , m_height(config.height)
    , m_bitrate(config.bitrate)
    , m_frameRateNumerator(0)
    , m_frameRateDenominator(1)
{
    bool ok = false;
    m_frameRateNumerator = config.framerate.toDouble(&ok);
    if (ok)
        return;

    auto frameratePieces = config.framerate.split('/');
    if (frameratePieces.size() != 2)
        return;

    double numerator = frameratePieces[0].toDouble(&ok);
    if (!ok || !numerator)
        return;

    double denominator = frameratePieces[1].toDouble(&ok);
    if (!ok || !denominator)
        return;

    if (!std::isfinite(numerator) || !std::isfinite(denominator))
        return;

    m_frameRateNumerator = numerator;
    m_frameRateDenominator = denominator;
}

MediaEngineAudioConfiguration::MediaEngineAudioConfiguration(AudioConfiguration&& config)
    : m_type(config.contentType)
    , m_channels(config.channels)
    , m_bitrate(0)
    , m_samplerate(0)
{
    if (config.bitrate)
        m_bitrate = config.bitrate.value();

    if (config.samplerate)
        m_samplerate = config.samplerate.value();
}

MediaEngineConfiguration::MediaEngineConfiguration(MediaConfiguration&& config)
{
    if (config.audio)
        m_audioConfiguration = MediaEngineAudioConfiguration::create(config.audio.value());

    if (config.video)
        m_videoConfiguration = MediaEngineVideoConfiguration::create(config.video.value());
}

} // namespace WebCore
