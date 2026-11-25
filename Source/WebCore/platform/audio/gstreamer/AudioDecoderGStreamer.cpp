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
#include "AudioDecoderGStreamer.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "GStreamerElementHarness.h"
#include "GStreamerRegistryScanner.h"
#include "PlatformRawAudioDataGStreamer.h"
#include "SharedBuffer.h"
#include <gst/pbutils/pbutils.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerAudioDecoder);

GST_DEBUG_CATEGORY(webkit_audio_decoder_debug);
#define GST_CAT_DEFAULT webkit_audio_decoder_debug

#if GST_CHECK_VERSION(1, 22, 0)
#define DEC_DEBUG(...) GST_DEBUG_ID(m_id.data(), __VA_ARGS__)
#define DEC_WARNING(...) GST_WARNING_ID(m_id.data(), __VA_ARGS__)
#else
#define DEC_DEBUG(...) GST_DEBUG(__VA_ARGS__)
#define DEC_WARNING(...) GST_WARNING(__VA_ARGS__)
#endif

static WorkQueue& gstDecoderWorkQueue()
{
    static std::once_flag onceKey;
    static LazyNeverDestroyed<Ref<WorkQueue>> queue;
    std::call_once(onceKey, [] {
        queue.construct(WorkQueue::create("GStreamer AudioDecoder queue"_s));
    });
    return queue.get();
}

class GStreamerInternalAudioDecoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GStreamerInternalAudioDecoder> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GStreamerInternalAudioDecoder);

public:
    static Ref<GStreamerInternalAudioDecoder> create(const String& codecName, const AudioDecoder::Config& config, AudioDecoder::OutputCallback&& outputCallback, GRefPtr<GstElement>&& element)
    {
        return adoptRef(*new GStreamerInternalAudioDecoder(codecName, config, WTF::move(outputCallback), WTF::move(element)));
    }
    ~GStreamerInternalAudioDecoder() { DEC_DEBUG("Disposing"); }

    Ref<AudioDecoder::DecodePromise> decode(AudioEncodedData&&);
    void flush();
    void close() { m_isClosed = true; }
    bool isConfigured() const { return !!m_inputCaps; }

private:
    GStreamerInternalAudioDecoder(const String& codecName, const AudioDecoder::Config&, AudioDecoder::OutputCallback&&, GRefPtr<GstElement>&&);

    AudioDecoder::OutputCallback m_outputCallback;

    const RefPtr<GStreamerElementHarness> m_harness;
    GRefPtr<GstCaps> m_inputCaps;
    GRefPtr<GstBuffer> m_header;
    bool m_isClosed { false };
    ASCIICString m_id;
};

void GStreamerAudioDecoder::create(const String& codecName, const Config& config, CreateCallback&& callback, OutputCallback&& outputCallback)
{
    if (!ensureGStreamerInitialized()) [[unlikely]] {
        callback(makeUnexpected("GStreamer initialization failed"_s));
        return;
    }

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_audio_decoder_debug, "webkitaudiodecoder", 0, "WebKit WebCodecs Audio Decoder");
    });

    auto& scanner = GStreamerRegistryScanner::singleton();
    auto lookupResult = scanner.isCodecSupported(GStreamerRegistryScanner::Configuration::Decoding, codecName);
    if (!lookupResult) {
        GST_WARNING("No decoder found for codec %s", codecName.utf8());
        callback(makeUnexpected(makeString("No decoder found for codec "_s, codecName)));
        return;
    }
    GRefPtr<GstElement> element = gst_element_factory_create(lookupResult.factory.get(), nullptr);

    Ref decoder = adoptRef(*new GStreamerAudioDecoder(codecName, config, WTF::move(outputCallback), WTF::move(element)));
    Ref internalDecoder = decoder->m_internalDecoder;
    if (!internalDecoder->isConfigured()) {
        callback(makeUnexpected(makeString("Internal audio decoder failed to configure for codec "_s, codecName)));
        return;
    }

    gstDecoderWorkQueue().dispatch([callback = WTF::move(callback), decoder = WTF::move(decoder)]() mutable {
        callback(Ref<AudioDecoder> { WTF::move(decoder) });
    });
}

