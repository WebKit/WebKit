/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "AudioTrackPrivateWebM.h"

#if ENABLE(MEDIA_SOURCE)

#include "MediaSample.h"

namespace WebCore {

Ref<AudioTrackPrivateWebM> AudioTrackPrivateWebM::create(webm::TrackEntry&& trackEntry)
{
    return adoptRef(*new AudioTrackPrivateWebM(WTFMove(trackEntry)));
}

AudioTrackPrivateWebM::AudioTrackPrivateWebM(webm::TrackEntry&& trackEntry)
    : m_track(WTFMove(trackEntry))
{
    if (m_track.is_enabled.is_present())
        setEnabled(m_track.is_enabled.value());
}

TrackID AudioTrackPrivateWebM::id() const
{
    if (m_track.track_uid.is_present())
        return m_track.track_uid.value();
    if (m_track.track_number.is_present())
        return m_track.track_number.value();
    ASSERT_NOT_REACHED();
    return 0;
}

std::optional<bool> AudioTrackPrivateWebM::defaultEnabled() const
{
    if (m_track.is_enabled.is_present())
        return m_track.is_enabled.value();
    return std::nullopt;
}

AtomString AudioTrackPrivateWebM::label() const
{
    if (m_label.isNull())
        m_label = m_track.name.is_present() ? AtomString::fromUTF8(m_track.name.value().data(), m_track.name.value().length()) : emptyAtom();
    return m_label;
}

AtomString AudioTrackPrivateWebM::language() const
{
    if (m_language.isNull())
        m_language = m_track.language.is_present() ? AtomString::fromUTF8(m_track.language.value().data(), m_track.language.value().length()) : emptyAtom();
    return m_language;
}

int AudioTrackPrivateWebM::trackIndex() const
{
    if (m_track.track_number.is_present())
        return m_track.track_number.value();
    return 0;
}

std::optional<MediaTime> AudioTrackPrivateWebM::codecDelay() const
{
    if (!m_track.codec_delay.is_present())
        return { };
    constexpr uint32_t k_us_in_seconds = 1000000000;
    return MediaTime(m_track.codec_delay.value(), k_us_in_seconds);
}

void AudioTrackPrivateWebM::setDiscardPadding(const MediaTime& discardPadding)
{
    m_discardPadding = discardPadding;
}

std::optional<MediaTime> AudioTrackPrivateWebM::discardPadding() const
{
    if (m_discardPadding.isInvalid() || m_discardPadding < MediaTime())
        return { };
    return m_discardPadding;
}

String AudioTrackPrivateWebM::codec() const
{
    if (m_formatDescription) {
        if (!m_formatDescription->codecString.isEmpty())
            return m_formatDescription->codecString;
        return String::fromLatin1(m_formatDescription->codecName.string().data());
    }

    if (!m_track.codec_id.is_present())
        return emptyString();

    StringView codecID { m_track.codec_id.value().data(), (unsigned)m_track.codec_id.value().length() };

    if (codecID == "A_VORBIS"_s)
        return "vorbis"_s;

    if (codecID == "V_OPUS"_s)
        return "opus"_s;

    return emptyString();
}

uint32_t AudioTrackPrivateWebM::sampleRate() const
{
    if (m_formatDescription)
        return m_formatDescription->rate;

    if (!m_track.audio.is_present())
        return 0;

    auto& audio = m_track.audio.value();
    if (audio.sampling_frequency.is_present())
        return audio.sampling_frequency.value();

    return 0;
}

uint32_t AudioTrackPrivateWebM::numberOfChannels() const
{
    if (m_formatDescription)
        return m_formatDescription->channels;

    if (!m_track.audio.is_present())
        return 0;

    auto& audio = m_track.audio.value();
    if (audio.channels.is_present())
        return audio.channels.value();

    return 0;
}

void AudioTrackPrivateWebM::setFormatDescription(Ref<AudioInfo>&& formatDescription)
{
    if (m_formatDescription && *m_formatDescription == formatDescription)
        return;
    m_formatDescription = WTFMove(formatDescription);
    updateConfiguration();
}

void AudioTrackPrivateWebM::updateConfiguration()
{
IGNORE_WARNINGS_BEGIN("c99-designator")
    PlatformAudioTrackConfiguration configuration {
        { .codec = codec() },
        .sampleRate = sampleRate(),
        .numberOfChannels = numberOfChannels(),
    };
IGNORE_WARNINGS_END
    setConfiguration(WTFMove(configuration));
}

}

#endif // ENABLE(MEDIA_SOURCE)
