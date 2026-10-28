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

#include "GStreamerRegistryScanner.h"
#include "GStreamerVideoCommon.h"
#include "GStreamerVideoFrameLibWebRTC.h"
#include "IntSize.h"
#include "LibWebRTCUtils.h"
#include "webrtc/modules/video_coding/codecs/vp8/libvpx_vp8_decoder.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"
#include <wtf/TZoneMallocInlines.h>

GST_DEBUG_CATEGORY(webkit_webrtcdec_debug);
#define GST_CAT_DEFAULT webkit_webrtcdec_debug

#if GST_CHECK_VERSION(1, 22, 0)
#define DEC_DEBUG(...) GST_DEBUG_ID(m_id.data(), __VA_ARGS__)
#define DEC_ERROR(...) GST_ERROR_ID(m_id.data(), __VA_ARGS__)
#else
#define DEC_DEBUG(...) GST_DEBUG(__VA_ARGS__)
#define DEC_ERROR(...) GST_ERROR(__VA_ARGS__)
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerVideoDecoderFactory);

class PassthroughVideoDecoder : public webrtc::VideoDecoder {
public:
    PassthroughVideoDecoder() = default;

    virtual bool updateCapsFromImageSize(IntSize&& newSize)
    {
        if (newSize == m_size)
            return false;

        DEC_DEBUG("Size changed to %dx%d", newSize.width(), newSize.height());
        m_size = WTF::move(newSize);
        m_caps = adoptGRef(gst_caps_new_simple(mediaType(), "width", G_TYPE_INT, m_size.width(), "height", G_TYPE_INT, m_size.height(), nullptr));
        return true;
    }

    virtual ASCIILiteral mediaType() = 0;
    virtual webrtc::VideoCodecType codecType() = 0;
    virtual ASCIILiteral name() = 0;
    virtual webrtc::SdpVideoFormat sdpVideoFormat() = 0;

    bool Configure(const webrtc::VideoDecoder::Settings&) override
    {
        static Atomic<uint64_t> uniqueId = 0;
        m_id = makeString("webkit-webrtc-video-dec-"_s, uniqueId.exchangeAdd(1)).ascii();
        return true;
    }

    void addDecoderIfSupported(std::vector<webrtc::SdpVideoFormat>& codecList)
    {
        if (!GStreamerRegistryScanner::singleton().isCodecSupported(GStreamerRegistryScanner::Configuration::Decoding, mediaType()))
            return;

        auto formats = configureSupportedDecoder();
        codecList.insert(codecList.end(), formats.begin(), formats.end());
    }

protected:
    GRefPtr<GstCaps> m_caps;

private:
    const char* ImplementationName() const override { return "Passthrough"; }
    int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) override
    {
        m_imageReadyCb = callback;
        return WEBRTC_VIDEO_CODEC_OK;
    }
    virtual std::vector<webrtc::SdpVideoFormat> configureSupportedDecoder() { return { sdpVideoFormat() }; }

    int32_t Decode(const webrtc::EncodedImage& inputImage, int64_t) override
    {
        if (m_needsKeyframe) {
            if (inputImage._frameType != webrtc::VideoFrameType::kVideoFrameKey) {
                DEC_ERROR("Waiting for keyframe but got a delta unit... asking for keyframe");
                return WEBRTC_VIDEO_CODEC_OK_REQUEST_KEYFRAME;
            }
            m_needsKeyframe = false;
        }

        if (inputImage._encodedWidth && inputImage._encodedHeight)
            updateCapsFromImageSize({ static_cast<int>(inputImage._encodedWidth), static_cast<int>(inputImage._encodedHeight) });

        if (!m_caps) [[unlikely]] {
            DEC_ERROR("Encoded image caps not set");
            ASSERT_NOT_REACHED();
            return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
        }

        auto encodedData = inputImage.GetEncodedData();
        if (!encodedData) {
            DEC_ERROR("Encoded image has no data buffer");
            return WEBRTC_VIDEO_CODEC_ERROR;
        }
        encodedData->AddRef();
        auto data = const_cast<uint8_t*>(encodedData->data());
        auto dataSize = encodedData->size();
        GRefPtr buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, data, dataSize, 0, dataSize, static_cast<gpointer>(encodedData.get()), [](gpointer data) {
            static_cast<webrtc::EncodedImageBufferInterface*>(data)->Release();
        }));

        GRefPtr sample = adoptGRef(gst_sample_new(buffer.get(), m_caps.get(), nullptr, nullptr));
        auto frame = convertGStreamerSampleToLibWebRTCVideoFrame(WTF::move(sample), inputImage);

        m_imageReadyCb->Decoded(frame);
        return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t Release() final { return WEBRTC_VIDEO_CODEC_OK; }

    ASCIICString m_id;
    bool m_needsKeyframe { true };
    IntSize m_size;
    webrtc::DecodedImageCallback* m_imageReadyCb { nullptr };
};