GStreamerAudioDecoder::GStreamerAudioDecoder(const String& codecName, const Config& config, OutputCallback&& outputCallback, GRefPtr<GstElement>&& element)
    : m_internalDecoder(GStreamerInternalAudioDecoder::create(codecName, config, WTF::move(outputCallback), WTF::move(element)))
{
}

GStreamerAudioDecoder::~GStreamerAudioDecoder()
{
    close();
}

Ref<AudioDecoder::DecodePromise> GStreamerAudioDecoder::decode(AudioEncodedData&& data)
{
    return invokeAsync(gstDecoderWorkQueue(), [data = WTF::move(data).isolatedCopy(), decoder = m_internalDecoder] mutable {
        return decoder->decode(WTF::move(data));
    });
}

Ref<GenericPromise> GStreamerAudioDecoder::flush()
{
    return invokeAsync(gstDecoderWorkQueue(), [decoder = m_internalDecoder] {
        decoder->flush();
        return GenericPromise::createAndResolve();
    });
}

void GStreamerAudioDecoder::reset()
{
    m_internalDecoder->close();
}

void GStreamerAudioDecoder::close()
{
    m_internalDecoder->close();
}

GStreamerInternalAudioDecoder::GStreamerInternalAudioDecoder(const String& codecName, const AudioDecoder::Config& config, AudioDecoder::OutputCallback&& outputCallback, GRefPtr<GstElement>&& element)
    : m_outputCallback(WTF::move(outputCallback))
{
    static Atomic<uint64_t> uniqueId = 0;
    m_id = makeString("webkit-audio-dec-"_s, uniqueId.exchangeAdd(1)).ascii();

    DEC_DEBUG("Configuring decoder %" GST_PTR_FORMAT " for codec %s", element.get(), codecName.ascii().data());

    ASCIILiteral parser;
    if (codecName.startsWith("mp4a"_s)) {
        m_inputCaps = adoptGRef(gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, config.numberOfChannels, nullptr));
        auto codecData = wrapSpanData(config.description);
        if (codecData)
            gst_caps_set_simple(m_inputCaps.get(), "codec_data", GST_TYPE_BUFFER, codecData.get(), "stream-format", G_TYPE_STRING, "raw", nullptr);
        else
            gst_caps_set_simple(m_inputCaps.get(), "stream-format", G_TYPE_STRING, "adts", nullptr);
    } else if (codecName == "mp3"_s) {
        m_inputCaps = adoptGRef(gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 1, "layer", G_TYPE_INT, 3, "rate", G_TYPE_INT, config.sampleRate, "channels", G_TYPE_INT, config.numberOfChannels, "parsed", G_TYPE_BOOLEAN, TRUE, nullptr));
    } else if (codecName == "opus"_s) {
        m_header = wrapSpanData(config.description);
        if (m_header)
            m_inputCaps = adoptGRef(gst_codec_utils_opus_create_caps_from_header(m_header.get(), nullptr));
        else
            m_inputCaps = adoptGRef(gst_caps_new_empty_simple("audio/x-opus"));
    } else if (codecName == "alaw"_s)
        m_inputCaps = adoptGRef(gst_caps_new_simple("audio/x-alaw", "rate", G_TYPE_INT, config.sampleRate, "channels", G_TYPE_INT, config.numberOfChannels, nullptr));
    else if (codecName == "ulaw"_s)
        m_inputCaps = adoptGRef(gst_caps_new_simple("audio/x-mulaw", "rate", G_TYPE_INT, config.sampleRate, "channels", G_TYPE_INT, config.numberOfChannels, nullptr));
    else if (codecName == "flac"_s) {
        m_header = wrapSpanData(config.description);
        if (!m_header) {
            DEC_WARNING("Decoder config description for flac codec is mandatory");
            return;
        }
        parser = "flacparse"_s;
        m_inputCaps = adoptGRef(gst_caps_new_simple("audio/x-flac", "channels", G_TYPE_INT, config.numberOfChannels, nullptr));
    } else if (codecName == "vorbis"_s) {
        m_header = wrapSpanData(config.description);
        if (!m_header) {
            DEC_WARNING("Decoder config description for vorbis codec is mandatory");
            return;
        }
        parser = "oggparse"_s;
        m_inputCaps = adoptGRef(gst_caps_new_empty_simple("application/ogg"));
    } else if (codecName.startsWith("pcm-"_s)) {
        auto components = codecName.split('-');
        auto& pcmFormat = components[1];
        GstAudioFormat gstPcmFormat = GST_AUDIO_FORMAT_UNKNOWN;
        if (equalLettersIgnoringASCIICase(pcmFormat, "u8"_s))
            gstPcmFormat = GST_AUDIO_FORMAT_U8;
        else if (equalLettersIgnoringASCIICase(pcmFormat, "s16"_s))
            gstPcmFormat = GST_AUDIO_FORMAT_S16;
        else if (equalLettersIgnoringASCIICase(pcmFormat, "s24"_s))
            gstPcmFormat = GST_AUDIO_FORMAT_S24;
        else if (equalLettersIgnoringASCIICase(pcmFormat, "s32"_s))
            gstPcmFormat = GST_AUDIO_FORMAT_S32;
        else if (equalLettersIgnoringASCIICase(pcmFormat, "f32"_s))
            gstPcmFormat = GST_AUDIO_FORMAT_F32;
        else {
            DEC_WARNING("Invalid LPCM codec format: %s", pcmFormat.ascii().data());
            return;
        }
        m_inputCaps = adoptGRef(gst_caps_new_simple("audio/x-raw", "format", G_TYPE_STRING, gst_audio_format_to_string(gstPcmFormat),
            "rate", G_TYPE_INT, config.sampleRate, "channels", G_TYPE_INT, config.numberOfChannels,
            "layout", G_TYPE_STRING, "interleaved", nullptr));
    } else {
        DEC_WARNING("Unsupported codec");
        return;
    }

    configureAudioDecoderForHarnessing(element);

    auto factory = gst_element_get_factory(element.get());
    bool isParserRequired = !gst_element_factory_can_sink_all_caps(factory, m_inputCaps.get());

    static Atomic<uint64_t> counter = 0;
    auto binName = makeString("audio-decoder-"_s, unsafeSpan(GST_OBJECT_NAME(element.get())), '-', counter.exchangeAdd(1));

    GRefPtr<GstElement> harnessedElement = gst_bin_new(binName.ascii().data());
    auto audioconvert = gst_element_factory_make("audioconvert", nullptr);
    auto outputCapsFilter = gst_element_factory_make("capsfilter", nullptr);
    GRefPtr outputCaps = adoptGRef(gst_caps_new_simple("audio/x-raw", "format", G_TYPE_STRING, "F32LE", nullptr));
    g_object_set(outputCapsFilter, "caps", outputCaps.get(), nullptr);
    gst_bin_add_many(GST_BIN_CAST(harnessedElement.get()), audioconvert, outputCapsFilter, element.get(), nullptr);

    GRefPtr<GstElement> head = element;
    if (!parser.isEmpty() && isParserRequired) {
        // The decoder won't accept the input caps, so put a parser in front.
        auto* parserElement = makeGStreamerElement(parser);
        if (!parserElement) {
            DEC_WARNING("Required parser %s not found, decoding will fail", parser.characters());
            m_inputCaps.clear();
            return;
        }

        gst_bin_add(GST_BIN_CAST(harnessedElement.get()), parserElement);
        if (!gst_element_link(parserElement, element.get())) {
            DEC_WARNING("Unable to link parser to decoder");
            m_inputCaps.clear();
            return;
        }
        head = parserElement;
    }

    if (!gst_element_link_many(element.get(), audioconvert, outputCapsFilter, nullptr)) {
        DEC_WARNING("Unable to link %" GST_PTR_FORMAT " to audioconvert", head.get());
        m_inputCaps.clear();
        return;
    }

    GRefPtr pad = adoptGRef(gst_element_get_static_pad(head.get(), "sink"));
    gst_element_add_pad(harnessedElement.get(), gst_ghost_pad_new("sink", pad.get()));

    pad = adoptGRef(gst_element_get_static_pad(outputCapsFilter, "src"));
    gst_element_add_pad(harnessedElement.get(), gst_ghost_pad_new("src", pad.get()));

    lazyInitialize(m_harness, GStreamerElementHarness::create(WTF::move(harnessedElement), [weakThis = ThreadSafeWeakPtr { *this }, this](auto&, GRefPtr<GstSample>&& outputSample) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (m_isClosed)
            return;

        auto outputBuffer = gst_sample_get_buffer(outputSample.get());
        if (GST_BUFFER_FLAG_IS_SET(outputBuffer, GST_BUFFER_FLAG_DECODE_ONLY))
            return;

        if (!gst_buffer_n_memory(outputBuffer))
            return;

        static std::once_flag onceFlag;
        std::call_once(onceFlag, [this] {
            m_harness->dumpGraph("audio-decoder"_s);
        });

        GST_TRACE_OBJECT(m_harness->element(), "Got frame with PTS: %" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(outputBuffer)));

        auto data = PlatformRawAudioDataGStreamer::create(WTF::move(outputSample));
        m_outputCallback(AudioDecoder::DecodedData { WTF::move(data) });
    }));
}

