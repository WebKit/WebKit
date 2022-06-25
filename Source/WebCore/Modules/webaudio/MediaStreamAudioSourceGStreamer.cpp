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

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "AudioBus.h"
#include "GStreamerAudioData.h"
#include "GStreamerAudioStreamDescription.h"
#include "Logging.h"

namespace WebCore {

static Vector<size_t> copyBusData(AudioBus& bus, GstBuffer* buffer, bool isMuted)
{
    Vector<size_t> offsets;
    GstMappedBuffer mappedBuffer(buffer, GST_MAP_WRITE);
    if (isMuted) {
        memset(mappedBuffer.data(), 0, mappedBuffer.size());
        return offsets;
    }

    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    offsets.reserveInitialCapacity(sizeof(size_t) * bus.numberOfChannels());
    size_t size = mappedBuffer.size() / bus.numberOfChannels();
    for (size_t channelIndex = 0; channelIndex < bus.numberOfChannels(); ++channelIndex) {
        const auto& channel = *bus.channel(channelIndex);
        auto offset = channelIndex * size;
        memcpy(mappedBuffer.data() + offset, channel.data(), sizeof(float) * channel.length());
        offsets.uncheckedAppend(offset);
    }
    return offsets;
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
    size_t size = GST_AUDIO_INFO_BPF(&info) * bus.numberOfChannels() * numberOfFrames;

    auto caps = adoptGRef(gst_audio_info_to_caps(&info));
    auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, size, nullptr));
    auto offsets = copyBusData(bus, buffer.get(), muted());
#if GST_CHECK_VERSION(1, 16, 0)
    gst_buffer_add_audio_meta(buffer.get(), &info, numberOfFrames, offsets.data());
#else
    UNUSED_VARIABLE(offsets);
#endif
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    GStreamerAudioData audioBuffer(WTFMove(sample), info);
    GStreamerAudioStreamDescription description(&info);
    audioSamplesAvailable(mediaTime, audioBuffer, description, numberOfFrames);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
