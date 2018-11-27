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

#include "GStreamerVideoEncoder.h"
#include "GStreamerVideoFrameLibWebRTC.h"
#include "webrtc/common_video/h264/h264_common.h"
#include "webrtc/common_video/h264/profile_level_id.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp8/libvpx_vp8_encoder.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/utility/simulcast_utility.h"

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#define GST_USE_UNSTABLE_API 1
#include <gst/codecparsers/gsth264parser.h>
#undef GST_USE_UNSTABLE_API
#include <gst/pbutils/encoding-profile.h>
#include <gst/video/video.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/StdMap.h>

// Required for unified builds
#ifdef GST_CAT_DEFAULT
#undef GST_CAT_DEFAULT
#endif

GST_DEBUG_CATEGORY(webkit_webrtcenc_debug);
#define GST_CAT_DEFAULT webkit_webrtcenc_debug

#define KBIT_TO_BIT 1024

namespace WebCore {

typedef struct {
    uint64_t rtpTimestamp;
    int64_t captureTimeMs;
    webrtc::CodecSpecificInfo codecInfo;
} FrameData;

class GStreamerVideoEncoder : public webrtc::VideoEncoder {
public:
    GStreamerVideoEncoder(const webrtc::SdpVideoFormat&)
        : m_firstFramePts(GST_CLOCK_TIME_NONE)
        , m_restrictionCaps(adoptGRef(gst_caps_new_empty_simple("video/x-raw")))
        , m_adapter(adoptGRef(gst_adapter_new()))
    {
    }
    GStreamerVideoEncoder()
        : m_firstFramePts(GST_CLOCK_TIME_NONE)
        , m_restrictionCaps(adoptGRef(gst_caps_new_empty_simple("video/x-raw")))
        , m_adapter(adoptGRef(gst_adapter_new()))
    {
    }

    int SetRates(uint32_t newBitrate, uint32_t frameRate) override
    {
        GST_INFO_OBJECT(m_pipeline.get(), "New bitrate: %d - framerate is %d",
            newBitrate, frameRate);

        auto caps = adoptGRef(gst_caps_copy(m_restrictionCaps.get()));

        SetRestrictionCaps(WTFMove(caps));

        if (m_encoder)
            g_object_set(m_encoder, "bitrate", newBitrate, nullptr);

        return WEBRTC_VIDEO_CODEC_OK;
    }

    GstElement* pipeline()
    {
        return m_pipeline.get();
    }

    GstElement* makeElement(const gchar* factoryName)
    {
        auto name = String::format("%s_enc_%s_%p", Name(), factoryName, this);
        auto elem = gst_element_factory_make(factoryName, name.utf8().data());

        return elem;
    }

    int32_t InitEncode(const webrtc::VideoCodec* codecSettings, int32_t, size_t)
    {
        g_return_val_if_fail(codecSettings, WEBRTC_VIDEO_CODEC_ERR_PARAMETER);
        g_return_val_if_fail(codecSettings->codecType == CodecType(), WEBRTC_VIDEO_CODEC_ERR_PARAMETER);

        if (webrtc::SimulcastUtility::NumberOfSimulcastStreams(*codecSettings) > 1) {
            GST_ERROR("Simulcast not supported.");

            return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
        }

        m_encodedFrame._size = codecSettings->width * codecSettings->height * 3;
        m_encodedFrame._buffer = new uint8_t[m_encodedFrame._size];
        m_encodedImageBuffer.reset(m_encodedFrame._buffer);
        m_encodedFrame._completeFrame = true;
        m_encodedFrame._encodedWidth = 0;
        m_encodedFrame._encodedHeight = 0;
        m_encodedFrame._length = 0;

        m_pipeline = makeElement("pipeline");

        connectSimpleBusMessageCallback(m_pipeline.get());
        auto encoder = createEncoder();
        ASSERT(encoder);
        m_encoder = encoder.get();

        g_object_set(m_encoder, "keyframe-interval", KeyframeInterval(codecSettings), nullptr);

        m_src = makeElement("appsrc");
        g_object_set(m_src, "is-live", true, "format", GST_FORMAT_TIME, nullptr);

        auto videoconvert = makeElement("videoconvert");
        auto sink = makeElement("appsink");
        gst_app_sink_set_emit_signals(GST_APP_SINK(sink), TRUE);
        g_signal_connect(sink, "new-sample", G_CALLBACK(newSampleCallbackTramp), this);

        auto name = String::format("%s_enc_rawcapsfilter_%p", Name(), this);
        m_capsFilter = gst_element_factory_make("capsfilter", name.utf8().data());
        if (m_restrictionCaps)
            g_object_set(m_capsFilter, "caps", m_restrictionCaps.get(), nullptr);

        gst_bin_add_many(GST_BIN(m_pipeline.get()), m_src, videoconvert, m_capsFilter, encoder.leakRef(), sink, nullptr);
        if (!gst_element_link_many(m_src, videoconvert, m_capsFilter, m_encoder, sink, nullptr))
            ASSERT_NOT_REACHED();

        gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);

