/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
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

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "GStreamerVideoEncoderFactory.h"

#include "GStreamerVideoCommon.h"
#include "GStreamerVideoFrameLibWebRTC.h"
#include "LibWebRTCWebKitMacros.h"
#include "webrtc/api/video_codecs/vp9_profile.h"
#include "webrtc/common_video/h264/h264_common.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp8/libvpx_vp8_encoder.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/utility/simulcast_utility.h"
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#define GST_USE_UNSTABLE_API
#include <gst/codecparsers/gsth264parser.h>
#undef GST_USE_UNSTABLE_API

#include <gst/pbutils/encoding-profile.h>
#include <gst/video/video.h>
#include <wtf/Atomics.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/StdMap.h>
#include <wtf/text/StringConcatenateNumbers.h>

GST_DEBUG_CATEGORY(webkit_webrtcenc_debug);
#define GST_CAT_DEFAULT webkit_webrtcenc_debug

namespace WebCore {

class GStreamerEncodedImageBuffer : public webrtc::EncodedImageBufferInterface {
    WTF_MAKE_FAST_ALLOCATED;

public:
    static rtc::scoped_refptr<GStreamerEncodedImageBuffer> create(GRefPtr<GstSample>&& sample)
    {
        return rtc::make_ref_counted<GStreamerEncodedImageBuffer>(WTFMove(sample));
    }

    const uint8_t* data() const final { return m_mappedBuffer->data(); }
    uint8_t* data() final { return m_mappedBuffer->data(); }
    size_t size() const final { return m_mappedBuffer->size(); }

    const GstBuffer* getBuffer() const { return gst_sample_get_buffer(m_sample.get()); }
    std::optional<FloatSize> getVideoResolution() const { return getVideoResolutionFromCaps(gst_sample_get_caps(m_sample.get())); }

protected:
    GStreamerEncodedImageBuffer() = default;
    ~GStreamerEncodedImageBuffer() = default;
    GStreamerEncodedImageBuffer(GRefPtr<GstSample>&& sample)
        : m_sample(sample)
    {
        m_mappedBuffer = GstMappedOwnedBuffer::create(gst_sample_get_buffer(m_sample.get()));
    }

    GRefPtr<GstSample> m_sample;
    RefPtr<GstMappedOwnedBuffer> m_mappedBuffer;
};

class GStreamerVideoEncoder : public webrtc::VideoEncoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GStreamerVideoEncoder(const webrtc::SdpVideoFormat&)
        : GStreamerVideoEncoder()
    {
    }

    GStreamerVideoEncoder()
        : m_firstFramePts(GST_CLOCK_TIME_NONE)
        , m_restrictionCaps(adoptGRef(gst_caps_new_empty_simple("video/x-raw")))
    {
    }

    void SetRates(const webrtc::VideoEncoder::RateControlParameters& parameters) override
    {
        GST_INFO_OBJECT(m_pipeline.get(), "New bitrate: %d - framerate is %f",
            parameters.bitrate.get_sum_bps(), parameters.framerate_fps);

        auto caps = adoptGRef(gst_caps_copy(m_restrictionCaps.get()));

        SetRestrictionCaps(WTFMove(caps));

        if (m_encoder)
            g_object_set(m_encoder.get(), "bitrate", parameters.bitrate.get_sum_bps(), nullptr);
    }

    GstElement* pipeline()
    {
        return m_pipeline.get();
    }

    GstElement* makeElement(const gchar* factoryName)
    {
        static Atomic<uint32_t> elementId;
        auto name = makeString(Name(), "-enc-", factoryName, "-", elementId.exchangeAdd(1));
        auto* elem = makeGStreamerElement(factoryName, name.utf8().data());
        return elem;
    }

