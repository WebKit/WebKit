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
#include "GStreamerRegistryScanner.h"
#include "GStreamerVideoCommon.h"
#include "GStreamerVideoFrameLibWebRTC.h"
#include "IntSize.h"
#include "webrtc/modules/video_coding/codecs/vp8/libvpx_vp8_decoder.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

GST_DEBUG_CATEGORY(webkit_webrtcdec_debug);
#define GST_CAT_DEFAULT webkit_webrtcdec_debug

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerVideoDecoderFactory);

class GStreamerWebRTCVideoDecoder : public webrtc::VideoDecoder {
public:
    GStreamerWebRTCVideoDecoder()
        : m_requireParse(false)
        , m_needsKeyframe(true)
    {
    }

    static void decodebinPadAddedCb(GstElement*, GstPad* srcPad, GstPad* sinkPad)
    {
        GST_INFO_OBJECT(srcPad, "Connecting pad to %" GST_PTR_FORMAT, sinkPad);
        if (gst_pad_link(srcPad, sinkPad) != GST_PAD_LINK_OK)
            ASSERT_NOT_REACHED();
    }

    GstElement* pipeline() const
    {
        return m_pipeline.get();
    }

    GstElement* makeElement(ASCIILiteral factoryName)
    {
        static Atomic<uint32_t> elementId;
        auto name = makeString(unsafeSpan(this->name()), "-dec-"_s, factoryName, "-"_s, elementId.exchangeAdd(1));
        return makeGStreamerElement(factoryName, name);
    }

    void handleError(GError* error)
    {
        if (!g_error_matches(error, GST_STREAM_ERROR, GST_STREAM_ERROR_DECODE))
            return;
        GST_INFO_OBJECT(pipeline(), "Needs keyframe, error: %s", error->message);
        m_needsKeyframe = true;
    }

    const char* ImplementationName() const override { return "GStreamer"; }

