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

#if USE(LIBWEBRTC) && USE(GSTREAMER)
#include "GStreamerVideoEncoderFactory.h"

#include "FloatSize.h"
#include "GStreamerRegistryScanner.h"
#include "GStreamerVideoFrameLibWebRTC.h"
#include "LibWebRTCUtils.h"
#include "LibWebRTCVideoFrameUtilities.h"
#include "LibWebRTCWebKitMacros.h"
#include "VideoEncoderGStreamer.h"
#include "VideoEncoderPrivateGStreamer.h"
#include "VideoEncoderScalabilityMode.h"
#include "VideoFrameGStreamer.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/video_codecs/vp9_profile.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"

#include <gst/video/video.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/StdMap.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

GST_DEBUG_CATEGORY(webkit_webrtcenc_debug);
#define GST_CAT_DEFAULT webkit_webrtcenc_debug

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerVideoEncoderFactory);

class GStreamerEncodedImageBuffer : public webrtc::EncodedImageBufferInterface {
public:
    static webrtc::scoped_refptr<GStreamerEncodedImageBuffer> create(std::span<const uint8_t> data)
    {
        return webrtc::make_ref_counted<GStreamerEncodedImageBuffer>(data);
    }

    const uint8_t* data() const override { return m_data.data(); }
    size_t size() const override { return m_data.size_bytes(); }

protected:
    GStreamerEncodedImageBuffer(std::span<const uint8_t> data)
        : m_data(data)
    {
    }

private:
    std::span<const uint8_t> m_data;
};

class LibWebRTCGStreamerVideoEncoder final : public webrtc::VideoEncoder {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(LibWebRTCGStreamerVideoEncoder);
public:
    LibWebRTCGStreamerVideoEncoder(const webrtc::SdpVideoFormat& sdpVideoFormat)
        : m_sdpVideoFormat(sdpVideoFormat)
    {
        WebCore::VideoEncoder::Config config;
        StringBuilder builder;
        if (m_sdpVideoFormat.IsSameCodec(webrtc::SdpVideoFormat::H264())) {
            builder.append("avc1"_s);
            const auto profileLevelId = m_sdpVideoFormat.parameters["profile-level-id"];
            if (!profileLevelId.empty())
                builder.append('.', fromStdString(profileLevelId));
            m_codecInfo.codecType = webrtc::kVideoCodecH264;
            m_codecInfo.codecSpecific.H264.packetization_mode = webrtc::H264PacketizationMode::NonInterleaved;
            config.useAnnexB = true;
        } else if (m_sdpVideoFormat.IsSameCodec(webrtc::SdpVideoFormat::VP8())) {
            builder.append("vp8"_s);
            m_codecInfo.codecType = webrtc::kVideoCodecVP8;
        }

        auto codecName = builder.toString();
        if (codecName.isEmpty()) {
            gst_printerrln("Unable to create GStreamer video encoder for format %s", sdpVideoFormat.ToString().c_str());
            return;
        }

        GST_INFO("Creating WebRTC video encoder for codec %s", codecName.ascii().data());
        auto result = GStreamerVideoEncoder::create(codecName, config, [](auto&&) { }, [&](auto&& encodedFrame) {
            notifyEncodedFrame(WTF::move(encodedFrame));
        });
        if (!result) {
            gst_printerrln("Unable to create GStreamer video encoder: %s", result.error().utf8().data());
            return;
        }
        m_internalEncoder = WTF::move(*result);
    }

    void notifyEncodedFrame(WebCore::VideoEncoder::EncodedFrame&& frame)
    {
        if (!m_encodedImageCallback) [[unlikely]]
            return;

        auto encodedImageBuffer = GStreamerEncodedImageBuffer::create(frame.data.span());

        webrtc::EncodedImage encodedImage;
        encodedImage.SetEncodedData(encodedImageBuffer);
        if (!encodedImage.size()) [[unlikely]]
            return;

        encodedImage._encodedWidth = m_size.width();
        encodedImage._encodedHeight = m_size.height();
        encodedImage.SetFrameType(frame.isKeyFrame ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta);
        encodedImage.SetPresentationTimestamp(webrtc::Timestamp::Millis(frame.timestamp));

        if (auto rtpTimestamp = m_rtpTimestamps.takeOptional(frame.timestamp)) [[likely]]
            encodedImage.SetRtpTimestamp(*rtpTimestamp);

        if (m_codecInfo.codecType == webrtc::kVideoCodecVP8)
            m_codecInfo.codecSpecific.VP8.temporalIdx = frame.temporalIndex.value_or(0);

        m_encodedImageCallback->OnEncodedImage(encodedImage, &m_codecInfo);
    }

