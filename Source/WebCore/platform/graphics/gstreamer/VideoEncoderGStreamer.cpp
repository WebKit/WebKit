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

static WorkQueue& gstWorkQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("GStreamer VideoEncoder Queue"));
    return queue.get();
}

class GStreamerInternalVideoEncoder : public ThreadSafeRefCounted<GStreamerInternalVideoEncoder> {
public:
    static Ref<GStreamerInternalVideoEncoder> create(const String& codecName, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback) { return adoptRef(*new GStreamerInternalVideoEncoder(codecName, WTFMove(outputCallback), WTFMove(postTaskCallback))); }
    ~GStreamerInternalVideoEncoder() = default;

    String initialize(const VideoEncoder::Config&);
    void postTask(Function<void()>&& task) { m_postTaskCallback(WTFMove(task)); }
    bool encode(VideoEncoder::RawFrame&&, bool shouldGenerateKeyFrame, VideoEncoder::EncodeCallback&&);
    void flush(Function<void()>&&);
    void close() { m_isClosed = true; }

    const RefPtr<GStreamerElementHarness> harness() const { return m_harness; }
    bool isClosed() const { return m_isClosed; }

private:
    GStreamerInternalVideoEncoder(const String& codecName, VideoEncoder::OutputCallback&&, VideoEncoder::PostTaskCallback&&);

    const String m_codecName;
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
        gstWorkQueue().dispatch([callback = WTFMove(callback), codecName]() mutable {
            callback(makeUnexpected(makeString("No GStreamer encoder found for codec ", codecName)));
        });
        return;
    }

    auto encoder = makeUniqueRef<GStreamerVideoEncoder>(codecName, WTFMove(outputCallback), WTFMove(postTaskCallback));
    auto error = encoder->initialize(config);
    gstWorkQueue().dispatch([callback = WTFMove(callback), descriptionCallback = WTFMove(descriptionCallback), encoder = WTFMove(encoder), error]() mutable {
        auto internalEncoder = encoder->m_internalEncoder;
        internalEncoder->postTask([callback = WTFMove(callback), descriptionCallback = WTFMove(descriptionCallback), encoder = WTFMove(encoder), error]() mutable {
            if (!error.isEmpty()) {
                GST_WARNING("Error creating encoder: %s", error.ascii().data());
                callback(makeUnexpected(makeString("GStreamer encoding initialization failed with error: ", error)));
                return;
            }

            GST_DEBUG("Encoder created");
            callback(UniqueRef<VideoEncoder> { WTFMove(encoder) });

            VideoEncoder::ActiveConfiguration configuration;
            // FIXME: How to properly fill configuration.colorspace?
            configuration.colorSpace = PlatformVideoColorSpace { PlatformVideoColorPrimaries::Bt709, PlatformVideoTransferCharacteristics::Iec6196621, PlatformVideoMatrixCoefficients::Smpte170m, false };
            descriptionCallback(WTFMove(configuration));
        });
    });
}

GStreamerVideoEncoder::GStreamerVideoEncoder(const String& codecName, OutputCallback&& outputCallback, PostTaskCallback&& postTaskCallback)
    : m_internalEncoder(GStreamerInternalVideoEncoder::create(codecName, WTFMove(outputCallback), WTFMove(postTaskCallback)))
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
    gstWorkQueue().dispatch([frame = WTFMove(frame), shouldGenerateKeyFrame, encoder = m_internalEncoder, callback = WTFMove(callback)]() mutable {
        auto result = encoder->encode(WTFMove(frame), shouldGenerateKeyFrame, WTFMove(callback));
        if (encoder->isClosed())
            return;

        String resultString;
        if (result)
            encoder->harness()->processOutputBuffers();
        else
            resultString = makeString("Encoding failed");
        callback(WTFMove(resultString));
    });
}