    bool Configure(const webrtc::VideoDecoder::Settings& codecSettings) override
    {
        m_src = makeElement("appsrc"_s);
        g_object_set(m_src.get(), "is-live", TRUE, "do-timestamp", TRUE, "max-buffers", static_cast<guint64>(2), "max-bytes", static_cast<guint64>(0), nullptr);
        m_rtpTimestampCaps = adoptGRef(gst_caps_new_empty_simple("timestamp/x-rtp"));

        auto decoder = makeElement("decodebin"_s);

        updateCapsFromImageSize({ codecSettings.max_render_resolution().Width(), codecSettings.max_render_resolution().Height() });

        m_pipeline = makeElement("pipeline"_s);
        connectSimpleBusMessageCallback(m_pipeline.get());

        auto clock = adoptGRef(gst_system_clock_obtain());
        gst_pipeline_use_clock(GST_PIPELINE(m_pipeline.get()), clock.get());
        gst_element_set_base_time(m_pipeline.get(), 0);
        gst_element_set_start_time(m_pipeline.get(), GST_CLOCK_TIME_NONE);

        m_sink = makeElement("appsink"_s);
        gst_app_sink_set_emit_signals(GST_APP_SINK_CAST(m_sink.get()), true);
        // This is a decoder, everything should happen as fast as possible and not be synced on the clock.
        g_object_set(m_sink.get(), "sync", FALSE, nullptr);

        auto sinkpad = adoptGRef(gst_element_get_static_pad(m_sink.get(), "sink"));
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
        GRefPtr<GstCaps> caps;
        if (m_requireParse) {
            caps = gst_caps_new_simple(mediaType(), "parsed", G_TYPE_BOOLEAN, TRUE, nullptr);

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
            // FIXME: How could we handle missing keyframes case if we do not plug parsers?
            caps = gst_caps_new_empty_simple(mediaType());
        }
        g_object_set(decoder, "caps", caps.get(), nullptr);

        gst_bin_add_many(GST_BIN(pipeline()), m_src.get(), decoder, m_sink.get(), nullptr);
        if (!gst_element_link(m_src.get(), decoder)) {
            GST_ERROR_OBJECT(pipeline(), "Could not link src to decoder.");
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

    int32_t Release() final
    {
        if (!m_pipeline)
            return WEBRTC_VIDEO_CODEC_OK;

        disconnectSimpleBusMessageCallback(m_pipeline.get());
        auto bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
        gst_bus_disable_sync_message_emission(bus.get());

        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
        m_src = nullptr;
        m_sink = nullptr;
        m_pipeline = nullptr;

        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t Decode(const webrtc::EncodedImage& inputImage, int64_t) override
    {
        if (m_needsKeyframe) {
            if (inputImage._frameType != webrtc::VideoFrameType::kVideoFrameKey) {
                GST_ERROR_OBJECT(pipeline(), "Waiting for keyframe but got a delta unit... asking for keyframe");
                return WEBRTC_VIDEO_CODEC_OK_REQUEST_KEYFRAME;
            }
            m_needsKeyframe = false;
        }

        if (!m_src) {
            GST_ERROR_OBJECT(pipeline(), "No source set, can't decode.");
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        if (inputImage._encodedWidth && inputImage._encodedHeight)
            updateCapsFromImageSize({ static_cast<int>(inputImage._encodedWidth), static_cast<int>(inputImage._encodedHeight) });

        if (!m_caps) [[unlikely]] {
            GST_ERROR_OBJECT(pipeline(), "Encoded image caps not set");
            ASSERT_NOT_REACHED();
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        GST_TRACE_OBJECT(pipeline(), "Pushing encoded image with RTP timestamp %u", inputImage.RtpTimestamp());
        auto encodedData = inputImage.GetEncodedData();
        if (!encodedData) {
            GST_ERROR_OBJECT(pipeline(), "Encoded image has no data buffer");
            return WEBRTC_VIDEO_CODEC_ERROR;
        }
        encodedData->AddRef();
        auto data = const_cast<uint8_t*>(encodedData->data());
        auto dataSize = encodedData->size();
        auto buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, data, dataSize, 0, dataSize, static_cast<gpointer>(encodedData.get()), [](gpointer data) {
            static_cast<webrtc::EncodedImageBufferInterface*>(data)->Release();
        }));
        if (!m_requireParse)
            gst_buffer_add_reference_timestamp_meta(buffer.get(), m_rtpTimestampCaps.get(), inputImage.RtpTimestamp(), GST_CLOCK_TIME_NONE);

        auto sample = adoptGRef(gst_sample_new(buffer.get(), m_caps.get(), nullptr, nullptr));
        switch (gst_app_src_push_sample(GST_APP_SRC_CAST(m_src.get()), sample.get())) {
        case GST_FLOW_OK:
            break;
        case GST_FLOW_FLUSHING:
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        default:
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        auto pulledSample = adoptGRef(gst_app_sink_try_pull_sample(GST_APP_SINK_CAST(m_sink.get()), GST_SECOND));
        if (!pulledSample) {
            GST_WARNING_OBJECT(pipeline(), "No decoded frame within timeout");
            return WEBRTC_VIDEO_CODEC_OK;
        }

        auto pulledBuffer = gst_sample_get_buffer(pulledSample.get());
        uint32_t rtpTimestamp = inputImage.RtpTimestamp();
        if (!m_requireParse) {
            if (auto meta = gst_buffer_get_reference_timestamp_meta(pulledBuffer, m_rtpTimestampCaps.get()))
                rtpTimestamp = static_cast<uint32_t>(meta->timestamp);
        }
        GST_TRACE_OBJECT(pipeline(), "Pulled video frame with RTP timestamp %u from %" GST_PTR_FORMAT, rtpTimestamp, pulledBuffer);
        auto frame = convertGStreamerSampleToLibWebRTCVideoFrame(WTF::move(pulledSample), rtpTimestamp);

        m_imageReadyCb->Decoded(frame);
        return WEBRTC_VIDEO_CODEC_OK;
    }

    virtual void updateCapsFromImageSize(IntSize&& newSize)
    {
        if (newSize == m_size)
            return;

        m_size = WTF::move(newSize);
        m_caps = adoptGRef(gst_caps_new_simple(mediaType(), "width", G_TYPE_INT, m_size.width(), "height", G_TYPE_INT, m_size.height(), nullptr));
    }

    void addDecoderIfSupported(std::vector<webrtc::SdpVideoFormat>& codecList)
    {
        if (!hasGStreamerDecoder())
            return;

        auto formats = configureSupportedDecoder();
        codecList.insert(codecList.end(), formats.begin(), formats.end());
    }

    virtual std::vector<webrtc::SdpVideoFormat> configureSupportedDecoder() { return { sdpVideoFormat() }; }

    static GRefPtr<GstElementFactory> GstDecoderFactory(ASCIILiteral capsStr)
    {
        return GStreamerRegistryScanner::singleton().isCodecSupported(GStreamerRegistryScanner::Configuration::Decoding, capsStr, false).factory;
    }

    bool hasGStreamerDecoder() { return GstDecoderFactory(mediaType()); }

    virtual ASCIILiteral mediaType() = 0;
    virtual webrtc::VideoCodecType codecType() = 0;
    virtual ASCIILiteral name() = 0;
    virtual webrtc::SdpVideoFormat sdpVideoFormat() = 0;

protected:
    GRefPtr<GstCaps> m_caps;
    IntSize m_size;
    bool m_requireParse = false;
    bool m_needsKeyframe;

private:
    GRefPtr<GstElement> m_pipeline;
    GRefPtr<GstElement> m_sink;
    GRefPtr<GstElement> m_src;
    GRefPtr<GstCaps> m_rtpTimestampCaps;

    webrtc::DecodedImageCallback* m_imageReadyCb;
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

    void updateCapsFromImageSize(IntSize&& newSize) final
    {
        GStreamerWebRTCVideoDecoder::updateCapsFromImageSize(WTF::move(newSize));
        gst_caps_set_simple(m_caps.get(), "alignment", G_TYPE_STRING, "au", "stream-format", G_TYPE_STRING, "byte-stream", nullptr);
    }

    ASCIILiteral mediaType() final { return "video/x-h264"_s; }
    ASCIILiteral name() final { return "h264"_s; }
    webrtc::SdpVideoFormat sdpVideoFormat() final { return webrtc::SdpVideoFormat::H264(); }
    webrtc::VideoCodecType codecType() final { return webrtc::kVideoCodecH264; }

    std::vector<webrtc::SdpVideoFormat> configureSupportedDecoder() final
    {
        return supportedH264Formats();
    }
};

class VP8Decoder : public GStreamerWebRTCVideoDecoder {
public:
    VP8Decoder() { }
    ASCIILiteral mediaType() final { return "video/x-vp8"_s; }
    ASCIILiteral name() final { return "vp8"_s; }
    webrtc::SdpVideoFormat sdpVideoFormat() final { return webrtc::SdpVideoFormat::VP8(); }

    webrtc::VideoCodecType codecType() final { return webrtc::kVideoCodecVP8; }
    static std::unique_ptr<webrtc::VideoDecoder> Create(const webrtc::Environment& environment)
    {
        auto factory = GstDecoderFactory("video/x-vp8"_s);
        if (!factory) {
            GST_INFO("No GStreamer VP8 decoder found, falling back to LibWebRTC for VP8 decoding.");
            return std::unique_ptr<webrtc::VideoDecoder>(new webrtc::LibvpxVp8Decoder(environment));
        }

        auto factoryName = CStringView::unsafeFromUTF8(GST_OBJECT_NAME(GST_OBJECT(factory.get())));
        if (equal(factoryName.span(), "vp8dec"_s) || equal(factoryName.span(), "vp8alphadecodebin"_s)) {
            GST_INFO("Our best GStreamer VP8 decoder is vp8dec, better use the one from LibWebRTC");
            return std::unique_ptr<webrtc::VideoDecoder>(new webrtc::LibvpxVp8Decoder(environment));
        }

        return std::unique_ptr<webrtc::VideoDecoder>(new VP8Decoder());
    }
};

class VP9Decoder : public GStreamerWebRTCVideoDecoder {
public:
    VP9Decoder(bool isSupportingVP9Profile0 = true, bool isSupportingVP9Profile2 = true)
        : m_isSupportingVP9Profile0(isSupportingVP9Profile0)
        , m_isSupportingVP9Profile2(isSupportingVP9Profile2) { };

    ASCIILiteral mediaType() final { return "video/x-vp9"_s; }
    ASCIILiteral name() final { return "vp9"_s; }
    webrtc::SdpVideoFormat sdpVideoFormat() final { return webrtc::SdpVideoFormat::VP9Profile0(); }

    webrtc::VideoCodecType codecType() final { return webrtc::kVideoCodecVP9; }
    static std::unique_ptr<webrtc::VideoDecoder> Create()
    {
        return std::unique_ptr<webrtc::VideoDecoder>(new VP9Decoder());
    }

    std::vector<webrtc::SdpVideoFormat> configureSupportedDecoder() final
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

    VP8Decoder().addDecoderIfSupported(formats);
    VP9Decoder(m_isSupportingVP9Profile0, m_isSupportingVP9Profile2).addDecoderIfSupported(formats);
    H264Decoder().addDecoderIfSupported(formats);

    return formats;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(LIBWEBRTC) && USE(GSTREAMER)
