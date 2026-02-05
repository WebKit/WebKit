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
#include "VideoDecoderGStreamer.h"
#include "VideoFrameContentHint.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "GStreamerElementHarness.h"
#include "GStreamerRegistryScanner.h"
#include "PlatformDisplay.h"
#include "VideoFrameGStreamer.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/MakeString.h>

#if USE(GSTREAMER_GL)
#include <gst/gl/gl.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerVideoDecoder);

GST_DEBUG_CATEGORY(webkit_video_decoder_debug);
#define GST_CAT_DEFAULT webkit_video_decoder_debug

static WorkQueue& gstDecoderWorkQueue()
{
    static std::once_flag onceKey;
    static LazyNeverDestroyed<Ref<WorkQueue>> queue;
    std::call_once(onceKey, [] {
        queue.construct(WorkQueue::create("GStreamer VideoDecoder queue"_s));
    });
    return queue.get();
}

class GStreamerInternalVideoDecoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GStreamerInternalVideoDecoder> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GStreamerInternalVideoDecoder);

public:
    static Ref<GStreamerInternalVideoDecoder> create(const String& codecName, const VideoDecoder::Config& config, VideoDecoder::OutputCallback&& outputCallback, GRefPtr<GstElement>&& element)
    {
        return adoptRef(*new GStreamerInternalVideoDecoder(codecName, config, WTF::move(outputCallback), WTF::move(element)));
    }
    ~GStreamerInternalVideoDecoder()
    {
        if (isConfigured()) {
            GST_DEBUG_OBJECT(harnessedElement(), "Disposing video decoder");
            return;
        }
        GST_DEBUG("Disposing un-configured video decoder");
    }

    Ref<VideoDecoder::DecodePromise> decode(std::span<const uint8_t>, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration);
    void flush();
    void close() { m_isClosed = true; }

    bool isConfigured() const { return !!m_inputCaps; }

    GstElement* harnessedElement() const { return m_harness->element(); }

private:
    GStreamerInternalVideoDecoder(const String& codecName, const VideoDecoder::Config&, VideoDecoder::OutputCallback&&, GRefPtr<GstElement>&&);

    VideoDecoder::OutputCallback m_outputCallback;

    RefPtr<GStreamerElementHarness> m_harness;
    FloatSize m_presentationSize;
    int64_t m_timestamp;
    std::optional<uint64_t> m_duration;
    bool m_isClosed { false };
    GRefPtr<GstCaps> m_inputCaps;
    std::optional<VideoFrameGStreamer::Info> m_videoInfo;
};

void GStreamerVideoDecoder::create(const String& codecName, const Config& config, CreateCallback&& callback, OutputCallback&& outputCallback)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_video_decoder_debug, "webkitvideodecoder", 0, "WebKit WebCodecs Video Decoder");
    });

    bool usingHardware = config.decoding == VideoDecoder::HardwareAcceleration::Yes;
    auto& scanner = GStreamerRegistryScanner::singleton();
    auto lookupResult = scanner.isCodecSupported(GStreamerRegistryScanner::Configuration::Decoding, codecName, usingHardware);
    if (usingHardware && !lookupResult) {
        GST_DEBUG("No hardware decoder found for codec %s, falling back to software", codecName.utf8().data());
        lookupResult = scanner.isCodecSupported(GStreamerRegistryScanner::Configuration::Decoding, codecName, false);
    }

    if (!lookupResult) {
        GST_WARNING("No decoder found for codec %s", codecName.utf8().data());
        callback(makeUnexpected(makeString("No decoder found for codec "_s, codecName)));
        return;
    }

    GRefPtr<GstElement> element = gst_element_factory_create(lookupResult.factory.get(), nullptr);
    Ref decoder = adoptRef(*new GStreamerVideoDecoder(codecName, config, WTF::move(outputCallback), WTF::move(element)));
    Ref internalDecoder = decoder->m_internalDecoder;
    if (!internalDecoder->isConfigured()) {
        GST_WARNING("Internal video decoder failed to configure for codec %s", codecName.utf8().data());
        callback(makeUnexpected(makeString("Internal video decoder failed to configure for codec "_s, codecName)));
        return;
    }

    gstDecoderWorkQueue().dispatch([callback = WTF::move(callback), decoder = WTF::move(decoder)]() mutable {
        auto internalDecoder = decoder->m_internalDecoder;
        GST_DEBUG_OBJECT(decoder->m_internalDecoder->harnessedElement(), "Video decoder created");
        callback(Ref<VideoDecoder> { WTF::move(decoder) });
    });
}

GStreamerVideoDecoder::GStreamerVideoDecoder(const String& codecName, const Config& config, OutputCallback&& outputCallback, GRefPtr<GstElement>&& element)
    : m_internalDecoder(GStreamerInternalVideoDecoder::create(codecName, config, WTF::move(outputCallback), WTF::move(element)))
{
}

GStreamerVideoDecoder::~GStreamerVideoDecoder()
{
    close();
}

