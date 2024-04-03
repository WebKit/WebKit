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
#include "VideoEncoderGStreamer.h"

#if ENABLE(WEB_CODECS) && USE(GSTREAMER)

#include "GStreamerCodecUtilities.h"
#include "GStreamerCommon.h"
#include "GStreamerElementHarness.h"
#include "GStreamerRegistryScanner.h"
#include "VideoEncoderPrivateGStreamer.h"
#include "VideoFrameGStreamer.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

#if USE(GSTREAMER_GL)
#include <gst/gl/gstglmemory.h>
#endif

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_video_encoder_debug);
#define GST_CAT_DEFAULT webkit_video_encoder_debug

static WorkQueue& gstEncoderWorkQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("GStreamer VideoEncoder Queue"));
    return queue.get();
}

class GStreamerInternalVideoEncoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GStreamerInternalVideoEncoder, WTF::DestructionThread::Main> {
    WTF_MAKE_NONCOPYABLE(GStreamerInternalVideoEncoder);
    WTF_MAKE_FAST_ALLOCATED;

public:
    static Ref<GStreamerInternalVideoEncoder> create(VideoEncoder::DescriptionCallback&& descriptionCallback, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback) { return adoptRef(*new GStreamerInternalVideoEncoder(WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postTaskCallback))); }
    ~GStreamerInternalVideoEncoder();

    String initialize(const String& codecName, const VideoEncoder::Config&);
    void postTask(Function<void()>&& task) { m_postTaskCallback(WTFMove(task)); }
    bool encode(VideoEncoder::RawFrame&&, bool shouldGenerateKeyFrame);
    void flush(Function<void()>&&);
    void close() { m_isClosed = true; }

    const RefPtr<GStreamerElementHarness> harness() const { return m_harness; }
    bool isClosed() const { return m_isClosed; }

private:
    GStreamerInternalVideoEncoder(VideoEncoder::DescriptionCallback&&, VideoEncoder::OutputCallback&&, VideoEncoder::PostTaskCallback&&);

    VideoEncoder::DescriptionCallback m_descriptionCallback;
    VideoEncoder::OutputCallback m_outputCallback;
    VideoEncoder::PostTaskCallback m_postTaskCallback;
    int64_t m_timestamp { 0 };
    std::optional<uint64_t> m_duration;
    bool m_isClosed { false };
    bool m_isInitialized { false };
    RefPtr<GStreamerElementHarness> m_harness;
    GUniquePtr<GstVideoConverter> m_colorConvert;
    GRefPtr<GstCaps> m_colorConvertInputCaps;
    GRefPtr<GstCaps> m_colorConvertOutputCaps;
};

void GStreamerVideoEncoder::create(const String& codecName, const VideoEncoder::Config& config, CreateCallback&& callback, DescriptionCallback&& descriptionCallback, OutputCallback&& outputCallback, PostTaskCallback&& postTaskCallback)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_video_encoder_debug, "webkitvideoencoder", 0, "WebKit WebCodecs Video Encoder");
    });
    registerWebKitGStreamerVideoEncoder();
    auto& scanner = GStreamerRegistryScanner::singleton();
    if (!scanner.isCodecSupported(GStreamerRegistryScanner::Configuration::Encoding, codecName)) {
        auto errorMessage = makeString("No GStreamer encoder found for codec "_s, codecName);
        postTaskCallback([callback = WTFMove(callback), errorMessage = WTFMove(errorMessage)]() mutable {
            callback(makeUnexpected(WTFMove(errorMessage)));
        });
        return;
    }

    auto encoder = makeUniqueRef<GStreamerVideoEncoder>(WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postTaskCallback));
    auto internalEncoder = encoder->m_internalEncoder;
    auto error = internalEncoder->initialize(codecName, config);
    if (!error.isEmpty()) {
        encoder->m_internalEncoder->postTask([callback = WTFMove(callback), error = WTFMove(error)]() mutable {
            GST_WARNING("Error creating encoder: %s", error.ascii().data());
            callback(makeUnexpected(makeString("GStreamer encoding initialization failed with error: "_s, error)));
        });
        return;
    }
    gstEncoderWorkQueue().dispatch([callback = WTFMove(callback), encoder = WTFMove(encoder)]() mutable {
        auto internalEncoder = encoder->m_internalEncoder;
        internalEncoder->postTask([callback = WTFMove(callback), encoder = WTFMove(encoder)]() mutable {
            GST_DEBUG("Encoder created");
            callback(UniqueRef<VideoEncoder> { WTFMove(encoder) });
        });
    });
}

GStreamerVideoEncoder::GStreamerVideoEncoder(DescriptionCallback&& descriptionCallback, OutputCallback&& outputCallback, PostTaskCallback&& postTaskCallback)
    : m_internalEncoder(GStreamerInternalVideoEncoder::create(WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postTaskCallback)))
{
}