        return WEBRTC_VIDEO_CODEC_OK;
    }

    bool SupportsNativeHandle() const final
    {
        return true;
    }

    int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) final
    {
        m_imageReadyCb = callback;

        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t Release() final
    {
        m_encodedFrame._buffer = nullptr;
        m_encodedImageBuffer.reset();
        if (m_pipeline) {
            GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
            gst_bus_set_sync_handler(bus.get(), nullptr, nullptr, nullptr);

            gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
            m_src = nullptr;
            m_encoder = nullptr;
            m_capsFilter = nullptr;
            m_pipeline = nullptr;
        }

        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t SetChannelParameters(uint32_t, int64_t) final
    {
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t Encode(const webrtc::VideoFrame& frame,
        const webrtc::CodecSpecificInfo* codecInfo,
        const std::vector<webrtc::FrameType>* frameTypes) final
    {
        if (!m_imageReadyCb) {
            GST_INFO_OBJECT(m_pipeline.get(), "No encoded callback set yet!");

            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        if (!m_src) {
            GST_INFO_OBJECT(m_pipeline.get(), "No source set yet!");

            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        auto sample = GStreamerSampleFromLibWebRTCVideoFrame(frame);
        auto buffer = gst_sample_get_buffer(sample.get());

        if (!GST_CLOCK_TIME_IS_VALID(m_firstFramePts)) {
            m_firstFramePts = GST_BUFFER_PTS(buffer);
            auto pad = adoptGRef(gst_element_get_static_pad(m_src, "src"));
            gst_pad_set_offset(pad.get(), -m_firstFramePts);
        }

        webrtc::CodecSpecificInfo localCodecInfo;
        FrameData frameData = { frame.timestamp(), frame.render_time_ms(), codecInfo ? *codecInfo : localCodecInfo };
        {
            auto locker = holdLock(m_bufferMapLock);
            m_framesData.append(frameData);
        }

        for (auto frame_type : *frameTypes) {
            if (frame_type == webrtc::kVideoFrameKey) {
                auto pad = adoptGRef(gst_element_get_static_pad(m_src, "src"));
                auto forceKeyUnit = gst_video_event_new_downstream_force_key_unit(GST_CLOCK_TIME_NONE,
                    GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, FALSE, 1);
                GST_INFO_OBJECT(m_pipeline.get(), "Requesting KEYFRAME!");

                if (!gst_pad_push_event(pad.get(), forceKeyUnit))
                    GST_WARNING_OBJECT(pipeline(), "Could not send ForceKeyUnit event");

                break;
            }
        }

        switch (gst_app_src_push_sample(GST_APP_SRC(m_src), sample.get())) {
        case GST_FLOW_OK:
            return WEBRTC_VIDEO_CODEC_OK;
        case GST_FLOW_FLUSHING:
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        default:
            return WEBRTC_VIDEO_CODEC_ERROR;
        }
    }

    GstFlowReturn newSampleCallback(GstElement* sink)
    {
        auto sample = adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(sink)));
        auto buffer = gst_sample_get_buffer(sample.get());
        auto caps = gst_sample_get_caps(sample.get());

        webrtc::CodecSpecificInfo localCodecInfo;
        FrameData frameData = { 0, 0, localCodecInfo};
        {
            auto locker = holdLock(m_bufferMapLock);
            if (!m_framesData.size()) {
                gst_adapter_push(m_adapter.get(), gst_buffer_ref(buffer));

                return GST_FLOW_OK;
            }

            if (gst_adapter_available(m_adapter.get()) > 0) {
                uint flags = GST_BUFFER_FLAGS(buffer);

                GST_INFO_OBJECT(m_pipeline.get(), "Got more buffer than pushed frame, trying to deal with it.");
                gst_adapter_push(m_adapter.get(), gst_buffer_ref(buffer));

                buffer = gst_adapter_take_buffer(m_adapter.get(), gst_adapter_available(m_adapter.get()));
                GST_BUFFER_FLAGS(buffer) = flags;
            }
            frameData = m_framesData[0];
            m_framesData.remove(static_cast<size_t>(0));
        }

        webrtc::RTPFragmentationHeader fragmentationInfo;
        Fragmentize(&m_encodedFrame, &m_encodedImageBuffer, &m_encodedImageBufferSize, buffer, &fragmentationInfo);
        if (!m_encodedFrame._size)
            return GST_FLOW_OK;

        gst_structure_get(gst_caps_get_structure(caps, 0),
            "width", G_TYPE_INT, &m_encodedFrame._encodedWidth,
            "height", G_TYPE_INT, &m_encodedFrame._encodedHeight,
            nullptr);

        m_encodedFrame._frameType = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT) ? webrtc::kVideoFrameDelta : webrtc::kVideoFrameKey;
        m_encodedFrame._completeFrame = false;
        m_encodedFrame.capture_time_ms_ = frameData.captureTimeMs;
        m_encodedFrame.SetTimestamp(frameData.rtpTimestamp);

        GST_LOG_OBJECT(m_pipeline.get(), "Got buffer capture_time_ms: %ld _timestamp: %u",
            m_encodedFrame.capture_time_ms_, m_encodedFrame.Timestamp());

        PopulateCodecSpecific(&frameData.codecInfo, buffer);

        webrtc::EncodedImageCallback::Result result = m_imageReadyCb->OnEncodedImage(m_encodedFrame, &frameData.codecInfo, &fragmentationInfo);
        if (result.error != webrtc::EncodedImageCallback::Result::OK)
            GST_ERROR_OBJECT(m_pipeline.get(), "Encode callback failed: %d", result.error);

        return GST_FLOW_OK;
    }

    GRefPtr<GstElement> createEncoder(void)
    {
        GRefPtr<GstElement> encoder = nullptr;
        GstElement* webrtcencoder = GST_ELEMENT(g_object_ref_sink(gst_element_factory_make("webrtcvideoencoder", NULL)));

        g_object_set(webrtcencoder, "format", adoptGRef(gst_caps_from_string(Caps())).get(), NULL);
        g_object_get(webrtcencoder, "encoder", &encoder.outPtr(), NULL);

        if (!encoder) {
            GST_INFO("No encoder found for %s", Caps());

            return nullptr;
        }

        return webrtcencoder;
    }

    void AddCodecIfSupported(std::vector<webrtc::SdpVideoFormat>* supportedFormats)
    {
        GstElement* encoder;

        if (createEncoder().get() != nullptr) {
            webrtc::SdpVideoFormat format = ConfigureSupportedCodec(encoder);

            supportedFormats->push_back(format);
        }
    }

    virtual const gchar* Caps()
    {
        return nullptr;
    }

    virtual webrtc::VideoCodecType CodecType() = 0;
    virtual webrtc::SdpVideoFormat ConfigureSupportedCodec(GstElement*)
    {
        return webrtc::SdpVideoFormat(Name());
    }

    virtual void PopulateCodecSpecific(webrtc::CodecSpecificInfo*, GstBuffer*) = 0;

    virtual void Fragmentize(webrtc::EncodedImage* encodedImage, std::unique_ptr<uint8_t[]>* encodedImageBuffer,
        size_t* bufferSize, GstBuffer* buffer, webrtc::RTPFragmentationHeader* fragmentationInfo)
    {
        GstMappedBuffer map(buffer, GST_MAP_READ);

        if (*bufferSize < map.size()) {
            encodedImage->_size = map.size();
            encodedImage->_buffer = new uint8_t[encodedImage->_size];
            encodedImageBuffer->reset(encodedImage->_buffer);
            *bufferSize = map.size();
        }

        memcpy(encodedImage->_buffer, map.data(), map.size());
        encodedImage->_length = map.size();
        encodedImage->_size = map.size();

        fragmentationInfo->VerifyAndAllocateFragmentationHeader(1);
        fragmentationInfo->fragmentationOffset[0] = 0;
        fragmentationInfo->fragmentationLength[0] = map.size();
        fragmentationInfo->fragmentationPlType[0] = 0;
        fragmentationInfo->fragmentationTimeDiff[0] = 0;
    }

    const char* ImplementationName() const
    {
        GRefPtr<GstElement> encoderImplementation;
        g_return_val_if_fail(m_encoder, nullptr);

        g_object_get(m_encoder, "encoder", &encoderImplementation.outPtr(), nullptr);

        return GST_OBJECT_NAME(gst_element_get_factory(encoderImplementation.get()));
    }

    virtual const gchar* Name() = 0;
    virtual int KeyframeInterval(const webrtc::VideoCodec* codecSettings) = 0;

    void SetRestrictionCaps(GRefPtr<GstCaps> caps)
    {
        if (m_restrictionCaps)
            g_object_set(m_capsFilter, "caps", m_restrictionCaps.get(), nullptr);

        m_restrictionCaps = caps;
    }

private:
    static GstFlowReturn newSampleCallbackTramp(GstElement* sink, GStreamerVideoEncoder* enc)
    {
        return enc->newSampleCallback(sink);
    }

    GRefPtr<GstElement> m_pipeline;
    GstElement* m_src;
    GstElement* m_encoder;
    GstElement* m_capsFilter;

    webrtc::EncodedImageCallback* m_imageReadyCb;
    GstClockTime m_firstFramePts;
    GRefPtr<GstCaps> m_restrictionCaps;
    webrtc::EncodedImage m_encodedFrame;
    std::unique_ptr<uint8_t[]> m_encodedImageBuffer;
    size_t m_encodedImageBufferSize;

    Lock m_bufferMapLock;
    GRefPtr<GstAdapter> m_adapter;
    Vector<FrameData> m_framesData;
};

class GStreamerH264Encoder : public GStreamerVideoEncoder {
public:
    GStreamerH264Encoder() { }

    GStreamerH264Encoder(const webrtc::SdpVideoFormat& format)
        : m_parser(gst_h264_nal_parser_new())
        , packetizationMode(webrtc::H264PacketizationMode::NonInterleaved)
    {
        auto it = format.parameters.find(cricket::kH264FmtpPacketizationMode);

        if (it != format.parameters.end() && it->second == "1")
            packetizationMode = webrtc::H264PacketizationMode::NonInterleaved;
    }

    int KeyframeInterval(const webrtc::VideoCodec* codecSettings) final
    {
        return codecSettings->H264().keyFrameInterval;
    }

    // FIXME - MT. safety!
    void Fragmentize(webrtc::EncodedImage* encodedImage, std::unique_ptr<uint8_t[]>* encodedImageBuffer, size_t *bufferSize,
        GstBuffer* gstbuffer, webrtc::RTPFragmentationHeader* fragmentationHeader) final
    {
        GstH264NalUnit nalu;
        auto parserResult = GST_H264_PARSER_OK;

        gsize offset = 0;
        size_t requiredSize = 0;

        std::vector<GstH264NalUnit> nals;

        const uint8_t startCode[4] = { 0, 0, 0, 1 };
        GstMappedBuffer map(gstbuffer, GST_MAP_READ);
        while (parserResult == GST_H264_PARSER_OK) {
            parserResult = gst_h264_parser_identify_nalu(m_parser, map.data(), offset, map.size(), &nalu);

            nalu.sc_offset = offset;
            nalu.offset = offset + sizeof(startCode);
            if (parserResult != GST_H264_PARSER_OK && parserResult != GST_H264_PARSER_NO_NAL_END)
                break;

            requiredSize += nalu.size + sizeof(startCode);
            nals.push_back(nalu);
            offset = nalu.offset + nalu.size;
        }

        if (encodedImage->_size < requiredSize) {
            encodedImage->_size = requiredSize;
            encodedImage->_buffer = new uint8_t[encodedImage->_size];
            encodedImageBuffer->reset(encodedImage->_buffer);
            *bufferSize = map.size();
        }

        // Iterate nal units and fill the Fragmentation info.
        fragmentationHeader->VerifyAndAllocateFragmentationHeader(nals.size());
        size_t fragmentIndex = 0;
        encodedImage->_length = 0;
        for (std::vector<GstH264NalUnit>::iterator nal = nals.begin(); nal != nals.end(); ++nal, fragmentIndex++) {

            ASSERT(map.data()[nal->sc_offset + 0] == startCode[0]);
            ASSERT(map.data()[nal->sc_offset + 1] == startCode[1]);
            ASSERT(map.data()[nal->sc_offset + 2] == startCode[2]);
            ASSERT(map.data()[nal->sc_offset + 3] == startCode[3]);

            fragmentationHeader->fragmentationOffset[fragmentIndex] = nal->offset;
            fragmentationHeader->fragmentationLength[fragmentIndex] = nal->size;

            memcpy(encodedImage->_buffer + encodedImage->_length, &map.data()[nal->sc_offset],
                sizeof(startCode) + nal->size);
            encodedImage->_length += nal->size + sizeof(startCode);
        }
    }

    webrtc::SdpVideoFormat ConfigureSupportedCodec(GstElement*) final
    {
        // TODO- Create from encoder src pad caps template
        return webrtc::SdpVideoFormat(cricket::kH264CodecName,
            { { cricket::kH264FmtpProfileLevelId, cricket::kH264ProfileLevelConstrainedBaseline },
                { cricket::kH264FmtpLevelAsymmetryAllowed, "1" },
                { cricket::kH264FmtpPacketizationMode, "1" } });
    }

    const gchar* Caps() final { return "video/x-h264"; }
    const gchar* Name() final { return cricket::kH264CodecName; }
    GstH264NalParser* m_parser;
    webrtc::VideoCodecType CodecType() final { return webrtc::kVideoCodecH264; }

    void PopulateCodecSpecific(webrtc::CodecSpecificInfo* codecSpecifiInfos, GstBuffer*) final
    {
        codecSpecifiInfos->codecType = CodecType();
        codecSpecifiInfos->codec_name = ImplementationName();
        webrtc::CodecSpecificInfoH264* h264Info = &(codecSpecifiInfos->codecSpecific.H264);
        h264Info->packetization_mode = packetizationMode;
    }

    webrtc::H264PacketizationMode packetizationMode;
};

class GStreamerVP8Encoder : public GStreamerVideoEncoder {
public:
    GStreamerVP8Encoder() { }
    GStreamerVP8Encoder(const webrtc::SdpVideoFormat&) { }
    const gchar* Caps() final { return "video/x-vp8"; }
    const gchar* Name() final { return cricket::kVp8CodecName; }
    webrtc::VideoCodecType CodecType() final { return webrtc::kVideoCodecVP8; }

    int KeyframeInterval(const webrtc::VideoCodec* codecSettings) final
    {
        return codecSettings->VP8().keyFrameInterval;
    }

    void PopulateCodecSpecific(webrtc::CodecSpecificInfo* codecSpecifiInfos, GstBuffer* buffer) final
    {
        codecSpecifiInfos->codecType = webrtc::kVideoCodecVP8;
        codecSpecifiInfos->codec_name = ImplementationName();
        webrtc::CodecSpecificInfoVP8* vp8Info = &(codecSpecifiInfos->codecSpecific.VP8);
        vp8Info->temporalIdx = 0;

        vp8Info->keyIdx = webrtc::kNoKeyIdx;
        vp8Info->nonReference = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
    }
};

std::unique_ptr<webrtc::VideoEncoder> GStreamerVideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat& format)
{
    if (format.name == cricket::kVp8CodecName) {
        GRefPtr<GstElement> webrtcencoder = adoptGRef(GST_ELEMENT(g_object_ref_sink(gst_element_factory_make("webrtcvideoencoder", NULL))));
        GRefPtr<GstElement> encoder = nullptr;

        g_object_set(webrtcencoder.get(), "format", adoptGRef(gst_caps_from_string("video/x-vp8")).get(), NULL);
        g_object_get(webrtcencoder.get(), "encoder", &encoder.outPtr(), NULL);

        if (encoder)
            return std::make_unique<GStreamerVP8Encoder>(format);

        GST_INFO("Using VP8 Encoder from LibWebRTC.");
        return std::make_unique<webrtc::LibvpxVp8Encoder>();
    }

    if (format.name == cricket::kH264CodecName)
        return std::make_unique<GStreamerH264Encoder>(format);

    return nullptr;
}

GStreamerVideoEncoderFactory::GStreamerVideoEncoderFactory()
{
    static std::once_flag debugRegisteredFlag;

    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtcenc_debug, "webkitlibwebrtcvideoencoder", 0, "WebKit WebRTC video encoder");
        gst_element_register(nullptr, "webrtcvideoencoder", GST_RANK_PRIMARY, GST_TYPE_WEBRTC_VIDEO_ENCODER);
    });
}

std::vector<webrtc::SdpVideoFormat> GStreamerVideoEncoderFactory::GetSupportedFormats() const
{
    std::vector<webrtc::SdpVideoFormat> supportedCodecs;

    supportedCodecs.push_back(webrtc::SdpVideoFormat(cricket::kVp8CodecName));
    GStreamerH264Encoder().AddCodecIfSupported(&supportedCodecs);

    return supportedCodecs;
}

} // namespace WebCore
#endif
