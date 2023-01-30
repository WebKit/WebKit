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

#if ENABLE(WEB_CODECS) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "GStreamerElementHarness.h"
#include "GStreamerRegistryScanner.h"
#include "VideoFrameGStreamer.h"
#include <wtf/WorkQueue.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_video_decoder_debug);
#define GST_CAT_DEFAULT webkit_video_decoder_debug

static WorkQueue& gstWorkQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("GStreamer VideoDecoder Queue"));
    return queue.get();
}

class GStreamerInternalVideoDecoder : public ThreadSafeRefCounted<GStreamerInternalVideoDecoder> {
public:
    static Ref<GStreamerInternalVideoDecoder> create(const String& codecName, const VideoDecoder::Config& config, VideoDecoder::OutputCallback&& outputCallback, VideoDecoder::PostTaskCallback&& postTaskCallback, GRefPtr<GstElement>&& element)
    {
        return adoptRef(*new GStreamerInternalVideoDecoder(codecName, config, WTFMove(outputCallback), WTFMove(postTaskCallback), WTFMove(element)));
    }
    ~GStreamerInternalVideoDecoder() = default;

    void postTask(Function<void()>&& task) { m_postTaskCallback(WTFMove(task)); }
    void decode(Span<const uint8_t>, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration,  VideoDecoder::DecodeCallback&&);
    void flush(Function<void()>&&);
    void close() { m_isClosed = true; }

private:
    GStreamerInternalVideoDecoder(const String& codecName, const VideoDecoder::Config&, VideoDecoder::OutputCallback&&, VideoDecoder::PostTaskCallback&&, GRefPtr<GstElement>&&);

    VideoDecoder::OutputCallback m_outputCallback;
    VideoDecoder::PostTaskCallback m_postTaskCallback;

    RefPtr<GStreamerElementHarness> m_harness;
    FloatSize m_presentationSize;
    bool m_isClosed { false };
};

bool GStreamerVideoDecoder::create(const String& codecName, const Config& config, CreateCallback&& callback, OutputCallback&& outputCallback, PostTaskCallback&& postTaskCallback)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_video_decoder_debug, "webkitvideodecoder", 0, "WebKit WebCodecs Video Decoder");
    });

    auto& scanner = GStreamerRegistryScanner::singleton();
    auto lookupResult = scanner.isCodecSupported(GStreamerRegistryScanner::Configuration::Decoding, codecName);
    if (!lookupResult) {
        GST_WARNING("No decoder found for codec %s", codecName.ascii().data());
        return false;
    }

    GRefPtr<GstElement> element = gst_element_factory_create(lookupResult.factory.get(), nullptr);
    auto decoder = makeUniqueRef<GStreamerVideoDecoder>(codecName, config, WTFMove(outputCallback), WTFMove(postTaskCallback), WTFMove(element));
    gstWorkQueue().dispatch([callback = WTFMove(callback), decoder = WTFMove(decoder)]() mutable {
        auto internalDecoder = decoder->m_internalDecoder;
        internalDecoder->postTask([callback = WTFMove(callback), decoder = WTFMove(decoder)]() mutable {
            GST_DEBUG("Video decoder created");
            callback(UniqueRef<VideoDecoder> { WTFMove(decoder) });
        });
    });

    return true;
}

GStreamerVideoDecoder::GStreamerVideoDecoder(const String& codecName, const Config& config, OutputCallback&& outputCallback, PostTaskCallback&& postTaskCallback, GRefPtr<GstElement>&& element)
    : m_internalDecoder(GStreamerInternalVideoDecoder::create(codecName, config, WTFMove(outputCallback), WTFMove(postTaskCallback), WTFMove(element)))
{
}

GStreamerVideoDecoder::~GStreamerVideoDecoder()
{
    close();
}

void GStreamerVideoDecoder::decode(EncodedFrame&& frame, DecodeCallback&& callback)
{
    gstWorkQueue().dispatch([value = Vector<uint8_t> { frame.data }, isKeyFrame = frame.isKeyFrame, timestamp = frame.timestamp, duration = frame.duration, decoder = m_internalDecoder, callback = WTFMove(callback)]() mutable {
        decoder->decode({ value.data(), value.size() }, isKeyFrame, timestamp, duration, WTFMove(callback));
    });
}