GStreamerVideoEncoder::~GStreamerVideoEncoder()
{
    GST_DEBUG_OBJECT(m_internalEncoder->harness()->element(), "Destroying");
    close();
}

void GStreamerVideoEncoder::encode(RawFrame&& frame, bool shouldGenerateKeyFrame, EncodeCallback&& callback)
{
    gstEncoderWorkQueue().dispatch([frame = WTFMove(frame), shouldGenerateKeyFrame, encoder = m_internalEncoder, callback = WTFMove(callback)]() mutable {
        auto result = encoder->encode(WTFMove(frame), shouldGenerateKeyFrame);
        if (encoder->isClosed())
            return;

        String resultString;
        if (result)
            encoder->harness()->processOutputSamples();
        else
            resultString = "Encoding failed"_s;

        encoder->postTask([weakEncoder = ThreadSafeWeakPtr { encoder.get() }, result = WTFMove(resultString), callback = WTFMove(callback)]() mutable {
            auto encoder = weakEncoder.get();
            if (!encoder || encoder->isClosed())
                return;

            callback(WTFMove(result));
        });
    });
}

void GStreamerVideoEncoder::flush(Function<void()>&& callback)
{
    gstEncoderWorkQueue().dispatch([encoder = m_internalEncoder, callback = WTFMove(callback)]() mutable {
        encoder->flush(WTFMove(callback));
    });
}

void GStreamerVideoEncoder::reset()
{
    GST_DEBUG_OBJECT(m_internalEncoder->harness()->element(), "Resetting");
    m_internalEncoder->close();
}

void GStreamerVideoEncoder::close()
{
    GST_DEBUG_OBJECT(m_internalEncoder->harness()->element(), "Closing");
    m_internalEncoder->close();
}

GStreamerInternalVideoEncoder::GStreamerInternalVideoEncoder(VideoEncoder::DescriptionCallback&& descriptionCallback, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback)
    : m_descriptionCallback(WTFMove(descriptionCallback))
    , m_outputCallback(WTFMove(outputCallback))
    , m_postTaskCallback(WTFMove(postTaskCallback))
{
    GRefPtr<GstElement> element = gst_element_factory_make("webkitvideoencoder", nullptr);

    auto pad = adoptGRef(gst_element_get_static_pad(element.get(), "src"));
    g_signal_connect_data(pad.get(), "notify::caps", G_CALLBACK(+[](GObject* pad, GParamSpec*, gpointer userData) {
        auto weakEncoder = static_cast<ThreadSafeWeakPtr<GStreamerInternalVideoEncoder>*>(userData);
        auto encoder = weakEncoder->get();
        if (!encoder)
            return;

        GRefPtr<GstCaps> caps;
        g_object_get(pad, "caps", &caps.outPtr(), nullptr);
        if (!caps)
            return;

        encoder->postTask([weakEncoder = WTFMove(weakEncoder), caps = WTFMove(caps)] {
            auto encoder = weakEncoder->get();
            if (!encoder)
                return;

            VideoEncoder::ActiveConfiguration configuration;
            configuration.colorSpace = videoColorSpaceFromCaps(caps.get());

            auto structure = gst_caps_get_structure(caps.get(), 0);
            GstBuffer* header = nullptr;
            if (auto streamHeader = gst_structure_get_value(structure, "streamheader")) {
                RELEASE_ASSERT(GST_VALUE_HOLDS_ARRAY(streamHeader));
                auto firstValue = gst_value_array_get_value(streamHeader, 0);
                RELEASE_ASSERT(GST_VALUE_HOLDS_BUFFER(firstValue));
                header = gst_value_get_buffer(firstValue);
            } else if (auto codecData = gst_structure_get_value(structure, "codec_data")) {
                RELEASE_ASSERT(GST_VALUE_HOLDS_BUFFER(codecData));
                header = gst_value_get_buffer(codecData);
            }

            if (header) {
                GstMappedBuffer buffer(header, GST_MAP_READ);
                configuration.description = { { buffer.data(), buffer.size() } };
            }
            encoder->m_descriptionCallback(WTFMove(configuration));
        });
    }), new ThreadSafeWeakPtr { *this }, [](void* data, GClosure*) {
        delete static_cast<ThreadSafeWeakPtr<GStreamerInternalVideoEncoder>*>(data);
    }, static_cast<GConnectFlags>(0));

    m_harness = GStreamerElementHarness::create(WTFMove(element), [weakThis = ThreadSafeWeakPtr { *this }, this](auto&, GRefPtr<GstSample>&& outputSample) {
        if (!weakThis.get())
            return;
        if (m_isClosed)
            return;

        static std::once_flag onceFlag;
        std::call_once(onceFlag, [this] {
            m_harness->dumpGraph("video-encoder");
        });

        auto outputBuffer = gst_sample_get_buffer(outputSample.get());
        bool isKeyFrame = !GST_BUFFER_FLAG_IS_SET(outputBuffer, GST_BUFFER_FLAG_DELTA_UNIT);
        GST_TRACE_OBJECT(m_harness->element(), "Notifying encoded%s frame", isKeyFrame ? " key" : "");
        GstMappedBuffer encodedImage(outputBuffer, GST_MAP_READ);
        VideoEncoder::EncodedFrame encodedFrame { encodedImage.createVector(), isKeyFrame, m_timestamp, m_duration, { } };

        m_postTaskCallback([protectedThis = Ref { *this }, encodedFrame = WTFMove(encodedFrame)]() mutable {
            if (protectedThis->m_isClosed)
                return;
            protectedThis->m_outputCallback({ WTFMove(encodedFrame) });
        });
    });
}