Ref<VideoDecoder::DecodePromise> GStreamerVideoDecoder::decode(EncodedFrame&& frame)
{
    return invokeAsync(gstDecoderWorkQueue(), [value = Vector<uint8_t> { frame.data }, isKeyFrame = frame.isKeyFrame, timestamp = frame.timestamp, duration = frame.duration, decoder = m_internalDecoder] {
        return decoder->decode(value.span(), isKeyFrame, timestamp, duration);
    });
}

Ref<GenericPromise> GStreamerVideoDecoder::flush()
{
    return invokeAsync(gstDecoderWorkQueue(), [decoder = m_internalDecoder] {
        decoder->flush();
        return GenericPromise::createAndResolve();
    });
}

void GStreamerVideoDecoder::reset()
{
    m_internalDecoder->close();
}

void GStreamerVideoDecoder::close()
{
    m_internalDecoder->close();
}

GStreamerInternalVideoDecoder::GStreamerInternalVideoDecoder(const String& codecName, const VideoDecoder::Config& config, VideoDecoder::OutputCallback&& outputCallback, GRefPtr<GstElement>&& element)
    : m_outputCallback(WTF::move(outputCallback))
{
    GST_DEBUG_OBJECT(element.get(), "Configuring decoder for codec %s", codecName.ascii().data());
    configureVideoDecoderForHarnessing(element);

#if USE(GSTREAMER_GL)
    static ASCIILiteral gstGlDisplayContextType = ASCIILiteral::fromLiteralUnsafe(GST_GL_DISPLAY_CONTEXT_TYPE);
    if (!setGstElementGLContext(element.get(), gstGlDisplayContextType))
        return;
    if (!setGstElementGLContext(element.get(), "gst.gl.app_context"_s))
        return;
#endif

    auto* factory = gst_element_get_factory(element.get());
    ASCIILiteral parser;
    if (codecName.startsWith("avc1"_s)) {
        m_inputCaps = adoptGRef(gst_caps_new_simple("video/x-h264", "stream-format", G_TYPE_STRING, "avc", "alignment", G_TYPE_STRING, "au", nullptr));
        if (auto codecData = wrapSpanData(config.description))
            gst_caps_set_simple(m_inputCaps.get(), "codec_data", GST_TYPE_BUFFER, codecData.get(), nullptr);
        if (!gst_element_factory_can_sink_all_caps(factory, m_inputCaps.get()))
            parser = "h264parse"_s;
    } else if (codecName.startsWith("av01"_s))
        m_inputCaps = adoptGRef(gst_caps_new_simple("video/x-av1", "stream-format", G_TYPE_STRING, "obu-stream", "alignment", G_TYPE_STRING, "frame", nullptr));
    else if (codecName.startsWith("vp8"_s))
        m_inputCaps = adoptGRef(gst_caps_new_empty_simple("video/x-vp8"));
    else if (codecName.startsWith("vp09"_s)) {
        m_inputCaps = adoptGRef(gst_caps_new_empty_simple("video/x-vp9"));
        parser = "vp9parse"_s;
    } else if (codecName.startsWith("hvc1"_s)) {
        m_inputCaps = adoptGRef(gst_caps_new_simple("video/x-h265", "stream-format", G_TYPE_STRING, "hvc1", "alignment", G_TYPE_STRING, "au", nullptr));
        if (auto codecData = wrapSpanData(config.description))
            gst_caps_set_simple(m_inputCaps.get(), "codec_data", GST_TYPE_BUFFER, codecData.get(), nullptr);
        if (!gst_element_factory_can_sink_all_caps(factory, m_inputCaps.get()))
            parser = "h265parse"_s;
    } else if (codecName.startsWith("hev1"_s)) {
        m_inputCaps = adoptGRef(gst_caps_new_simple("video/x-h265", "stream-format", G_TYPE_STRING, "hev1", "alignment", G_TYPE_STRING, "au", nullptr));
        if (auto codecData = wrapSpanData(config.description))
            gst_caps_set_simple(m_inputCaps.get(), "codec_data", GST_TYPE_BUFFER, codecData.get(), nullptr);
        if (!gst_element_factory_can_sink_all_caps(factory, m_inputCaps.get()))
            parser = "h265parse"_s;
    } else {
        GST_ERROR("Codec %s not wired in yet", codecName.ascii().data());
        return;
    }

    if (config.width && config.height)
        gst_caps_set_simple(m_inputCaps.get(), "width", G_TYPE_INT, config.width, "height", G_TYPE_INT, config.height, nullptr);

    GRefPtr<GstElement> harnessedElement;
    if (!parser.isEmpty()) {
        // The decoder won't accept the input caps, so put a parser in front.
        auto* parserElement = makeGStreamerElement(parser);
        if (!parserElement) {
            GST_WARNING_OBJECT(element.get(), "Required parser %s not found, decoding will fail", parser.characters());
            m_inputCaps.clear();
            return;
        }
        harnessedElement = gst_bin_new(nullptr);
        gst_bin_add_many(GST_BIN_CAST(harnessedElement.get()), parserElement, element.get(), nullptr);
        gst_element_link(parserElement, element.get());
        auto sinkPad = adoptGRef(gst_element_get_static_pad(parserElement, "sink"));
        gst_element_add_pad(harnessedElement.get(), gst_ghost_pad_new("sink", sinkPad.get()));
        auto srcPad = adoptGRef(gst_element_get_static_pad(element.get(), "src"));
        gst_element_add_pad(harnessedElement.get(), gst_ghost_pad_new("src", srcPad.get()));
    } else
        harnessedElement = WTF::move(element);

    auto allowedSinkCaps = adoptGRef(gst_caps_new_empty());
#if USE(GSTREAMER_GL)
#if USE(GBM)
    gst_caps_append(allowedSinkCaps.get(), buildDMABufCaps().leakRef());
#endif // USE(GBM)
    gst_caps_append(allowedSinkCaps.get(), gst_caps_from_string("video/x-raw(memory:GLMemory)"));
#endif // USE(GSTREAMER_GL)
    gst_caps_append(allowedSinkCaps.get(), gst_caps_from_string("video/x-raw"));

    m_harness = GStreamerElementHarness::create(WTF::move(harnessedElement), [weakThis = ThreadSafeWeakPtr { *this }, this](auto& stream, GRefPtr<GstSample>&& outputSample) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (m_isClosed)
            return;

        static std::once_flag onceFlag;
        std::call_once(onceFlag, [this] {
            m_harness->dumpGraph("video-decoder"_s);
        });

        if (m_presentationSize.isEmpty())
            m_presentationSize = getVideoResolutionFromCaps(stream.outputCaps().get()).value_or(FloatSize { 0, 0 });

        auto outputBuffer = gst_sample_get_buffer(outputSample.get());
        auto duration = m_duration.value_or(GST_BUFFER_DURATION(outputBuffer));
        auto timestamp = static_cast<int64_t>(GST_BUFFER_PTS(outputBuffer));
        if (timestamp == -1)
            timestamp = m_timestamp;

        if (!m_videoInfo)
            m_videoInfo = VideoFrameGStreamer::infoFromCaps(GRefPtr(gst_sample_get_caps(outputSample.get())));

        GST_TRACE_OBJECT(m_harness->element(), "Handling decoded frame with PTS: %" GST_TIME_FORMAT " and duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(timestamp), GST_TIME_ARGS(duration));
        VideoFrameGStreamer::CreateOptions options(IntSize(m_presentationSize), { *m_videoInfo });
        options.presentationTime = fromGstClockTime(timestamp);
        options.contentHint = VideoFrameContentHint::WebCodecs;
        auto videoFrame = VideoFrameGStreamer::create(WTF::move(outputSample), options);
        m_outputCallback(VideoDecoder::DecodedFrame { WTF::move(videoFrame), timestamp, duration });
    }, std::nullopt, WTF::move(allowedSinkCaps));

    const auto& stream = m_harness->outputStreams().first();
    const auto& pad = stream->targetPad();
    gst_pad_set_query_function(pad.get(), reinterpret_cast<GstPadQueryFunction>(+[](GstPad* pad, GstObject* parent, GstQuery* query) -> gboolean {
        if (GST_QUERY_TYPE(query) == GST_QUERY_ALLOCATION) {
            gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, nullptr);
            return true;
        }
        return gst_pad_query_default(pad, parent, query);
    }));
}