class H264Decoder : public PassthroughVideoDecoder {
public:
    bool updateCapsFromImageSize(IntSize&& newSize) final
    {
        bool result = PassthroughVideoDecoder::updateCapsFromImageSize(WTF::move(newSize));
        if (result)
            gst_caps_set_simple(m_caps.get(), "alignment", G_TYPE_STRING, "au", "stream-format", G_TYPE_STRING, "byte-stream", nullptr);
        return result;
    }

private:
    bool Configure(const webrtc::VideoDecoder::Settings& codecSettings) final
    {
        if (codecSettings.codec_type() != webrtc::kVideoCodecH264)
            return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;

        return PassthroughVideoDecoder::Configure(codecSettings);
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

class VP8Decoder : public PassthroughVideoDecoder {
public:
    static std::unique_ptr<webrtc::VideoDecoder> Create(const webrtc::Environment& environment)
    {
        auto factory = GStreamerRegistryScanner::singleton().isCodecSupported(GStreamerRegistryScanner::Configuration::Decoding, "video/x-vp8"_s, false).factory;
        if (!factory) {
            GST_INFO("No GStreamer VP8 decoder found, falling back to LibWebRTC for VP8 decoding.");
            return std::unique_ptr<webrtc::VideoDecoder>(new webrtc::LibvpxVp8Decoder(environment));
        }

        auto factoryName = UTF8CStringView::unsafeFromUTF8(GST_OBJECT_NAME(GST_OBJECT(factory.get())));
        if (equal(factoryName.span(), "vp8dec"_s) || equal(factoryName.span(), "vp8alphadecodebin"_s)) {
            GST_INFO("Our best GStreamer VP8 decoder is vp8dec, better use the one from LibWebRTC");
            return std::unique_ptr<webrtc::VideoDecoder>(new webrtc::LibvpxVp8Decoder(environment));
        }

        return std::unique_ptr<webrtc::VideoDecoder>(new VP8Decoder());
    }

private:
    ASCIILiteral mediaType() final { return "video/x-vp8"_s; }
    ASCIILiteral name() final { return "vp8"_s; }
    webrtc::SdpVideoFormat sdpVideoFormat() final { return webrtc::SdpVideoFormat::VP8(); }
    webrtc::VideoCodecType codecType() final { return webrtc::kVideoCodecVP8; }
};

class VP9Decoder : public PassthroughVideoDecoder {
public:
    static std::unique_ptr<webrtc::VideoDecoder> Create()
    {
        return std::unique_ptr<webrtc::VideoDecoder>(new VP9Decoder());
    }

    VP9Decoder(bool isSupportingVP9Profile0 = true, bool isSupportingVP9Profile2 = true)
        : m_isSupportingVP9Profile0(isSupportingVP9Profile0)
        , m_isSupportingVP9Profile2(isSupportingVP9Profile2)
    {
    }

private:
    ASCIILiteral mediaType() final { return "video/x-vp9"_s; }
    ASCIILiteral name() final { return "vp9"_s; }
    webrtc::SdpVideoFormat sdpVideoFormat() final { return webrtc::SdpVideoFormat::VP9Profile0(); }

    webrtc::VideoCodecType codecType() final { return webrtc::kVideoCodecVP9; }
    std::vector<webrtc::SdpVideoFormat> configureSupportedDecoder() final
    {
        std::vector<webrtc::SdpVideoFormat> formats;
        if (m_isSupportingVP9Profile0)
            formats.push_back(webrtc::SdpVideoFormat::VP9Profile0());
        if (m_isSupportingVP9Profile2)
            formats.push_back(webrtc::SdpVideoFormat::VP9Profile2());
        return formats;
    }

    bool m_isSupportingVP9Profile0;
    bool m_isSupportingVP9Profile2;
};

std::unique_ptr<webrtc::VideoDecoder> GStreamerVideoDecoderFactory::Create(const webrtc::Environment& environment, const webrtc::SdpVideoFormat& format)
{
    auto formatName = fromStdString(format.name);
    // Check format ignoring ascii case, see also: webrtc/video-lowercase-media-subtype.html.
    if (WTF::equalLettersIgnoringASCIICase(formatName, "h264"_s))
        return std::unique_ptr<webrtc::VideoDecoder>(new H264Decoder());
    if (WTF::equalLettersIgnoringASCIICase(formatName, "vp8"_s))
        return VP8Decoder::Create(environment);
    if (WTF::equalLettersIgnoringASCIICase(formatName, "vp9"_s))
        return VP9Decoder::Create();

    GST_ERROR("Could not create decoder for %s", formatName.ascii().data());
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