GStreamerInternalVideoEncoder::~GStreamerInternalVideoEncoder()
{
    if (!m_harness)
        return;

    auto pad = adoptGRef(gst_element_get_static_pad(m_harness->element(), "src"));
    g_signal_handlers_disconnect_by_data(pad.get(), this);
}

String GStreamerInternalVideoEncoder::initialize(const String& codecName, const VideoEncoder::Config& config)
{
    GST_DEBUG_OBJECT(m_harness->element(), "Initializing encoder for codec %s", codecName.ascii().data());
    GRefPtr<GstCaps> encoderCaps;
    if (codecName == "vp8"_s)
        encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-vp8"));
    else if (codecName.startsWith("vp09"_s))
        encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-vp9"));
    else if (codecName.startsWith("avc1"_s)) {
        encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-h264"));
        auto [profile, level] = GStreamerCodecUtilities::parseH264ProfileAndLevel(codecName);
        if (profile)
            gst_caps_set_simple(encoderCaps.get(), "profile", G_TYPE_STRING, profile, nullptr);
        // FIXME: Set level on caps too?
        UNUSED_VARIABLE(level);
    } else if (codecName.startsWith("av01"_s)) {
        // FIXME: parse codec parameters.
        encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-av1"));
    } else if (codecName.startsWith("hvc1"_s) || codecName.startsWith("hev1"_s)) {
        encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-h265"));
        if (const char* profile = GStreamerCodecUtilities::parseHEVCProfile(codecName))
            gst_caps_set_simple(encoderCaps.get(), "profile", G_TYPE_STRING, profile, nullptr);
    } else
        return makeString("Unsupported outgoing video encoding: "_s, codecName);

    if (config.width)
        gst_caps_set_simple(encoderCaps.get(), "width", G_TYPE_INT, static_cast<int>(config.width), nullptr);
    if (config.height)
        gst_caps_set_simple(encoderCaps.get(), "height", G_TYPE_INT, static_cast<int>(config.height), nullptr);

    // FIXME: Propagate config.frameRate to caps?
    gst_caps_set_simple(encoderCaps.get(), "framerate", GST_TYPE_FRACTION, 1, 1, nullptr);

    if (!videoEncoderSetFormat(WEBKIT_VIDEO_ENCODER(m_harness->element()), WTFMove(encoderCaps), codecName))
        return "Unable to set encoder format"_s;

    if (config.bitRate > 1000)
        g_object_set(m_harness->element(), "bitrate", static_cast<uint32_t>(config.bitRate / 1000), nullptr);
    m_isInitialized = true;
    return emptyString();
}

bool GStreamerInternalVideoEncoder::encode(VideoEncoder::RawFrame&& rawFrame, bool shouldGenerateKeyFrame)
{
    if (!m_isInitialized) {
        GST_WARNING_OBJECT(m_harness->element(), "Encoder not initialized");
        return true;
    }

    m_timestamp = rawFrame.timestamp;
    m_duration = rawFrame.duration;

    if (shouldGenerateKeyFrame) {
        GST_INFO_OBJECT(m_harness->element(), "Requesting key-frame!");
        m_harness->pushEvent(gst_video_event_new_downstream_force_key_unit(GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, FALSE, 1));
    }

    auto& gstVideoFrame = downcast<VideoFrameGStreamer>(rawFrame.frame.get());

    // FIXME: The WebRTC encoder doesn't support GL memories ingesting yet, so until then we do a conversion here.
    return m_harness->pushSample(gstVideoFrame.downloadSample());
}

void GStreamerInternalVideoEncoder::flush(Function<void()> && callback)
{
    m_harness->flush();
    m_postTaskCallback(WTFMove(callback));
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_CODECS) && USE(GSTREAMER)
