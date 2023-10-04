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

class GStreamerInternalVideoEncoder : public ThreadSafeRefCounted<GStreamerInternalVideoEncoder>
    , public CanMakeWeakPtr<GStreamerInternalVideoEncoder, WeakPtrFactoryInitialization::Eager> {
    WTF_MAKE_FAST_ALLOCATED;

public:
    static Ref<GStreamerInternalVideoEncoder> create(const String& codecName, VideoEncoder::DescriptionCallback&& descriptionCallback, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback) { return adoptRef(*new GStreamerInternalVideoEncoder(codecName, WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postTaskCallback))); }
    ~GStreamerInternalVideoEncoder();

    String initialize(const VideoEncoder::Config&);
    void postTask(Function<void()>&& task) { m_postTaskCallback(WTFMove(task)); }
    bool encode(VideoEncoder::RawFrame&&, bool shouldGenerateKeyFrame);
    void flush(Function<void()>&&);
    void close() { m_isClosed = true; }

    const RefPtr<GStreamerElementHarness> harness() const { return m_harness; }
    bool isClosed() const { return m_isClosed; }

private:
    GStreamerInternalVideoEncoder(const String& codecName, VideoEncoder::DescriptionCallback&&, VideoEncoder::OutputCallback&&, VideoEncoder::PostTaskCallback&&);

    const String m_codecName;
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
        gstEncoderWorkQueue().dispatch([callback = WTFMove(callback), codecName]() mutable {
            callback(makeUnexpected(makeString("No GStreamer encoder found for codec "_s, codecName)));
        });
        return;
    }

    auto encoder = makeUniqueRef<GStreamerVideoEncoder>(codecName, WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postTaskCallback));
    auto error = encoder->initialize(config);
    gstEncoderWorkQueue().dispatch([callback = WTFMove(callback), encoder = WTFMove(encoder), error]() mutable {
        auto internalEncoder = encoder->m_internalEncoder;
        internalEncoder->postTask([callback = WTFMove(callback), encoder = WTFMove(encoder), error]() mutable {
            if (!error.isEmpty()) {
                GST_WARNING("Error creating encoder: %s", error.ascii().data());
                callback(makeUnexpected(makeString("GStreamer encoding initialization failed with error: "_s, error)));
                return;
            }

            GST_DEBUG("Encoder created");
            callback(UniqueRef<VideoEncoder> { WTFMove(encoder) });
        });
    });
}

GStreamerVideoEncoder::GStreamerVideoEncoder(const String& codecName, DescriptionCallback&& descriptionCallback, OutputCallback&& outputCallback, PostTaskCallback&& postTaskCallback)
    : m_internalEncoder(GStreamerInternalVideoEncoder::create(codecName, WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postTaskCallback)))
{
}

GStreamerVideoEncoder::~GStreamerVideoEncoder()
{
    GST_DEBUG_OBJECT(m_internalEncoder->harness()->element(), "Destroying");
    close();
}

String GStreamerVideoEncoder::initialize(const VideoEncoder::Config& config)
{
    return m_internalEncoder->initialize(config);
}

