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
#include "VideoTrackPrivateWebM.h"

#if ENABLE(MEDIA_SOURCE)

#include "MediaSample.h"

namespace WebCore {

Ref<VideoTrackPrivateWebM> VideoTrackPrivateWebM::create(webm::TrackEntry&& trackEntry)
{
    return adoptRef(*new VideoTrackPrivateWebM(WTFMove(trackEntry)));
}

VideoTrackPrivateWebM::VideoTrackPrivateWebM(webm::TrackEntry&& trackEntry)
    : m_track(WTFMove(trackEntry))
{
    if (m_track.is_enabled.is_present())
        setSelected(m_track.is_enabled.value());

    updateConfiguration();
}

void VideoTrackPrivateWebM::setFormatDescription(Ref<VideoInfo>&& formatDescription)
{
    if (m_formatDescription && *m_formatDescription == formatDescription)
        return;
    m_formatDescription = WTFMove(formatDescription);
    updateConfiguration();
}

TrackID VideoTrackPrivateWebM::id() const
{
    if (m_track.track_uid.is_present())
        return m_track.track_uid.value();
    if (m_track.track_number.is_present())
        return m_track.track_number.value();
    ASSERT_NOT_REACHED();
    return 0;
}

std::optional<bool> VideoTrackPrivateWebM::defaultEnabled() const
{
    if (m_track.is_enabled.is_present())
        return m_track.is_enabled.value();
    return std::nullopt;
}

AtomString VideoTrackPrivateWebM::label() const
{
    if (m_label.isNull())
        m_label = m_track.name.is_present() ? AtomString::fromUTF8(m_track.name.value().data(), m_track.name.value().length()) : emptyAtom();
    return m_label;
}

AtomString VideoTrackPrivateWebM::language() const
{
    if (m_language.isNull())
        m_language = m_track.language.is_present() ? AtomString::fromUTF8(m_track.language.value().data(), m_track.language.value().length()) : emptyAtom();
    return m_language;
}

int VideoTrackPrivateWebM::trackIndex() const
{
    if (m_track.track_number.is_present())
        return m_track.track_number.value();
    return 0;
}

String VideoTrackPrivateWebM::codec() const
{
    if (m_formatDescription) {
        if (!m_formatDescription->codecString.isEmpty())
            return m_formatDescription->codecString;
        return String::fromLatin1(m_formatDescription->codecName.string().data());
    }

    if (!m_track.codec_id.is_present())
        return emptyString();

    StringView codecID { m_track.codec_id.value().data(), (unsigned)m_track.codec_id.value().length() };

    if (codecID == "V_VP9"_s)
        return "vp09"_s;

    if (codecID == "V_VP8"_s)
        return "vp08"_s;

    return emptyString();
}

uint32_t VideoTrackPrivateWebM::width() const
{
    if (m_formatDescription)
        return m_formatDescription->size.width();

    if (!m_track.video.is_present())
        return 0;

    auto& video = m_track.video.value();
    if (video.display_width.is_present())
        return video.display_width.value();

    if (video.pixel_width.is_present())
        return video.pixel_width.value();

    return 0;
}

uint32_t VideoTrackPrivateWebM::height() const
{
    if (m_formatDescription)
        return m_formatDescription->size.height();

    if (!m_track.video.is_present())
        return 0;

    auto& video = m_track.video.value();
    if (video.display_height.is_present())
        return video.display_height.value();

    if (video.pixel_height.is_present())
        return video.pixel_height.value();

    return 0;
}

double VideoTrackPrivateWebM::framerate() const
{
    if (!m_track.video.is_present())
        return 0;

    auto& video = m_track.video.value();
    if (video.frame_rate.is_present())
        return video.frame_rate.value();

    if (m_track.default_duration.is_present()) {
        static constexpr double nanosecondsPerSecond = 1000 * 1000 * 1000;
        return nanosecondsPerSecond / m_track.default_duration.value();
    }

    return 0;
}

PlatformVideoColorSpace VideoTrackPrivateWebM::colorSpace() const
{
    if (m_formatDescription)
        return m_formatDescription->colorSpace;
    return { };
}

void VideoTrackPrivateWebM::updateConfiguration()
{
    PlatformVideoTrackConfiguration configuration {
        { .codec = codec() },
        .width = width(),
        .height = height(),
        .framerate = framerate(),
        .colorSpace = colorSpace(),
    };
    setConfiguration(WTFMove(configuration));
}

}

#endif // ENABLE(MEDIA_SOURCE)