Ref<AudioDecoder::DecodePromise> GStreamerInternalAudioDecoder::decode(AudioEncodedData&& data)
{
    DEC_DEBUG("Decoding%s frame with size %zu bytes", data.isKeyFrame ? " key" : "", data.data->size());

    auto encodedData = wrapSharedBuffer(WTF::move(data.data));
    if (!encodedData)
        return AudioDecoder::DecodePromise::createAndResolve();

    GstSegment segment;
    gst_segment_init(&segment, GST_FORMAT_TIME);
    if (data.timestamp < 0)
        segment.rate = -1.0;

    if (m_header) {
        DEC_DEBUG("Pushing initial header");
        m_harness->start(GRefPtr<GstCaps>(m_inputCaps), &segment);
        m_harness->pushBuffer(WTF::move(m_header));
    }

    GST_BUFFER_PTS(encodedData.get()) = abs(data.timestamp) * 1000;
    if (data.duration)
        GST_BUFFER_DURATION(encodedData.get()) = *data.duration;

    auto result = m_harness->pushSample(adoptGRef(gst_sample_new(encodedData.get(), m_inputCaps.get(), &segment, nullptr)));
    if (!result)
        return AudioDecoder::DecodePromise::createAndReject("Decode error"_s);

    m_harness->processOutputSamples();
    return AudioDecoder::DecodePromise::createAndResolve();
}

void GStreamerInternalAudioDecoder::flush()
{
    if (m_isClosed) {
        DEC_DEBUG("Decoder closed, nothing to flush");
        return;
    }

    if (!m_harness->isStarted()) {
        DEC_DEBUG("Decoder hasn't started yet, nothing to flush");
        return;
    }

    DEC_DEBUG("Pushing an empty buffer with discont flag");
    GRefPtr buffer = adoptGRef(gst_buffer_new());
    GST_BUFFER_FLAG_SET(buffer.get(), GST_BUFFER_FLAG_DISCONT);
    m_harness->pushBuffer(WTF::move(buffer));

    m_harness->flushBuffers();
}

#undef DEC_DEBUG
#undef DEC_WARNING

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
