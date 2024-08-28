/*
 * Copyright (C) 2018,2020 Metrological Group B.V.
 * Copyright (C) 2018,2020 Igalia S.L. All rights reserved.
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
#include "GStreamerVideoDecoderFactory.h"

#include "GStreamerQuirks.h"
#include "GStreamerVideoCommon.h"
#include "GStreamerVideoFrameLibWebRTC.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp8/libvpx_vp8_decoder.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>
#include <mutex>
#include <wtf/Lock.h>
#include <wtf/StdMap.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/text/WTFString.h>

GST_DEBUG_CATEGORY(webkit_webrtcdec_debug);
#define GST_CAT_DEFAULT webkit_webrtcdec_debug

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerVideoDecoderFactory);

class GStreamerWebRTCVideoDecoder : public webrtc::VideoDecoder {
public:
    GStreamerWebRTCVideoDecoder()
        : m_width(0)
        , m_height(0)
        , m_requireParse(false)
        , m_needsKeyframe(true)
    {
        m_rtpTimestampCaps = adoptGRef(gst_caps_new_empty_simple("timestamp/x-rtp"));
    }

    static void decodebinPadAddedCb(GstElement*, GstPad* srcpad, GstPad* sinkpad)
    {
        GST_INFO_OBJECT(srcpad, "connecting pad with %" GST_PTR_FORMAT, sinkpad);
        if (gst_pad_link(srcpad, sinkpad) != GST_PAD_LINK_OK)
            ASSERT_NOT_REACHED();
    }

    GstElement* pipeline()
    {
        return m_pipeline.get();
    }

    GstElement* makeElement(const gchar* factoryName)
    {
        GUniquePtr<char> name(g_strdup_printf("%s_dec_%s_%p", Name(), factoryName, this));

        return makeGStreamerElement(factoryName, name.get());
    }

    void handleError(GError* error)
    {
        if (!g_error_matches(error, GST_STREAM_ERROR, GST_STREAM_ERROR_DECODE))
            return;
        GST_INFO_OBJECT(pipeline(), "--> needs keyframe (%s)", error->message);
        m_needsKeyframe = true;
    }

    bool Configure(const webrtc::VideoDecoder::Settings& codecSettings) override
    {
        m_src = makeElement("appsrc");
        g_object_set(m_src, "is-live", TRUE, "do-timestamp", TRUE, "max-buffers", 2, "max-bytes", 0, nullptr);

        GRefPtr<GstCaps> caps = nullptr;
        auto capsfilter = CreateFilter();
        auto decoder = makeElement("decodebin");

        updateCapsFromImageSize(codecSettings.max_render_resolution().Width(), codecSettings.max_render_resolution().Height());

        m_pipeline = makeElement("pipeline");
        connectSimpleBusMessageCallback(m_pipeline.get());

        auto sinkpad = adoptGRef(gst_element_get_static_pad(capsfilter, "sink"));
        g_signal_connect(decoder, "pad-added", G_CALLBACK(decodebinPadAddedCb), sinkpad.get());

        auto& quirksManager = GStreamerQuirksManager::singleton();
        if (quirksManager.isEnabled()) {
            // Prevent auto-plugging of hardware-accelerated elements. Those will be used in the playback pipeline.
            g_signal_connect(decoder, "autoplug-select", G_CALLBACK(+[](GstElement*, GstPad*, GstCaps*, GstElementFactory* factory, gpointer) -> unsigned {
                static auto skipAutoPlug = gstGetAutoplugSelectResult("skip"_s);
                static auto tryAutoPlug = gstGetAutoplugSelectResult("try"_s);
                RELEASE_ASSERT(skipAutoPlug);
                RELEASE_ASSERT(tryAutoPlug);
                auto& quirksManager = GStreamerQuirksManager::singleton();
                auto isHardwareAccelerated = quirksManager.isHardwareAccelerated(factory).value_or(false);
                if (isHardwareAccelerated)
                    return *skipAutoPlug;
                return *tryAutoPlug;
            }), nullptr);
        }

        // Make the decoder output "parsed" frames only and let the main decodebin
        // do the real decoding. This allows us to have optimized decoding/rendering
        // happening in the main pipeline.
        if (m_requireParse) {
            caps = gst_caps_new_simple(Caps(), "parsed", G_TYPE_BOOLEAN, TRUE, nullptr);

            auto bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
            gst_bus_enable_sync_message_emission(bus.get());
            g_signal_connect_swapped(bus.get(), "sync-message::warning", G_CALLBACK(+[](GStreamerWebRTCVideoDecoder* decoder, GstMessage* message) {
                GUniqueOutPtr<GError> error;
                gst_message_parse_warning(message, &error.outPtr(), nullptr);
                decoder->handleError(error.get());
            }), this);
            g_signal_connect_swapped(bus.get(), "sync-message::error", G_CALLBACK(+[](GStreamerWebRTCVideoDecoder* decoder, GstMessage* message) {
                GUniqueOutPtr<GError> error;
                gst_message_parse_error(message, &error.outPtr(), nullptr);
                decoder->handleError(error.get());
            }), this);
        } else {
            /* FIXME - How could we handle missing keyframes case we do not plug parsers ? */
            caps = gst_caps_new_empty_simple(Caps());
        }
        g_object_set(decoder, "caps", caps.get(), nullptr);

        m_sink = makeElement("appsink");
        gst_app_sink_set_emit_signals(GST_APP_SINK(m_sink), true);
        // This is a decoder, everything should happen as fast as possible and not be synced on the clock.
        g_object_set(m_sink, "sync", false, nullptr);

        gst_bin_add_many(GST_BIN(pipeline()), m_src, decoder, capsfilter, m_sink, nullptr);
        if (!gst_element_link(m_src, decoder)) {
            GST_ERROR_OBJECT(pipeline(), "Could not link src to decoder.");
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        if (!gst_element_link(capsfilter, m_sink)) {
            GST_ERROR_OBJECT(pipeline(), "Could not link capsfilter to sink.");
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        if (gst_element_set_state(pipeline(), GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
            GST_ERROR_OBJECT(pipeline(), "Could not set state to PLAYING.");
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        return true;
    }

    int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) override
    {
        m_imageReadyCb = callback;

        return WEBRTC_VIDEO_CODEC_OK;
    }

    virtual GstElement* CreateFilter()
    {
        return makeElement("identity");
    }

    int32_t Release() final
    {
        if (m_pipeline) {
            disconnectSimpleBusMessageCallback(m_pipeline.get());
            auto bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
            gst_bus_set_sync_handler(bus.get(), nullptr, nullptr, nullptr);
            gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
            m_src = nullptr;
            m_sink = nullptr;
            m_pipeline = nullptr;
        }

        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t Decode(const webrtc::EncodedImage& inputImage, int64_t) override
    {
        if (m_needsKeyframe) {
            if (inputImage._frameType != webrtc::VideoFrameType::kVideoFrameKey) {
                GST_ERROR("Waiting for keyframe but got a delta unit... asking for keyframe");
                return WEBRTC_VIDEO_CODEC_ERROR;
            }
            m_needsKeyframe = false;
        }

        if (!m_src) {
            GST_ERROR("No source set, can't decode.");
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        if (inputImage._encodedWidth && inputImage._encodedHeight)
            updateCapsFromImageSize(inputImage._encodedWidth, inputImage._encodedHeight);

        if (UNLIKELY(!m_caps)) {
            GST_ERROR("Encoded image caps not set");
            ASSERT_NOT_REACHED();
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        // FIXME: Use a GstBufferPool.
        GST_TRACE_OBJECT(pipeline(), "Pushing encoded image with RTP timestamp %u", inputImage.RtpTimestamp());
        auto buffer = adoptGRef(gstBufferNewWrappedFast(fastMemDup(inputImage.data(), inputImage.size()), inputImage.size()));

        gst_buffer_add_reference_timestamp_meta(buffer.get(), m_rtpTimestampCaps.get(), inputImage.RtpTimestamp(), GST_CLOCK_TIME_NONE);

        auto sample = adoptGRef(gst_sample_new(buffer.get(), m_caps.get(), nullptr, nullptr));
        switch (gst_app_src_push_sample(GST_APP_SRC(m_src), sample.get())) {
        case GST_FLOW_OK:
            break;
        case GST_FLOW_FLUSHING:
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        default:
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        return pullSample();
    }

    int32_t pullSample()
    {
        auto sample = adoptGRef(gst_app_sink_try_pull_sample(GST_APP_SINK(m_sink), GST_SECOND / 30));
        if (!sample) {
            GST_ERROR("Needs more data");
            return WEBRTC_VIDEO_CODEC_OK;
        }
        auto buffer = gst_sample_get_buffer(sample.get());
        auto meta = gst_buffer_get_reference_timestamp_meta(buffer, m_rtpTimestampCaps.get());
        RELEASE_ASSERT(meta);
        auto frame = convertGStreamerSampleToLibWebRTCVideoFrame(WTFMove(sample), meta->timestamp);
        GST_TRACE_OBJECT(pipeline(), "Pulled video frame with RTP timestamp %u from %" GST_PTR_FORMAT, static_cast<uint32_t>(meta->timestamp), buffer);
        m_imageReadyCb->Decoded(frame);
        return WEBRTC_VIDEO_CODEC_OK;
    }

    virtual void updateCapsFromImageSize(int width, int height)
    {
        if (m_width == width && m_height == height)
            return;

        m_width = width;
        m_height = height;
        m_caps = adoptGRef(gst_caps_new_simple(Caps(), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, nullptr));
    }

    void AddDecoderIfSupported(std::vector<webrtc::SdpVideoFormat>& codecList)
    {
        if (HasGstDecoder()) {
            auto formats = ConfigureSupportedDecoder();
            codecList.insert(codecList.end(), formats.begin(), formats.end());
        }
    }

    virtual std::vector<webrtc::SdpVideoFormat> ConfigureSupportedDecoder()
    {
        return { sdpVideoFormat() };
    }

    static GRefPtr<GstElementFactory> GstDecoderFactory(const char* capsStr)
    {
        auto allDecoders = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER,
            GST_RANK_MARGINAL);
        auto caps = adoptGRef(gst_caps_from_string(capsStr));
        auto decoders = gst_element_factory_list_filter(allDecoders,
            caps.get(), GST_PAD_SINK, FALSE);

        gst_plugin_feature_list_free(allDecoders);
        GRefPtr<GstElementFactory> res;
        if (decoders)
            res = GST_ELEMENT_FACTORY(decoders->data);
        gst_plugin_feature_list_free(decoders);

        return res;
    }

    bool HasGstDecoder()
    {
        return GstDecoderFactory(Caps());
    }

    virtual const gchar* Caps() = 0;
    virtual webrtc::VideoCodecType CodecType() = 0;
    const char* ImplementationName() const override { return "GStreamer"; }
    virtual const gchar* Name() = 0;
    virtual webrtc::SdpVideoFormat sdpVideoFormat() = 0;

protected:
    GRefPtr<GstCaps> m_caps;
    int m_width;
    int m_height;
    bool m_requireParse = false;
    bool m_needsKeyframe;

private:
    GRefPtr<GstElement> m_pipeline;
    GstElement* m_sink;
    GstElement* m_src;

    webrtc::DecodedImageCallback* m_imageReadyCb;

    GRefPtr<GstCaps> m_rtpTimestampCaps;
};

class H264Decoder : public GStreamerWebRTCVideoDecoder {
public:
    H264Decoder()
    {
        m_requireParse = true;

        auto& quirksManager = GStreamerQuirksManager::singleton();
        if (quirksManager.isEnabled())
            m_requireParse = quirksManager.shouldParseIncomingLibWebRTCBitStream();
    }

    bool Configure(const webrtc::VideoDecoder::Settings& codecSettings) final
    {
        if (codecSettings.codec_type() != webrtc::kVideoCodecH264)
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;

        return GStreamerWebRTCVideoDecoder::Configure(codecSettings);
    }

    void updateCapsFromImageSize(int width, int height) final
    {
        if (m_width == width && m_height == height)
            return;

        m_width = width;
        m_height = height;
        m_caps = adoptGRef(gst_caps_new_simple(Caps(), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height,
            "alignment", G_TYPE_STRING, "au", nullptr));
    }
    const gchar* Caps() final { return "video/x-h264"; }
    const gchar* Name() final { return "h264"; }
    webrtc::SdpVideoFormat sdpVideoFormat() final { return webrtc::SdpVideoFormat::H264(); }
    webrtc::VideoCodecType CodecType() final { return webrtc::kVideoCodecH264; }

    std::vector<webrtc::SdpVideoFormat> ConfigureSupportedDecoder() final
    {
        return supportedH264Formats();
    }
};

class VP8Decoder : public GStreamerWebRTCVideoDecoder {
public:
    VP8Decoder() { }
    const gchar* Caps() final { return "video/x-vp8"; }
    const gchar* Name() final { return "vp8"; }
    webrtc::SdpVideoFormat sdpVideoFormat() final { return webrtc::SdpVideoFormat::VP8(); }

    webrtc::VideoCodecType CodecType() final { return webrtc::kVideoCodecVP8; }
    static std::unique_ptr<webrtc::VideoDecoder> Create(const webrtc::Environment& environment)
    {
        auto factory = GstDecoderFactory("video/x-vp8");
        if (factory) {
            const auto* factoryName = GST_OBJECT_NAME(GST_OBJECT(factory.get()));
            if (!g_strcmp0(factoryName, "vp8dec") || !g_strcmp0(factoryName, "vp8alphadecodebin")) {
                GST_INFO("Our best GStreamer VP8 decoder is vp8dec, better use the one from LibWebRTC");
                return std::unique_ptr<webrtc::VideoDecoder>(new webrtc::LibvpxVp8Decoder(environment));
            }
        }

        return std::unique_ptr<webrtc::VideoDecoder>(new VP8Decoder());
    }
};

class VP9Decoder : public GStreamerWebRTCVideoDecoder {
public:
    VP9Decoder(bool isSupportingVP9Profile0 = true, bool isSupportingVP9Profile2 = true)
        : m_isSupportingVP9Profile0(isSupportingVP9Profile0)
        , m_isSupportingVP9Profile2(isSupportingVP9Profile2) { };

    const gchar* Caps() final { return "video/x-vp9"; }
    const gchar* Name() final { return "vp9"; }
    webrtc::SdpVideoFormat sdpVideoFormat() final { return webrtc::SdpVideoFormat::VP9Profile0(); }

    webrtc::VideoCodecType CodecType() final { return webrtc::kVideoCodecVP9; }
    static std::unique_ptr<webrtc::VideoDecoder> Create()
    {
        return std::unique_ptr<webrtc::VideoDecoder>(new VP9Decoder());
    }

    std::vector<webrtc::SdpVideoFormat> ConfigureSupportedDecoder() final
    {
        std::vector<webrtc::SdpVideoFormat> formats;
        if (m_isSupportingVP9Profile0)
            formats.push_back(webrtc::SdpVideoFormat::VP9Profile0());
        if (m_isSupportingVP9Profile2)
            formats.push_back(webrtc::SdpVideoFormat::VP9Profile2());
        return formats;
    }
private:
    bool m_isSupportingVP9Profile0;
    bool m_isSupportingVP9Profile2;
};

std::unique_ptr<webrtc::VideoDecoder> GStreamerVideoDecoderFactory::Create(const webrtc::Environment& environment, const webrtc::SdpVideoFormat& format)
{
    if (format.name == "H264")
        return std::unique_ptr<webrtc::VideoDecoder>(new H264Decoder());
    if (format == webrtc::SdpVideoFormat::VP8())
        return VP8Decoder::Create(environment);
    if (format.name == "VP9")
        return VP9Decoder::Create();

    GST_ERROR("Could not create decoder for %s", format.name.c_str());
    return nullptr;
}

GStreamerVideoDecoderFactory::GStreamerVideoDecoderFactory(bool isSupportingVP9Profile0, bool isSupportingVP9Profile2)
    : m_isSupportingVP9Profile0(isSupportingVP9Profile0)
    , m_isSupportingVP9Profile2(isSupportingVP9Profile2)
{
    ensureGStreamerInitialized();

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtcdec_debug, "webkitlibwebrtcvideodecoder", 0, "WebKit WebRTC video decoder");
    });
}
std::vector<webrtc::SdpVideoFormat> GStreamerVideoDecoderFactory::GetSupportedFormats() const
{
    std::vector<webrtc::SdpVideoFormat> formats;

    VP8Decoder().AddDecoderIfSupported(formats);
    VP9Decoder(m_isSupportingVP9Profile0, m_isSupportingVP9Profile2).AddDecoderIfSupported(formats);
    H264Decoder().AddDecoderIfSupported(formats);

    return formats;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(LIBWEBRTC) && USE(GSTREAMER)