void GStreamerVideoEncoder::flush(Function<void()>&& callback)
{
    gstWorkQueue().dispatch([encoder = m_internalEncoder, callback = WTFMove(callback)]() mutable {
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

GStreamerInternalVideoEncoder::GStreamerInternalVideoEncoder(const String& codecName, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback)
    : m_codecName(codecName)
    , m_outputCallback(WTFMove(outputCallback))
    , m_postTaskCallback(WTFMove(postTaskCallback))
{
    GRefPtr<GstElement> element = gst_element_factory_make("webkitvideoencoder", nullptr);
    m_harness = GStreamerElementHarness::create(WTFMove(element), [protectedThis = Ref { *this }, this](auto&, const GRefPtr<GstBuffer>& outputBuffer) {
        if (protectedThis->m_isClosed)
            return;

        bool isKeyFrame = !GST_BUFFER_FLAG_IS_SET(outputBuffer.get(), GST_BUFFER_FLAG_DELTA_UNIT);
        GST_TRACE_OBJECT(m_harness->element(), "Notifying encoded%s frame", isKeyFrame ? " key" : "");
        GstMappedBuffer encodedImage(outputBuffer.get(), GST_MAP_READ);
        VideoEncoder::EncodedFrame encodedFrame {
            Vector<uint8_t> { Span<const uint8_t> { encodedImage.data(), encodedImage.size() } },
            isKeyFrame, m_timestamp, m_duration
        };

        m_postTaskCallback([protectedThis = Ref { *this }, encodedFrame = WTFMove(encodedFrame)]() mutable {
            if (protectedThis->m_isClosed)
                return;
            protectedThis->m_outputCallback({ WTFMove(encodedFrame) });
        });
    });
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
    } else
        return makeString("Unsupported outgoing video encoding: ", m_codecName);

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

bool GStreamerInternalVideoEncoder::encode(VideoEncoder::RawFrame&& rawFrame, bool shouldGenerateKeyFrame, VideoEncoder::EncodeCallback&& callback)
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

    auto sample = downcast<VideoFrameGStreamer>(rawFrame.frame.get()).sample();

    bool result = false;
    auto* buffer = gst_sample_get_buffer(sample);
    GST_BUFFER_PTS(buffer) = m_timestamp;

    // FIXME: The WebRTC encoder doesn't support GL memories ingesting yet, so until then we do a conversion here.
#if USE(GSTREAMER_GL)
    auto* memory = gst_buffer_peek_memory(buffer, 0);
    if (gst_is_gl_memory(memory)) {
        auto* inputCaps = gst_sample_get_caps(sample);
        auto outputCaps = adoptGRef(gst_caps_copy(inputCaps));
        auto* features = gst_caps_get_features(outputCaps.get(), 0);
        gst_caps_features_remove(features, GST_CAPS_FEATURE_MEMORY_GL_MEMORY);

        GstVideoInfo inputInfo, outputInfo;
        if (!gst_video_info_from_caps(&outputInfo, outputCaps.get()) || !gst_video_info_from_caps(&inputInfo, inputCaps)) {
            m_postTaskCallback([protectedThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
                if (protectedThis->m_isClosed)
                    return;

                callback("Unable to convert GL video frame"_s);
            });
            return false;
        }

        if (!m_colorConvertOutputCaps || !gst_caps_is_equal(m_colorConvertOutputCaps.get(), outputCaps.get())
            || !m_colorConvertInputCaps || !gst_caps_is_equal(m_colorConvertInputCaps.get(), inputCaps)) {
            m_colorConvert.reset(gst_video_converter_new(&inputInfo, &outputInfo, nullptr));
            m_colorConvertInputCaps = adoptGRef(gst_caps_copy(inputCaps));
            m_colorConvertOutputCaps = outputCaps;
        }

        auto outputBuffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&outputInfo), nullptr));
        {
            GstMappedFrame inputFrame(buffer, inputInfo, GST_MAP_READ);
            GstMappedFrame outputFrame(outputBuffer.get(), outputInfo, GST_MAP_WRITE);
            gst_video_converter_frame(m_colorConvert.get(), inputFrame.get(), outputFrame.get());
        }

        GST_BUFFER_PTS(outputBuffer.get()) = GST_BUFFER_PTS(buffer);
        GST_BUFFER_DTS(outputBuffer.get()) = GST_BUFFER_DTS(buffer);
        GST_BUFFER_DURATION(outputBuffer.get()) = GST_BUFFER_DURATION(buffer);
        auto convertedSample = adoptGRef(gst_sample_new(outputBuffer.get(), outputCaps.get(), nullptr, nullptr));
        result = m_harness->pushSample(WTFMove(convertedSample));
    } else
#endif
        result = m_harness->pushSample(GRefPtr<GstSample>(sample));

    return result;
}

void GStreamerInternalVideoEncoder::flush(Function<void()> && callback)
{
    m_harness->flush();
    m_postTaskCallback(WTFMove(callback));
}

}

#endif // ENABLE(WEB_CODECS) && USE(GSTREAMER)