Ref<VideoDecoder::DecodePromise> GStreamerInternalVideoDecoder::decode(std::span<const uint8_t> frameData, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration)
{
    GST_DEBUG_OBJECT(m_harness->element(), "Decoding%s frame", isKeyFrame ? " key" : "");
    auto buffer = wrapSpanData(frameData);
    if (!buffer)
        return VideoDecoder::DecodePromise::createAndReject("Empty frame"_s);

    m_timestamp = timestamp;
    m_duration = duration;

    GST_BUFFER_DTS(buffer.get()) = GST_BUFFER_PTS(buffer.get()) = timestamp;
    if (duration)
        GST_BUFFER_DURATION(buffer.get()) = *duration;

    if (!isKeyFrame)
        GST_BUFFER_FLAG_SET(buffer.get(), GST_BUFFER_FLAG_DELTA_UNIT);

    // FIXME: Maybe configure segment here, could be useful for reverse playback.
    auto result = m_harness->pushSample(adoptGRef(gst_sample_new(buffer.get(), m_inputCaps.get(), nullptr, nullptr)));
    if (!result)
        return VideoDecoder::DecodePromise::createAndReject("Decode error"_s);

    m_harness->processOutputSamples();
    return VideoDecoder::DecodePromise::createAndResolve();
}

void GStreamerInternalVideoDecoder::flush()
{
    if (m_isClosed) {
        GST_DEBUG_OBJECT(m_harness->element(), "Decoder closed, nothing to flush");
        return;
    }

    m_harness->reset();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