    int32_t InitEncode(const webrtc::VideoCodec* codecSettings, const webrtc::VideoEncoder::Settings&) override
    {
        g_return_val_if_fail(codecSettings, WEBRTC_VIDEO_CODEC_ERR_PARAMETER);
        g_return_val_if_fail(codecSettings->codecType == CodecType(), WEBRTC_VIDEO_CODEC_ERR_PARAMETER);

        if (webrtc::SimulcastUtility::NumberOfSimulcastStreams(*codecSettings) > 1) {
            GST_ERROR("Simulcast not supported.");
            return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
        }

        m_pipeline = makeElement("pipeline");

        connectSimpleBusMessageCallback(m_pipeline.get());
        m_encoder = createEncoder();
        ASSERT(m_encoder);

        g_object_set(m_encoder.get(), "keyframe-interval", KeyframeInterval(codecSettings), nullptr);

        m_src = makeElement("appsrc");
        g_object_set(m_src.get(), "is-live", true, "format", GST_FORMAT_TIME, nullptr);

        auto* videoconvert = makeElement("videoconvert");
        auto* videoscale = makeElement("videoscale");
        m_sink = makeElement("appsink");
        g_object_set(m_sink.get(), "sync", FALSE, nullptr);

        m_capsFilter = makeElement("capsfilter");
        if (m_restrictionCaps)
            g_object_set(m_capsFilter.get(), "caps", m_restrictionCaps.get(), nullptr);

        gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), m_src.get(), videoconvert, videoscale, m_capsFilter.get(), m_encoder.get(), m_sink.get(), nullptr);
        if (!gst_element_link_many(m_src.get(), videoconvert, videoscale, m_capsFilter.get(), m_encoder.get(), m_sink.get(), nullptr)) {
            GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_VERBOSE, "webkit-webrtc-encoder.error");
            ASSERT_NOT_REACHED();
        }

        gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_VERBOSE, "webkit-webrtc-encoder");
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) final
    {
        m_imageReadyCb = callback;

        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t Release() final
    {
        m_encodedFrame.ClearEncodedData();
        if (m_pipeline) {
            disconnectSimpleBusMessageCallback(m_pipeline.get());
            gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
            m_src = nullptr;
            m_encoder = nullptr;
            m_capsFilter = nullptr;
            m_sink = nullptr;
            m_pipeline = nullptr;
        }

        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t returnFromFlowReturn(GstFlowReturn flow)
    {
        switch (flow) {
        case GST_FLOW_OK:
            return WEBRTC_VIDEO_CODEC_OK;
        case GST_FLOW_FLUSHING:
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        default:
            return WEBRTC_VIDEO_CODEC_ERROR;
        }
    }

    VideoEncoder::EncoderInfo GetEncoderInfo() const override
    {
        VideoEncoder::EncoderInfo info;
        info.implementation_name = "GStreamer";
        info.has_trusted_rate_controller = true;
        return info;
    }

    int32_t Encode(const webrtc::VideoFrame& frame, const std::vector<webrtc::VideoFrameType>* frameTypes) final
    {
        int32_t res;

        if (!m_imageReadyCb) {
            GST_INFO_OBJECT(m_pipeline.get(), "No encoded callback set yet!");

            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        if (!m_src) {
            GST_INFO_OBJECT(m_pipeline.get(), "No source set yet!");

            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        auto sample = convertLibWebRTCVideoFrameToGStreamerSample(frame);
        auto buffer = gst_sample_get_buffer(sample.get());

        if (!GST_CLOCK_TIME_IS_VALID(m_firstFramePts)) {
            m_firstFramePts = GST_BUFFER_PTS(buffer);
            auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
            gst_pad_set_offset(pad.get(), -m_firstFramePts);
        }

        for (auto frame_type : *frameTypes) {
            if (frame_type == webrtc::VideoFrameType::kVideoFrameKey) {
                auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
                auto forceKeyUnit = gst_video_event_new_downstream_force_key_unit(GST_CLOCK_TIME_NONE,
                    GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, FALSE, 1);
                GST_INFO_OBJECT(m_pipeline.get(), "Requesting KEYFRAME!");

                if (!gst_pad_push_event(pad.get(), forceKeyUnit))
                    GST_WARNING_OBJECT(pipeline(), "Could not send ForceKeyUnit event");

                break;
            }
        }

        res = returnFromFlowReturn(gst_app_src_push_sample(GST_APP_SRC(m_src.get()), sample.get()));
        if (res != WEBRTC_VIDEO_CODEC_OK)
            return res;

        auto encodedSample = adoptGRef(gst_app_sink_try_pull_sample(GST_APP_SINK(m_sink.get()), 5 * GST_SECOND));
        if (!encodedSample) {
            GST_ERROR("Didn't get any encodedSample");
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        auto encodedData = GStreamerEncodedImageBuffer::create(WTFMove(encodedSample));
        const auto* encodedBuffer = encodedData->getBuffer();
        auto resolution = encodedData->getVideoResolution();
        m_encodedFrame.SetEncodedData(encodedData);
        if (!m_encodedFrame.size())
            return WEBRTC_VIDEO_CODEC_OK;

        ASSERT(resolution);
        m_encodedFrame._encodedWidth = resolution->width();
        m_encodedFrame._encodedHeight = resolution->height();
        m_encodedFrame._frameType = GST_BUFFER_FLAG_IS_SET(encodedBuffer, GST_BUFFER_FLAG_DELTA_UNIT) ? webrtc::VideoFrameType::kVideoFrameDelta : webrtc::VideoFrameType::kVideoFrameKey;
        m_encodedFrame.capture_time_ms_ = frame.render_time_ms();
        m_encodedFrame.SetTimestamp(frame.timestamp());

        GST_LOG_OBJECT(m_pipeline.get(), "Got buffer capture_time_ms: %" G_GINT64_FORMAT " _timestamp: %u", m_encodedFrame.capture_time_ms_, m_encodedFrame.Timestamp());

        webrtc::CodecSpecificInfo codecInfo;
        PopulateCodecSpecific(&codecInfo, encodedBuffer);
        webrtc::EncodedImageCallback::Result result = m_imageReadyCb->OnEncodedImage(m_encodedFrame, &codecInfo);
        if (result.error != webrtc::EncodedImageCallback::Result::OK)
            GST_ERROR_OBJECT(m_pipeline.get(), "Encode callback failed: %d", result.error);

        return WEBRTC_VIDEO_CODEC_OK;
    }

    GRefPtr<GstElement> createEncoder(void)
    {
        GRefPtr<GstElement> webrtcencoder = gst_element_factory_make("webrtcvideoencoder", NULL);
        g_object_set(webrtcencoder.get(), "format", adoptGRef(gst_caps_from_string(Caps())).get(), NULL);

        GRefPtr<GstElement> encoder = nullptr;
        g_object_get(webrtcencoder.get(), "encoder", &encoder.outPtr(), NULL);
        if (!encoder) {
            GST_INFO("No encoder found for %s", Caps());
            return nullptr;
        }

        return webrtcencoder;
    }

    void AddCodecIfSupported(std::vector<webrtc::SdpVideoFormat>& supportedFormats)
    {
        if (auto encoder = createEncoder()) {
            auto formats = ConfigureSupportedCodec();
            supportedFormats.insert(supportedFormats.end(), formats.begin(), formats.end());
        }
    }

    virtual const gchar* Caps()
    {
        return nullptr;
    }

    virtual std::vector<webrtc::SdpVideoFormat> ConfigureSupportedCodec()
    {
        return { webrtc::SdpVideoFormat(Name()) };
    }

    virtual webrtc::VideoCodecType CodecType() = 0;
    virtual void PopulateCodecSpecific(webrtc::CodecSpecificInfo*, const GstBuffer*) = 0;
    virtual const gchar* Name() = 0;
    virtual int KeyframeInterval(const webrtc::VideoCodec* codecSettings) = 0;

    void SetRestrictionCaps(GRefPtr<GstCaps> caps)
    {
        if (m_restrictionCaps)
            g_object_set(m_capsFilter.get(), "caps", m_restrictionCaps.get(), nullptr);

        m_restrictionCaps = caps;
    }

private:
    GRefPtr<GstElement> m_pipeline;
    GRefPtr<GstElement> m_src;
    GRefPtr<GstElement> m_encoder;
    GRefPtr<GstElement> m_capsFilter;

    webrtc::EncodedImageCallback* m_imageReadyCb;
    GstClockTime m_firstFramePts;
    GRefPtr<GstCaps> m_restrictionCaps;
    webrtc::EncodedImage m_encodedFrame;

    GRefPtr<GstElement> m_sink;
};

class GStreamerH264Encoder : public GStreamerVideoEncoder {
public:
    GStreamerH264Encoder() { }

    GStreamerH264Encoder(const webrtc::SdpVideoFormat& format)
        : packetizationMode(webrtc::H264PacketizationMode::NonInterleaved)
    {
        auto it = format.parameters.find(cricket::kH264FmtpPacketizationMode);

        if (it != format.parameters.end() && it->second == "1")
            packetizationMode = webrtc::H264PacketizationMode::NonInterleaved;
    }

    int KeyframeInterval(const webrtc::VideoCodec* codecSettings) final
    {
        return codecSettings->H264().keyFrameInterval;
    }

    std::vector<webrtc::SdpVideoFormat> ConfigureSupportedCodec() final
    {
        return supportedH264Formats();
    }

    const gchar* Caps() final { return "video/x-h264"; }
    const gchar* Name() final { return cricket::kH264CodecName; }
    webrtc::VideoCodecType CodecType() final { return webrtc::kVideoCodecH264; }

    void PopulateCodecSpecific(webrtc::CodecSpecificInfo* codecSpecificInfos, const GstBuffer*) final
    {
        codecSpecificInfos->codecType = CodecType();
        webrtc::CodecSpecificInfoH264* h264Info = &(codecSpecificInfos->codecSpecific.H264);
        h264Info->packetization_mode = packetizationMode;
    }

    webrtc::H264PacketizationMode packetizationMode;
};

std::unique_ptr<webrtc::VideoEncoder> GStreamerVideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat& format)
{
    // FIXME: vpxenc doesn't support simulcast nor SVC. vp9enc supports only profile 0. These
    // shortcomings trigger webrtc/vp9.html and webrtc/simulcast-h264.html timeouts and most likely
    // bad UX in WPE/GTK browsers. So for now we prefer to use LibWebRTC's VPx encoders.
    if (format.name == cricket::kVp9CodecName) {
        GST_INFO("Using VP9 Encoder from LibWebRTC.");
        return webrtc::VP9Encoder::Create(cricket::VideoCodec(format));
    }

    if (format.name == cricket::kVp8CodecName) {
        GST_INFO("Using VP8 Encoder from LibWebRTC.");
        return makeUniqueWithoutFastMallocCheck<webrtc::LibvpxVp8Encoder>(webrtc::LibvpxInterface::Create(), webrtc::VP8Encoder::Settings());
    }

    if (format.name == cricket::kH264CodecName) {
#if WEBKIT_LIBWEBRTC_OPENH264_ENCODER
        GST_INFO("Using OpenH264 libwebrtc encoder.");
        return webrtc::H264Encoder::Create(cricket::VideoCodec(format));
#else
        GST_INFO("Using H264 GStreamer encoder.");
        return makeUnique<GStreamerH264Encoder>(format);
#endif
    }

    return nullptr;
}

GStreamerVideoEncoderFactory::GStreamerVideoEncoderFactory(bool isSupportingVP9Profile0, bool isSupportingVP9Profile2)
    : m_isSupportingVP9Profile0(isSupportingVP9Profile0)
    , m_isSupportingVP9Profile2(isSupportingVP9Profile2)
{
    ensureGStreamerInitialized();

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtcenc_debug, "webkitlibwebrtcvideoencoder", 0, "WebKit WebRTC video encoder");
    });
}

std::vector<webrtc::SdpVideoFormat> GStreamerVideoEncoderFactory::GetSupportedFormats() const
{
    std::vector<webrtc::SdpVideoFormat> supportedCodecs;

    supportedCodecs.push_back(webrtc::SdpVideoFormat(cricket::kVp8CodecName));
    if (m_isSupportingVP9Profile0)
        supportedCodecs.push_back(webrtc::SdpVideoFormat(cricket::kVp9CodecName, {{"profile-id", "0"}}));
    if (m_isSupportingVP9Profile2)
        supportedCodecs.push_back(webrtc::SdpVideoFormat(cricket::kVp9CodecName, {{"profile-id", "2"}}));

    // If OpenH264 is present, prefer it over the GStreamer encoders (x264enc, usually).
#if WEBKIT_LIBWEBRTC_OPENH264_ENCODER
    auto formats = supportedH264Formats();
    supportedCodecs.insert(supportedCodecs.end(), formats.begin(), formats.end());
#else
    GStreamerH264Encoder().AddCodecIfSupported(supportedCodecs);
#endif

    return supportedCodecs;
}

} // namespace WebCore
#endif
