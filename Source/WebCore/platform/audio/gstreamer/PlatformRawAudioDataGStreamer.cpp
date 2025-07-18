/*
 * Copyright (C) 2023 Igalia S.L
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
#include "PlatformRawAudioDataGStreamer.h"

#if ENABLE(WEB_CODECS) && USE(GSTREAMER)

#include "AudioSampleFormat.h"
#include "GStreamerCommon.h"
#include "MediaSampleGStreamer.h"
#include "SharedBuffer.h"
#include "WebCodecsAudioDataAlgorithms.h"
#include <wtf/glib/GUniquePtr.h>

GST_DEBUG_CATEGORY(webkit_audio_data_debug);
#define GST_CAT_DEFAULT webkit_audio_data_debug

namespace WebCore {

static void ensureAudioDataDebugCategoryInitialized()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_audio_data_debug, "webkitaudiodata", 0, "WebKit Audio Data");
    });
}

static std::pair<GstAudioFormat, GstAudioLayout> convertAudioSampleFormatToGStreamerFormat(const AudioSampleFormat& format)
{
    switch (format) {
    case AudioSampleFormat::U8:
        return { GST_AUDIO_FORMAT_U8, GST_AUDIO_LAYOUT_INTERLEAVED };
    case AudioSampleFormat::S16:
        return { GST_AUDIO_FORMAT_S16, GST_AUDIO_LAYOUT_INTERLEAVED };
    case AudioSampleFormat::S32:
        return { GST_AUDIO_FORMAT_S32, GST_AUDIO_LAYOUT_INTERLEAVED };
    case AudioSampleFormat::F32:
        return { GST_AUDIO_FORMAT_F32, GST_AUDIO_LAYOUT_INTERLEAVED };
    case AudioSampleFormat::U8Planar:
        return { GST_AUDIO_FORMAT_U8, GST_AUDIO_LAYOUT_NON_INTERLEAVED };
    case AudioSampleFormat::S16Planar:
        return { GST_AUDIO_FORMAT_S16, GST_AUDIO_LAYOUT_NON_INTERLEAVED };
    case AudioSampleFormat::S32Planar:
        return { GST_AUDIO_FORMAT_S32, GST_AUDIO_LAYOUT_NON_INTERLEAVED };
    case AudioSampleFormat::F32Planar:
        return { GST_AUDIO_FORMAT_F32, GST_AUDIO_LAYOUT_NON_INTERLEAVED };
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { GST_AUDIO_FORMAT_UNKNOWN, GST_AUDIO_LAYOUT_INTERLEAVED };
}

Ref<PlatformRawAudioData> PlatformRawAudioData::create(Ref<MediaSample>&& sample)
{
    ASSERT(sample->platformSample().type == PlatformSample::GStreamerSampleType);
    return PlatformRawAudioDataGStreamer::create(GRefPtr { sample->platformSample().sample.gstSample });
}

RefPtr<PlatformRawAudioData> PlatformRawAudioData::create(std::span<const uint8_t> sourceData, AudioSampleFormat format, float sampleRate, int64_t timestamp, size_t numberOfFrames, size_t numberOfChannels)
{
    ensureAudioDataDebugCategoryInitialized();
    auto [gstFormat, layout] = convertAudioSampleFormatToGStreamerFormat(format);

    GstAudioInfo info;
    gst_audio_info_set_format(&info, gstFormat, static_cast<int>(sampleRate), numberOfChannels, nullptr);
    GST_AUDIO_INFO_LAYOUT(&info) = layout;

    if (!GST_AUDIO_INFO_IS_VALID(&info)) {
        GST_WARNING("Invalid audio info, unable to create AudioData for it");
        return nullptr;
    }

    auto caps = adoptGRef(gst_audio_info_to_caps(&info));
    GST_TRACE("Creating raw audio wrapper with caps %" GST_PTR_FORMAT, caps.get());
    GST_MEMDUMP("Source", sourceData.data(), sourceData.size());

    Ref data = SharedBuffer::create(Vector<uint8_t>(sourceData));
    gpointer bufferData = const_cast<void*>(static_cast<const void*>(data->span().data()));
    auto bufferLength = data->size();
    auto buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, bufferData, bufferLength, 0, bufferLength, reinterpret_cast<gpointer>(&data.leakRef()), [](gpointer data) {
        static_cast<SharedBuffer*>(data)->deref();
    }));
    GST_BUFFER_DURATION(buffer.get()) = (numberOfFrames / sampleRate) * 1000000000;

    GstSegment segment;
    gst_segment_init(&segment, GST_FORMAT_TIME);
    if (timestamp < 0)
        segment.rate = -1.0;

    GST_BUFFER_PTS(buffer.get()) = abs(timestamp) * 1000;

    gst_buffer_add_audio_meta(buffer.get(), &info, numberOfFrames, nullptr);

    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), &segment, nullptr));
    return PlatformRawAudioDataGStreamer::create(WTFMove(sample));
}

PlatformRawAudioDataGStreamer::PlatformRawAudioDataGStreamer(GRefPtr<GstSample>&& sample)
    : m_sample(WTFMove(sample))
{
    ensureAudioDataDebugCategoryInitialized();
    gst_audio_info_from_caps(&m_info, gst_sample_get_caps(m_sample.get()));
}

AudioSampleFormat PlatformRawAudioDataGStreamer::format() const
{
    auto gstFormat = GST_AUDIO_INFO_FORMAT(&m_info);
    auto layout = GST_AUDIO_INFO_LAYOUT(&m_info);
    switch (gstFormat) {
    case GST_AUDIO_FORMAT_U8:
        if (layout == GST_AUDIO_LAYOUT_INTERLEAVED)
            return AudioSampleFormat::U8;
        return AudioSampleFormat::U8Planar;
    case GST_AUDIO_FORMAT_S16:
        if (layout == GST_AUDIO_LAYOUT_INTERLEAVED)
            return AudioSampleFormat::S16;
        return AudioSampleFormat::S16Planar;
    case GST_AUDIO_FORMAT_S32:
        if (layout == GST_AUDIO_LAYOUT_INTERLEAVED)
            return AudioSampleFormat::S32;
        return AudioSampleFormat::S32Planar;
    case GST_AUDIO_FORMAT_F32:
        if (layout == GST_AUDIO_LAYOUT_INTERLEAVED)
            return AudioSampleFormat::F32;
        return AudioSampleFormat::F32Planar;
    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return AudioSampleFormat::U8;
}

size_t PlatformRawAudioDataGStreamer::sampleRate() const
{
    return GST_AUDIO_INFO_RATE(&m_info);
}

size_t PlatformRawAudioDataGStreamer::numberOfChannels() const
{
    return GST_AUDIO_INFO_CHANNELS(&m_info);
}

size_t PlatformRawAudioDataGStreamer::numberOfFrames() const
{
    return gst_buffer_get_size(gst_sample_get_buffer(m_sample.get())) / GST_AUDIO_INFO_BPF(&m_info);
}

std::optional<uint64_t> PlatformRawAudioDataGStreamer::duration() const
{
    auto buffer = gst_sample_get_buffer(m_sample.get());
    if (!GST_BUFFER_DURATION_IS_VALID(buffer))
        return { };

    return GST_TIME_AS_USECONDS(GST_BUFFER_DURATION(buffer));
}

int64_t PlatformRawAudioDataGStreamer::timestamp() const
{
    auto buffer = gst_sample_get_buffer(m_sample.get());
    auto timestamp = GST_TIME_AS_USECONDS(GST_BUFFER_PTS(buffer));
    auto segment = gst_sample_get_segment(m_sample.get());
    if (segment->rate < 0)
        return -timestamp;
    return timestamp;
}

bool PlatformRawAudioDataGStreamer::isInterleaved() const
{
    return GST_AUDIO_INFO_LAYOUT(&m_info) == GST_AUDIO_LAYOUT_INTERLEAVED;
}

size_t PlatformRawAudioDataGStreamer::memoryCost() const
{
    return gst_buffer_get_size(gst_sample_get_buffer(m_sample.get()));
}

std::optional<Variant<Vector<std::span<uint8_t>>, Vector<std::span<int16_t>>, Vector<std::span<int32_t>>, Vector<std::span<float>>>> PlatformRawAudioDataGStreamer::planesOfSamples(size_t samplesOffset)
{
    GstMappedAudioBuffer mappedBuffer(m_sample, GST_MAP_READ);
    if (!mappedBuffer)
        return std::nullopt;

    switch (format()) {
    case AudioSampleFormat::U8:
    case AudioSampleFormat::U8Planar:
        return mappedBuffer.samples<uint8_t>(samplesOffset);
    case AudioSampleFormat::S16:
    case AudioSampleFormat::S16Planar:
        return mappedBuffer.samples<int16_t>(samplesOffset);
    case AudioSampleFormat::S32:
    case AudioSampleFormat::S32Planar:
        return mappedBuffer.samples<int32_t>(samplesOffset);
    case AudioSampleFormat::F32:
    case AudioSampleFormat::F32Planar:
        return mappedBuffer.samples<float>(samplesOffset);
    }
    RELEASE_ASSERT_NOT_REACHED();
    return std::nullopt;
}

#ifndef GST_DISABLE_GST_DEBUG
static const char* layoutToString(GstAudioLayout layout)
{
    switch (layout) {
    case GST_AUDIO_LAYOUT_INTERLEAVED:
        return "interleaved";
    case GST_AUDIO_LAYOUT_NON_INTERLEAVED:
        return "planar";
    }
    return "unknown";
}
#endif

void PlatformRawAudioData::copyTo(std::span<uint8_t> destination, AudioSampleFormat format, size_t planeIndex, std::optional<size_t> frameOffset, std::optional<size_t>, unsigned long copyElementCount)
{
    // WebCodecsAudioDataAlgorithms's computeCopyElementCount ensures that all parameters are correct.
    auto& audioData = downcast<PlatformRawAudioDataGStreamer>(*this);

    auto sourceFormat = audioData.format();
    const auto& sourceSample = audioData.sample();
    bool isDestinationInterleaved = isAudioSampleFormatInterleaved(format);
    auto sourceOffset = frameOffset.value_or(0);

#ifndef GST_DISABLE_GST_DEBUG
    [[maybe_unused]] auto [gstSourceFormat, sourceLayout] = convertAudioSampleFormatToGStreamerFormat(sourceFormat);
    auto [gstDestinationFormat, destinationLayout] = convertAudioSampleFormatToGStreamerFormat(format);
    const char* destinationFormatDescription = gst_audio_format_to_string(gstDestinationFormat);
    GST_TRACE("Copying %s %s data at planeIndex %zu, destination format is %s %s, source offset: %zu", layoutToString(sourceLayout), gst_audio_format_to_string(gstSourceFormat), planeIndex, layoutToString(destinationLayout), destinationFormatDescription, sourceOffset);
#endif

    // Copy memory when:
    // - formats fully match
    // - sample format matches and source is mono (planar and interleaved have the same layout)
    if (sourceFormat == format || (audioSampleElementFormat(sourceFormat) == audioSampleElementFormat(format) && numberOfChannels() == 1)) {
        ASSERT(!isDestinationInterleaved || !planeIndex);
        GstMappedBuffer mappedBuffer(gst_sample_get_buffer(sourceSample.get()), GST_MAP_READ);
        auto source = mappedBuffer.span<uint8_t>();
        GUniquePtr<GstAudioInfo> sourceInfo(gst_audio_info_copy(audioData.info()));
        size_t sampleSize = GST_AUDIO_INFO_BPS(sourceInfo.get());
        size_t frameSize = audioData.isInterleaved() ? GST_AUDIO_INFO_BPF(sourceInfo.get()) : sampleSize;
        size_t planeOffset = planeIndex * numberOfFrames();
        size_t frameOffsetInBytes = (planeOffset + frameOffset.value_or(0)) * frameSize;
        size_t copyLengthInBytes = copyElementCount * sampleSize;
        RELEASE_ASSERT(frameOffsetInBytes + copyLengthInBytes <= source.size());
        auto subSource = source.subspan(frameOffsetInBytes, copyLengthInBytes);
        memcpySpan(destination, subSource);
        return;
    }

    auto source = audioData.planesOfSamples(sourceOffset * (audioData.isInterleaved() ? numberOfChannels() : 1));
    if (!source)
        return;

    if (isDestinationInterleaved) {
        // Copy of all channels of the source into the destination buffer and deinterleave.
        ASSERT(!planeIndex);
        copyToInterleaved(*source, destination, format, copyElementCount);
        return;
    }

    auto copyElements = []<typename T>(std::span<T> destination, const auto& sourcePlane, size_t samples)
    {
        RELEASE_ASSERT(destination.size() >= samples);
        for (size_t sample = 0; sample < samples; sample++)
            destination[sample] = convertAudioSample<T>(sourcePlane[sample]);
    };

    WTF::switchOn(audioElementSpan(format, destination), [&](auto dst) {
        switchOn(*source, [&](auto& src) {
            if (src[planeIndex].size() < copyElementCount)
                return;
            copyElements(dst, src[planeIndex], copyElementCount);
        });
    });
}

} // namespace WebCore

#undef GST_CAT_DEFAULT

#endif // ENABLE(WEB_CODECS) && USE(GSTREAMER)