    void SetRates(const webrtc::VideoEncoder::RateControlParameters& parameters) final
    {
        if (!m_internalEncoder)
            return;

        size_t totalSpatialLayers = 0;
        for (size_t spatialIndex = 0; spatialIndex < webrtc::kMaxSpatialLayers; spatialIndex++) {
            if (!parameters.bitrate.HasBitrate(spatialIndex, 0))
                break;
            totalSpatialLayers++;
        }
        size_t totalTemporalLayers = 0;
        for (size_t temporalIndex = 0; temporalIndex < webrtc::kMaxTemporalStreams; temporalIndex++) {
            if (!parameters.bitrate.HasBitrate(0, temporalIndex))
                break;
            totalTemporalLayers++;
        }

        WebCore::VideoEncoderScalabilityMode mode;
        switch (totalSpatialLayers) {
        case 1: {
            switch (totalTemporalLayers) {
            case 1:
                mode = WebCore::VideoEncoderScalabilityMode::L1T1;
                break;
            case 2:
                mode = WebCore::VideoEncoderScalabilityMode::L1T2;
                break;
            case 3:
                mode = WebCore::VideoEncoderScalabilityMode::L1T3;
                break;
            default:
                gst_printerrln("Unsupported scalability mode for 1 spatial layer and %zu temporal layers", totalTemporalLayers);
                return;
            }
            break;
        }
        default:
            gst_printerrln("Unsupported scalability mode for %zu spatial layers", totalSpatialLayers);
            return;
        }
        auto bitRateAllocation = WebCore::WebKitVideoEncoderBitRateAllocation::create(mode);
        for (size_t spatialIndex = 0; spatialIndex < totalSpatialLayers; spatialIndex++) {
            for (size_t temporalIndex = 0; temporalIndex < totalTemporalLayers; temporalIndex++) {
                if (!parameters.bitrate.HasBitrate(spatialIndex, temporalIndex))
                    continue;
                auto bitRate = parameters.bitrate.GetBitrate(spatialIndex, temporalIndex);
                if (!bitRate)
                    continue;
                bitRateAllocation->setBitRate(spatialIndex, temporalIndex, bitRate);
            }
        }
        m_frameRate = parameters.framerate_fps;
        static_cast<GStreamerVideoEncoder&>(*m_internalEncoder).setBitRateAllocation(WTF::move(bitRateAllocation), parameters.framerate_fps);
    }