void GStreamerVideoDecoder::flush(Function<void()>&& callback)
{
    gstWorkQueue().dispatch([decoder = m_internalDecoder, callback = WTFMove(callback)]() mutable {
        decoder->flush(WTFMove(callback));
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

GStreamerInternalVideoDecoder::GStreamerInternalVideoDecoder(const String& codecName, const VideoDecoder::Config& config, VideoDecoder::OutputCallback&& outputCallback, VideoDecoder::PostTaskCallback&& postTaskCallback, GRefPtr<GstElement>&& element)
    : m_outputCallback(WTFMove(outputCallback))
    , m_postTaskCallback(WTFMove(postTaskCallback))
{
    configureVideoDecoderForHarnessing(element);

    GST_DEBUG_OBJECT(element.get(), "Configuring decoder for codec %s", codecName.ascii().data());
    GRefPtr<GstCaps> inputCaps;
    if (codecName.startsWith("avc1"_s)) {
        inputCaps = adoptGRef(gst_caps_new_simple("video/x-h264", "stream-format", G_TYPE_STRING, "avc", "alignment", G_TYPE_STRING, "au", nullptr));

        Vector<uint8_t> data { config.description };
        if (!data.isEmpty()) {
            auto bufferSize = data.size();
            auto bufferData = data.data();
            auto* codecData = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, bufferData, bufferSize, 0, bufferSize, new Vector<uint8_t>(WTFMove(data)), [](gpointer data) {
                delete static_cast<Vector<uint8_t>*>(data);
            });

            gst_caps_set_simple(inputCaps.get(), "codec_data", GST_TYPE_BUFFER, codecData, nullptr);
        }
    } else if (codecName.startsWith("av01"_s))
        inputCaps = adoptGRef(gst_caps_new_simple("video/x-av1", "stream-format", G_TYPE_STRING, "obu-stream", "alignment", G_TYPE_STRING, "frame", nullptr));
    else if (codecName.startsWith("vp8"_s))
        inputCaps = adoptGRef(gst_caps_new_empty_simple("video/x-vp8"));
    else if (codecName.startsWith("vp09"_s))
        inputCaps = adoptGRef(gst_caps_new_empty_simple("video/x-vp9"));
    else {
        WTFLogAlways("Codec %s not wired in yet", codecName.ascii().data());
        return;
    }

    gst_caps_set_simple(inputCaps.get(), "width", G_TYPE_INT, config.width, "height", G_TYPE_INT, config.height, nullptr);
    m_harness = GStreamerElementHarness::create(WTFMove(element), [protectedThis = Ref { *this }, this](auto& stream, const GRefPtr<GstBuffer>& outputBuffer) {
        if (protectedThis->m_isClosed)
            return;

        GST_TRACE_OBJECT(m_harness->element(), "Got frame with PTS: %" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(outputBuffer.get())));

        if (m_presentationSize.isEmpty())
            m_presentationSize = getVideoResolutionFromCaps(stream.outputCaps().get()).value_or(FloatSize { 0, 0 });

        m_postTaskCallback([protectedThis = Ref { *this }, this, outputBuffer = GRefPtr<GstBuffer>(outputBuffer), outputCaps = stream.outputCaps()]() mutable {
            if (protectedThis->m_isClosed)
                return;

            auto sample = adoptGRef(gst_sample_new(outputBuffer.get(), outputCaps.get(), nullptr, nullptr));
            auto videoFrame = VideoFrameGStreamer::create(WTFMove(sample), m_presentationSize, fromGstClockTime(GST_BUFFER_PTS(outputBuffer.get())));

            m_outputCallback(VideoDecoder::DecodedFrame { WTFMove(videoFrame), static_cast<int64_t>(GST_BUFFER_PTS(outputBuffer.get())), GST_BUFFER_DURATION(outputBuffer.get()) });
        });
    });
    m_harness->start(WTFMove(inputCaps));
}

void GStreamerInternalVideoDecoder::decode(Span<const uint8_t> frameData, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration, VideoDecoder::DecodeCallback&& callback)
{
    GST_DEBUG_OBJECT(m_harness->element(), "Decoding%s frame", isKeyFrame ? " key" : "");

    Vector<uint8_t> data { frameData };
    if (data.isEmpty()) {
        m_postTaskCallback([protectedThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
            if (protectedThis->m_isClosed)
                return;

            protectedThis->m_outputCallback(makeUnexpected("Empty frame"_s));
            callback({ });
        });
        return;
    }

    auto bufferSize = data.size();
    auto bufferData = data.data();
    auto buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, bufferData, bufferSize, 0, bufferSize, new Vector<uint8_t>(WTFMove(data)), [](gpointer data) {
        delete static_cast<Vector<uint8_t>*>(data);
    }));

    GST_BUFFER_DTS(buffer.get()) = GST_BUFFER_PTS(buffer.get()) = timestamp;
    if (duration)
        GST_BUFFER_DURATION(buffer.get()) = *duration;

    auto result = m_harness->pushBuffer(WTFMove(buffer));
    m_postTaskCallback([protectedThis = Ref { *this }, callback = WTFMove(callback), result]() mutable {
        if (protectedThis->m_isClosed)
            return;

        if (result)
            protectedThis->m_harness->processOutputBuffers();
        else
            protectedThis->m_outputCallback(makeUnexpected("Decode error"_s));

        callback({ });
    });
}

void GStreamerInternalVideoDecoder::flush(Function<void()>&& callback)
{
    if (m_isClosed) {
        GST_DEBUG_OBJECT(m_harness->element(), "Decoder closed, nothing to flush");
        m_postTaskCallback(WTFMove(callback));
        return;
    }

    m_harness->flush();
    m_postTaskCallback(WTFMove(callback));
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS) && USE(GSTREAMER)
