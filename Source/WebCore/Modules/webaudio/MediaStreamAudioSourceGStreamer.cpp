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
#include "Logging.h"

namespace WebCore {

static void copyBusData(AudioBus& bus, GstBuffer* buffer, bool isMuted)
{
    GstMappedBuffer mappedBuffer(buffer, GST_MAP_WRITE);
    if (isMuted) {
        memset(mappedBuffer.data(), 0, mappedBuffer.size());
        return;
    }

    size_t offset = 0;
    for (size_t channelIndex = 0; channelIndex < bus.numberOfChannels(); ++channelIndex) {
        const auto& channel = *bus.channel(channelIndex);
        auto dataSize = sizeof(float) * channel.length();
        memcpy(mappedBuffer.data() + offset, channel.data(), dataSize);
        offset += dataSize;
    }
}

void MediaStreamAudioSource::consumeAudio(AudioBus& bus, size_t numberOfFrames)
{
    if (!bus.numberOfChannels() || bus.numberOfChannels() > 2) {
        RELEASE_LOG_ERROR(Media, "MediaStreamAudioSource::consumeAudio(%p) trying to consume bus with %u channels", this, bus.numberOfChannels());
        return;
    }

    MediaTime mediaTime((m_numberOfFrames * G_USEC_PER_SEC) / m_currentSettings.sampleRate(), G_USEC_PER_SEC);
    m_numberOfFrames += numberOfFrames;

    GstAudioInfo info;
    gst_audio_info_set_format(&info, GST_AUDIO_FORMAT_F32LE, m_currentSettings.sampleRate(), bus.numberOfChannels(), nullptr);
    GST_AUDIO_INFO_LAYOUT(&info) = GST_AUDIO_LAYOUT_NON_INTERLEAVED;
    size_t size = GST_AUDIO_INFO_BPS(&info) * bus.numberOfChannels() * numberOfFrames;

    auto caps = adoptGRef(gst_audio_info_to_caps(&info));
    auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, size, nullptr));

    GST_BUFFER_PTS(buffer.get()) = toGstClockTime(mediaTime);
    GST_BUFFER_FLAG_SET(buffer.get(), GST_BUFFER_FLAG_LIVE);

    copyBusData(bus, buffer.get(), muted());
    gst_buffer_add_audio_meta(buffer.get(), &info, numberOfFrames, nullptr);
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    GStreamerAudioData audioBuffer(WTFMove(sample), info);
    GStreamerAudioStreamDescription description(&info);
    audioSamplesAvailable(mediaTime, audioBuffer, description, numberOfFrames);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && ENABLE(WEB_AUDIO)