    int32_t InitEncode(const webrtc::VideoCodec* codecSettings, const webrtc::VideoEncoder::Settings&) final
    {
        if (!codecSettings)
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;

        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) final
    {
        m_encodedImageCallback = callback;
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t Release() final
    {
        if (m_internalEncoder)
            m_internalEncoder->close();
        return WEBRTC_VIDEO_CODEC_OK;
    }

    VideoEncoder::EncoderInfo GetEncoderInfo() const final
    {
        VideoEncoder::EncoderInfo info;
        info.implementation_name = "GStreamer";
        info.has_trusted_rate_controller = true;
        return info;
    }

    int32_t Encode(const webrtc::VideoFrame& frame, const std::vector<webrtc::VideoFrameType>* frameTypes) final
    {
        if (!m_internalEncoder) [[unlikely]]
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

        if (!m_encodedImageCallback) [[unlikely]]
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

        bool shouldGenerateKeyFrame = false;
        for (const auto& frame_type : *frameTypes) {
            if (frame_type == webrtc::VideoFrameType::kVideoFrameKey) {
                shouldGenerateKeyFrame = true;
                break;
            }
        }

        m_rtpTimestamps.add(frame.render_time_ms(), frame.rtp_timestamp());

        auto options = VideoFrameGStreamer::CreateOptions();
        options.rotation = videoRotationFromLibWebRTCVideoFrame(frame);
        auto colorSpace = colorSpaceFromLibWebRTCVideoFrame(frame);
        auto sample = convertLibWebRTCVideoFrameToGStreamerSample(frame);

        if (m_frameRate) {
            int framerateNumerator, framerateDenominator;
            gst_util_double_to_fraction(*m_frameRate, &framerateNumerator, &framerateDenominator);
            GRefPtr caps = gst_sample_get_caps(sample.get());
            auto writableCaps = adoptGRef(gst_caps_make_writable(caps.leakRef()));

            sample = adoptGRef(gst_sample_make_writable(sample.leakRef()));
            gst_caps_set_simple(writableCaps.get(), "framerate", GST_TYPE_FRACTION, framerateNumerator, framerateDenominator, nullptr);
            gst_sample_set_caps(sample.get(), writableCaps.get());
        }

        auto gstVideoFrame = VideoFrameGStreamer::create(WTF::move(sample), options, colorSpace.value_or(PlatformVideoColorSpace { }));
        m_size = gstVideoFrame->presentationSize();
        WebCore::VideoEncoder::RawFrame rawFrame { WTF::move(gstVideoFrame), frame.render_time_ms(), { } };
        m_internalEncoder->encode(WTF::move(rawFrame), shouldGenerateKeyFrame);
        return WEBRTC_VIDEO_CODEC_OK;
    }

private:
    webrtc::SdpVideoFormat m_sdpVideoFormat;
    webrtc::CodecSpecificInfo m_codecInfo;
    RefPtr<WebCore::VideoEncoder> m_internalEncoder;
    webrtc::EncodedImageCallback* m_encodedImageCallback;
    IntSize m_size;
    std::optional<double> m_frameRate;
    HashMap<int64_t, uint32_t, WTF::IntHash<int64_t>, WTF::UnsignedWithZeroKeyHashTraits<int64_t>> m_rtpTimestamps;
};

std::unique_ptr<webrtc::VideoEncoder> GStreamerVideoEncoderFactory::Create(const webrtc::Environment& environment, const webrtc::SdpVideoFormat& format)
{
    // FIXME: vp9enc doesn't support simulcast nor SVC. These shortcomings trigger webrtc/vp9.html
    // and webrtc/simulcast-h264.html timeouts and most likely bad UX in WPE/GTK browsers. So for
    // now we prefer to use LibWebRTC's VP9 encoders.
    if (format == webrtc::SdpVideoFormat::VP9Profile0()) {
        GST_INFO("Using VP9 P0 encoder from LibWebRTC.");
        return webrtc::CreateVp9Encoder(environment, { webrtc::VP9Profile::kProfile0 });
    }
    if (format == webrtc::SdpVideoFormat::VP9Profile2()) {
        GST_INFO("Using VP9 P2 encoder from LibWebRTC.");
        return webrtc::CreateVp9Encoder(environment, { webrtc::VP9Profile::kProfile2 });
    }

    if (format.IsSameCodec(webrtc::SdpVideoFormat::VP8()) || format.IsSameCodec(webrtc::SdpVideoFormat::H264()))
        return makeUnique<LibWebRTCGStreamerVideoEncoder>(format);

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
    std::vector<webrtc::SdpVideoFormat> supportedFormats;

    auto& scanner = GStreamerRegistryScanner::singleton();
    if (scanner.isCodecSupported(GStreamerRegistryScanner::Configuration::Encoding, "vp8"_s))
        supportedFormats.push_back(webrtc::SdpVideoFormat::VP8());

    if (scanner.isCodecSupported(GStreamerRegistryScanner::Configuration::Encoding, "avc1"_s))
        supportedFormats.push_back(webrtc::SdpVideoFormat::H264());

    if (m_isSupportingVP9Profile0)
        supportedFormats.push_back(webrtc::SdpVideoFormat::VP9Profile0());
    if (m_isSupportingVP9Profile2)
        supportedFormats.push_back(webrtc::SdpVideoFormat::VP9Profile2());

    return supportedFormats;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(LIBWEBRTC) && USE(GSTREAMER)
