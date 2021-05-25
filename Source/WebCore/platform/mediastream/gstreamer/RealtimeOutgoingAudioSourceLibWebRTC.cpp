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

#include "GStreamerAudioData.h"
#include "GStreamerAudioStreamDescription.h"
#include "LibWebRTCAudioFormat.h"
#include "LibWebRTCProvider.h"
#include "NotImplemented.h"

namespace WebCore {

RealtimeOutgoingAudioSourceLibWebRTC::RealtimeOutgoingAudioSourceLibWebRTC(Ref<MediaStreamTrackPrivate>&& audioSource)
    : RealtimeOutgoingAudioSource(WTFMove(audioSource))
{
    m_adapter = adoptGRef(gst_adapter_new()),
    m_sampleConverter = nullptr;
}

RealtimeOutgoingAudioSourceLibWebRTC::~RealtimeOutgoingAudioSourceLibWebRTC()
{
    m_sampleConverter = nullptr;
}

Ref<RealtimeOutgoingAudioSource> RealtimeOutgoingAudioSource::create(Ref<MediaStreamTrackPrivate>&& audioSource)
{
    return RealtimeOutgoingAudioSourceLibWebRTC::create(WTFMove(audioSource));
}

static inline GstAudioInfo libwebrtcAudioFormat(int sampleRate, size_t channelCount)
{
    GstAudioFormat format = gst_audio_format_build_integer(
        LibWebRTCAudioFormat::isSigned,
        LibWebRTCAudioFormat::isBigEndian ? G_BIG_ENDIAN : G_LITTLE_ENDIAN,
        LibWebRTCAudioFormat::sampleSize,
        LibWebRTCAudioFormat::sampleSize);

    GstAudioInfo info;

    size_t libWebRTCChannelCount = channelCount >= 2 ? 2 : channelCount;
    gst_audio_info_set_format(&info, format, sampleRate, libWebRTCChannelCount, nullptr);
    return info;
}

void RealtimeOutgoingAudioSourceLibWebRTC::audioSamplesAvailable(const MediaTime&, const PlatformAudioData& audioData, const AudioStreamDescription& streamDescription, size_t /* sampleCount */)
{
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    auto data = static_cast<const GStreamerAudioData&>(audioData);
    auto desc = static_cast<const GStreamerAudioStreamDescription&>(streamDescription);

    if (m_sampleConverter && !gst_audio_info_is_equal(&m_inputStreamDescription, &desc.getInfo())) {
        GST_ERROR_OBJECT(this, "FIXME - Audio format renegotiation is not possible yet!");
        m_sampleConverter = nullptr;
    }

    if (!m_sampleConverter) {
        m_inputStreamDescription = desc.getInfo();
        m_outputStreamDescription = libwebrtcAudioFormat(LibWebRTCAudioFormat::sampleRate, desc.numberOfChannels());
        m_sampleConverter.reset(gst_audio_converter_new(GST_AUDIO_CONVERTER_FLAG_IN_WRITABLE, &m_inputStreamDescription,
            &m_outputStreamDescription, nullptr));
    }

    {
        Locker locker { m_adapterLock };
        const auto& sample = data.getSample();
        auto* buffer = gst_sample_get_buffer(sample.get());
        gst_adapter_push(m_adapter.get(), gst_buffer_ref(buffer));
    }
    LibWebRTCProvider::callOnWebRTCSignalingThread([protectedThis = makeRef(*this)] {
        protectedThis->pullAudioData();
    });
}

void RealtimeOutgoingAudioSourceLibWebRTC::pullAudioData()
{
    if (!GST_AUDIO_INFO_IS_VALID(&m_inputStreamDescription) || !GST_AUDIO_INFO_IS_VALID(&m_outputStreamDescription)) {
        GST_INFO("No stream description set yet.");
        return;
    }

    size_t outChunkSampleCount = LibWebRTCAudioFormat::chunkSampleCount;
    size_t outBufferSize = outChunkSampleCount * m_outputStreamDescription.bpf;

    Locker locker { m_adapterLock };
    size_t inChunkSampleCount = gst_audio_converter_get_in_frames(m_sampleConverter.get(), outChunkSampleCount);
    size_t inBufferSize = inChunkSampleCount * m_inputStreamDescription.bpf;

    while (gst_adapter_available(m_adapter.get()) > inBufferSize) {
        auto inBuffer = adoptGRef(gst_adapter_take_buffer(m_adapter.get(), inBufferSize));
        m_audioBuffer.grow(outBufferSize);
        if (isSilenced())
            webkitGstAudioFormatFillSilence(m_outputStreamDescription.finfo, m_audioBuffer.data(), outBufferSize);
        else {
            GstMappedBuffer inMap(inBuffer.get(), GST_MAP_READ);

            gpointer in[1] = { inMap.data() };
            gpointer out[1] = { m_audioBuffer.data() };
            if (!gst_audio_converter_samples(m_sampleConverter.get(), static_cast<GstAudioConverterFlags>(0), in, inChunkSampleCount, out, outChunkSampleCount)) {
                GST_ERROR("Could not convert samples.");

                return;
            }
        }

        sendAudioFrames(m_audioBuffer.data(), LibWebRTCAudioFormat::sampleSize, GST_AUDIO_INFO_RATE(&m_outputStreamDescription),
            GST_AUDIO_INFO_CHANNELS(&m_outputStreamDescription), outChunkSampleCount);
    }
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
