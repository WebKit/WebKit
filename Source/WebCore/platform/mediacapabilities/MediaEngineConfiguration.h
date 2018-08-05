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

#pragma once

#include "AudioConfiguration.h"
#include "ContentType.h"
#include "IntSize.h"
#include "MediaConfiguration.h"
#include "VideoConfiguration.h"

#include <wtf/Forward.h>

namespace WebCore {
class MediaEngineConfiguration;

class MediaEngineVideoConfiguration : public RefCounted<MediaEngineVideoConfiguration> {
public:
    static Ref<MediaEngineVideoConfiguration> create(VideoConfiguration& config)
    {
        return adoptRef(*new MediaEngineVideoConfiguration(WTFMove(config)));
    };

    ContentType contentType() const { return m_type; };
    IntSize size() const { return IntSize(m_width, m_height); };
    uint64_t bitrate() const { return m_bitrate; };
    double framerate() const { return m_frameRateNumerator / m_frameRateDenominator; };

private:
    MediaEngineVideoConfiguration(VideoConfiguration&&);

    ContentType m_type;
    uint32_t m_width;
    uint32_t m_height;
    uint64_t m_bitrate;
    double m_frameRateNumerator;
    double m_frameRateDenominator;
};

class MediaEngineAudioConfiguration : public RefCounted<MediaEngineAudioConfiguration> {
public:
    static Ref<MediaEngineAudioConfiguration> create(AudioConfiguration& config)
    {
        return adoptRef(*new MediaEngineAudioConfiguration(WTFMove(config)));
    };

    ContentType contentType() const { return m_type; };
    String channels() const { return m_channels; };
    uint64_t bitrate() const { return m_bitrate; };
    uint32_t samplerate() const { return m_samplerate; };

private:
    MediaEngineAudioConfiguration(AudioConfiguration&&);

    ContentType m_type;
    String m_channels;
    uint64_t m_bitrate;
    uint32_t m_samplerate;
};

class MediaEngineConfiguration : public RefCounted<MediaEngineConfiguration> {
public:
    MediaEngineConfiguration(MediaConfiguration&&);
    virtual ~MediaEngineConfiguration() = default;

    enum class ImplementationType {
        Mock,
    };

    virtual ImplementationType implementationType() const = 0;

    RefPtr<MediaEngineAudioConfiguration> audioConfiguration() const { return m_audioConfiguration; };
    RefPtr<MediaEngineVideoConfiguration> videoConfiguration() const { return m_videoConfiguration; };

private:
    RefPtr<MediaEngineAudioConfiguration> m_audioConfiguration;
    RefPtr<MediaEngineVideoConfiguration> m_videoConfiguration;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_MEDIA_ENGINE_CONFIGURATION(ToValueTypeName, ImplementationTypeName) \
    SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName)                       \
    static bool isType(const WebCore::MediaEngineConfiguration& config) { return config.implementationType() == ImplementationTypeName; } \
    SPECIALIZE_TYPE_TRAITS_END()
