/*
 * Copyright (C) 2017 Igalia S.L
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

#if USE(LIBWEBRTC) && USE(GSTREAMER)
#include "RealtimeOutgoingAudioSourceLibWebRTC.h"

#include "LibWebRTCAudioFormat.h"
#include "LibWebRTCProvider.h"
#include "NotImplemented.h"
#include "gstreamer/GStreamerAudioData.h"

namespace WebCore {

RealtimeOutgoingAudioSourceLibWebRTC::RealtimeOutgoingAudioSourceLibWebRTC(Ref<MediaStreamTrackPrivate>&& audioSource)
    : RealtimeOutgoingAudioSource(WTFMove(audioSource))
{
    m_adapter = adoptGRef(gst_adapter_new()),
    m_sampleConverter = nullptr;
}

RealtimeOutgoingAudioSourceLibWebRTC::~RealtimeOutgoingAudioSourceLibWebRTC()
{
    unobserveSource();
    if (m_sampleConverter)
        g_clear_pointer(&m_sampleConverter, gst_audio_converter_free);
}

Ref<RealtimeOutgoingAudioSource> RealtimeOutgoingAudioSource::create(Ref<MediaStreamTrackPrivate>&& audioSource)
{
    return RealtimeOutgoingAudioSourceLibWebRTC::create(WTFMove(audioSource));
}

static inline std::unique_ptr<GStreamerAudioStreamDescription> libwebrtcAudioFormat(int sampleRate,
    size_t channelCount)
{
    GstAudioFormat format = gst_audio_format_build_integer(
        LibWebRTCAudioFormat::isSigned,
        LibWebRTCAudioFormat::isBigEndian ? G_BIG_ENDIAN : G_LITTLE_ENDIAN,
        LibWebRTCAudioFormat::sampleSize,
        LibWebRTCAudioFormat::sampleSize);

    GstAudioInfo info;

    size_t libWebRTCChannelCount = channelCount >= 2 ? 2 : channelCount;
    gst_audio_info_set_format(&info, format, sampleRate, libWebRTCChannelCount, nullptr);

    return std::unique_ptr<GStreamerAudioStreamDescription>(new GStreamerAudioStreamDescription(info));
}

void RealtimeOutgoingAudioSourceLibWebRTC::audioSamplesAvailable(const MediaTime&,
    const PlatformAudioData& audioData, const AudioStreamDescription& streamDescription,
    size_t /* sampleCount */)
{
    auto data = static_cast<const GStreamerAudioData&>(audioData);
    auto desc = static_cast<const GStreamerAudioStreamDescription&>(streamDescription);

    if (m_sampleConverter && !gst_audio_info_is_equal(m_inputStreamDescription->getInfo(), desc.getInfo())) {
        GST_ERROR_OBJECT(this, "FIXME - Audio format renegotiation is not possible yet!");
        g_clear_pointer(&m_sampleConverter, gst_audio_converter_free);
    }

    if (!m_sampleConverter) {
        m_inputStreamDescription = std::unique_ptr<GStreamerAudioStreamDescription>(new GStreamerAudioStreamDescription(desc.getInfo()));
        m_outputStreamDescription = libwebrtcAudioFormat(LibWebRTCAudioFormat::sampleRate, streamDescription.numberOfChannels());

        m_sampleConverter = gst_audio_converter_new(GST_AUDIO_CONVERTER_FLAG_IN_WRITABLE,
            m_inputStreamDescription->getInfo(),
            m_outputStreamDescription->getInfo(),
            nullptr);
    }

    LockHolder locker(m_adapterMutex);
    auto buffer = gst_sample_get_buffer(data.getSample());
    gst_adapter_push(m_adapter.get(), gst_buffer_ref(buffer));
    LibWebRTCProvider::callOnWebRTCSignalingThread([protectedThis = makeRef(*this)] {
        protectedThis->pullAudioData();
    });
}

void RealtimeOutgoingAudioSourceLibWebRTC::pullAudioData()
{
    if (!m_inputStreamDescription || !m_outputStreamDescription) {
        GST_INFO("No stream description set yet.");

        return;
    }

    size_t outChunkSampleCount = LibWebRTCAudioFormat::chunkSampleCount;
    size_t outBufferSize = outChunkSampleCount * m_outputStreamDescription->getInfo()->bpf;

    LockHolder locker(m_adapterMutex);
    size_t inChunkSampleCount = gst_audio_converter_get_in_frames(m_sampleConverter, outChunkSampleCount);
    size_t inBufferSize = inChunkSampleCount * m_inputStreamDescription->getInfo()->bpf;

    auto available = gst_adapter_available(m_adapter.get());
    if (inBufferSize > available) {
        GST_DEBUG("Not enough data: wanted: %ld > %ld available",
            inBufferSize, available);

        return;
    }

    auto inBuffer = adoptGRef(gst_adapter_take_buffer(m_adapter.get(), inBufferSize));
    auto outBuffer = adoptGRef(gst_buffer_new_allocate(nullptr, outBufferSize, 0));
    GstMappedBuffer outMap(outBuffer.get(), GST_MAP_WRITE);
    if (isSilenced())
        gst_audio_format_fill_silence(m_outputStreamDescription->getInfo()->finfo, outMap.data(), outMap.size());
    else {
        GstMappedBuffer inMap(inBuffer.get(), GST_MAP_READ);

        gpointer in[1] = { inMap.data() };
        gpointer out[1] = { outMap.data() };
        if (!gst_audio_converter_samples(m_sampleConverter, static_cast<GstAudioConverterFlags>(0), in, inChunkSampleCount, out, outChunkSampleCount)) {
            GST_ERROR("Could not convert samples.");

            return;
        }
    }

    sendAudioFrames(outMap.data(), LibWebRTCAudioFormat::sampleSize, static_cast<int>(m_outputStreamDescription->sampleRate()),
        static_cast<int>(m_outputStreamDescription->numberOfChannels()), outChunkSampleCount);
}

bool RealtimeOutgoingAudioSourceLibWebRTC::isReachingBufferedAudioDataHighLimit()
{
    notImplemented();
    return false;
}

bool RealtimeOutgoingAudioSourceLibWebRTC::isReachingBufferedAudioDataLowLimit()
{
    notImplemented();
    return false;
}

bool RealtimeOutgoingAudioSourceLibWebRTC::hasBufferedEnoughData()
{
    notImplemented();
    return false;
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