void GStreamerVideoEncoder::encode(RawFrame&& frame, bool shouldGenerateKeyFrame, EncodeCallback&& callback)
{
    gstEncoderWorkQueue().dispatch([frame = WTFMove(frame), shouldGenerateKeyFrame, encoder = m_internalEncoder, callback = WTFMove(callback)]() mutable {
        auto result = encoder->encode(WTFMove(frame), shouldGenerateKeyFrame);
        if (encoder->isClosed())
            return;

        String resultString;
        if (result)
            encoder->harness()->processOutputBuffers();
        else
            resultString = "Encoding failed"_s;

        encoder->postTask([weakEncoder = WeakPtr { encoder.get() }, result = WTFMove(resultString), callback = WTFMove(callback)]() mutable {
            if (!weakEncoder || weakEncoder->isClosed())
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

GStreamerInternalVideoEncoder::GStreamerInternalVideoEncoder(const String& codecName, VideoEncoder::DescriptionCallback&& descriptionCallback, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback)
    : m_codecName(codecName)
    , m_descriptionCallback(WTFMove(descriptionCallback))
    , m_outputCallback(WTFMove(outputCallback))
    , m_postTaskCallback(WTFMove(postTaskCallback))
{
    GRefPtr<GstElement> element = gst_element_factory_make("webkitvideoencoder", nullptr);

    auto pad = adoptGRef(gst_element_get_static_pad(element.get(), "src"));
    g_signal_connect_data(pad.get(), "notify::caps", G_CALLBACK(+[](GObject* pad, GParamSpec*, gpointer userData) {
        WeakPtr<GStreamerInternalVideoEncoder> encoder(*static_cast<WeakPtr<GStreamerInternalVideoEncoder>*>(userData));
        if (!encoder)
            return;

        GRefPtr<GstCaps> caps;
        g_object_get(pad, "caps", &caps.outPtr(), nullptr);
        encoder.get()->postTask([weakEncoder = WeakPtr { *encoder.get() }, caps = WTFMove(caps)] {
            if (!weakEncoder)
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
            weakEncoder.get()->m_descriptionCallback(WTFMove(configuration));
        });
    }), new WeakPtr { *this }, [](void* data, GClosure*) {
        delete static_cast<WeakPtr<GStreamerInternalVideoEncoder>*>(data);
    }, static_cast<GConnectFlags>(0));

    m_harness = GStreamerElementHarness::create(WTFMove(element), [weakThis = WeakPtr { *this }, this](auto&, const GRefPtr<GstBuffer>& outputBuffer) {
        if (!weakThis)
            return;
        if (m_isClosed)
            return;

        static std::once_flag onceFlag;
        std::call_once(onceFlag, [this] {
            m_harness->dumpGraph("video-encoder");
        });

        bool isKeyFrame = !GST_BUFFER_FLAG_IS_SET(outputBuffer.get(), GST_BUFFER_FLAG_DELTA_UNIT);
        GST_TRACE_OBJECT(m_harness->element(), "Notifying encoded%s frame", isKeyFrame ? " key" : "");
        GstMappedBuffer encodedImage(outputBuffer.get(), GST_MAP_READ);
        VideoEncoder::EncodedFrame encodedFrame {
            Vector<uint8_t> { std::span<const uint8_t> { encodedImage.data(), encodedImage.size() } },
            isKeyFrame, m_timestamp, m_duration, { }
        };

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

String GStreamerInternalVideoEncoder::initialize(const VideoEncoder::Config& config)
{
    GST_DEBUG_OBJECT(m_harness->element(), "Initializing encoder for codec %s", m_codecName.ascii().data());
    GRefPtr<GstCaps> encoderCaps;
    if (m_codecName == "vp8"_s)
        encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-vp8"));
    else if (m_codecName.startsWith("vp09"_s)) {
        encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-vp9"));
        if (auto profileId = GStreamerCodecUtilities::parseVP9Profile(m_codecName)) {
            auto profile = makeString(profileId);
            gst_caps_set_simple(encoderCaps.get(), "profile", G_TYPE_STRING, profile.ascii().data(), nullptr);
        }
    } else if (m_codecName.startsWith("avc1"_s)) {
        encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-h264"));
        auto [profile, level] = GStreamerCodecUtilities::parseH264ProfileAndLevel(m_codecName);
        if (profile)
            gst_caps_set_simple(encoderCaps.get(), "profile", G_TYPE_STRING, profile, nullptr);
        // FIXME: Set level on caps too?
        UNUSED_VARIABLE(level);
    } else if (m_codecName.startsWith("av01"_s)) {
        // FIXME: parse codec parameters.
        encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-av1"));
    } else if (m_codecName.startsWith("hvc1"_s) || m_codecName.startsWith("hev1"_s)) {
        encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-h265"));
        if (const char* profile = GStreamerCodecUtilities::parseHEVCProfile(m_codecName))
            gst_caps_set_simple(encoderCaps.get(), "profile", G_TYPE_STRING, profile, nullptr);
    } else
        return makeString("Unsupported outgoing video encoding: "_s, m_codecName);

    if (config.width)
        gst_caps_set_simple(encoderCaps.get(), "width", G_TYPE_INT, static_cast<int>(config.width), nullptr);
    if (config.height)
        gst_caps_set_simple(encoderCaps.get(), "height", G_TYPE_INT, static_cast<int>(config.height), nullptr);

    // FIXME: Propagate config.frameRate to caps?

    if (!videoEncoderSetFormat(WEBKIT_VIDEO_ENCODER(m_harness->element()), WTFMove(encoderCaps)))
        return "Unable to set encoder format"_s;

    if (config.bitRate)
        g_object_set(m_harness->element(), "bitrate", static_cast<uint32_t>(config.bitRate / 1024), nullptr);
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
