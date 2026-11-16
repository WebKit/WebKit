/*
 * Copyright (C) 2020 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "MediaStreamAudioSource.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && ENABLE(WEB_AUDIO)

#include "AudioBus.h"
#include "GStreamerAudioData.h"
#include "GStreamerAudioStreamDescription.h"
#include "GStreamerCommon.h"
#include "Logging.h"

GST_DEBUG_CATEGORY(webkit_mediastream_audio_source_debug);
#define GST_CAT_DEFAULT webkit_mediastream_audio_source_debug

namespace WebCore {

static void ensureMediaStreamAudioSourceDebugCategory()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_mediastream_audio_source_debug, "webkitmediastreamaudiosource", 0, "WebKit MediaStream Audio Source");
    });
}

static MediaTime mediaTimeFromFrames(size_t numberOfFrames, uint32_t sampleRate)
{
    return MediaTime((numberOfFrames * G_USEC_PER_SEC) / sampleRate, G_USEC_PER_SEC);
}

void MediaStreamAudioSource::consumeAudio(AudioBus& bus, size_t numberOfFrames)
{
    ensureMediaStreamAudioSourceDebugCategory();

    if (!bus.numberOfChannels() || bus.numberOfChannels() > 2) {
        GST_ERROR("Unable to consume WebAudio data with %u channels", bus.numberOfChannels());
        RELEASE_LOG_ERROR(Media, "MediaStreamAudioSource::consumeAudio(%p) trying to consume bus with %u channels", this, bus.numberOfChannels());
        return;
    }

    if (m_baseTime.isInvalid()) {
        GRefPtr clock = adoptGRef(gst_system_clock_obtain());
        m_baseTime = fromGstClockTime(gst_clock_get_time(clock.get()));
    }

    auto mediaTime = m_baseTime + mediaTimeFromFrames(m_numberOfFrames, m_currentSettings.sampleRate());
    auto duration = mediaTimeFromFrames(numberOfFrames, m_currentSettings.sampleRate());
    m_numberOfFrames += numberOfFrames;

    // Lazily initialize caps, the settings don't change so this is OK.
    if (!m_caps || GST_AUDIO_INFO_CHANNELS(&m_info) != static_cast<int>(bus.numberOfChannels())) {
        gst_audio_info_set_format(&m_info, GST_AUDIO_FORMAT_F32LE, m_currentSettings.sampleRate(), bus.numberOfChannels(), nullptr);
        GST_AUDIO_INFO_LAYOUT(&m_info) = GST_AUDIO_LAYOUT_NON_INTERLEAVED;
        m_caps = adoptGRef(gst_audio_info_to_caps(&m_info));
    }

    auto channels = bus.numberOfChannels();
    GRefPtr buffer = adoptGRef(gst_buffer_new_and_alloc(sizeof(float) * numberOfFrames * channels));
    GST_BUFFER_PTS(buffer.get()) = toGstClockTime(mediaTime);
    GST_BUFFER_DURATION(buffer.get()) = toGstClockTime(duration);
    GST_BUFFER_FLAG_SET(buffer.get(), GST_BUFFER_FLAG_LIVE);

    {
        GstMappedBuffer map(buffer, GST_MAP_WRITE);
        auto dest = map.mutableSpan<float>();
        for (size_t channel = 0; channel < channels; ++channel)
            memcpySpan(dest.subspan(channel * numberOfFrames, numberOfFrames), bus.channel(channel)->span());
    }

    gst_buffer_add_audio_meta(buffer.get(), &m_info, numberOfFrames, nullptr);
    if (bus.isSilent())
        gst_buffer_add_audio_level_meta(buffer.get(), 127, FALSE);

    GRefPtr sample = adoptGRef(gst_sample_new(buffer.get(), m_caps.get(), nullptr, nullptr));
    GStreamerAudioData audioBuffer(WTF::move(sample), m_info);
    GStreamerAudioStreamDescription description(&m_info);
    audioSamplesAvailable(mediaTime, audioBuffer, description, numberOfFrames);
}

} // namespace WebCore

#undef GST_CAT_DEFAULT

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && ENABLE(WEB_AUDIO)
