/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LibWebRTCDav1dDecoder.h"

#if USE(LIBWEBRTC) && ENABLE(AV1)

#include "Dav1dSPI.h"
#include "LibWebRTCMacros.h"
#include "Logging.h"
#include "PlanarImageConverter.h"
#include <algorithm>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UniqueRef.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN

#include <webrtc/api/scoped_refptr.h>
#include <webrtc/api/video/encoded_image.h>
#include <webrtc/api/video/i010_buffer.h>
#include <webrtc/api/video/i210_buffer.h>
#include <webrtc/api/video/i410_buffer.h>
#include <webrtc/api/video/i420_buffer.h>
#include <webrtc/api/video/i422_buffer.h>
#include <webrtc/api/video/i444_buffer.h>
#include <webrtc/common_video/include/video_frame_buffer_pool.h>
#include <webrtc/modules/video_coding/include/video_error_codes.h>
#include <webrtc/rtc_base/logging.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

class Dav1dDecoder final : public webrtc::VideoDecoder {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(Dav1dDecoder);
public:
    Dav1dDecoder();
    ~Dav1dDecoder();

private:
    bool Configure(const Settings&) final;
    int32_t Decode(const webrtc::EncodedImage&, bool missingFrames, int64_t renderTimeMs) final;
    int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback*) final;
    int32_t Release() final;
    DecoderInfo GetDecoderInfo() const final;
    const char* ImplementationName() const final;

    webrtc::VideoFrameBufferPool m_bufferPool;
    Dav1dContext* m_context { nullptr };
    webrtc::DecodedImageCallback* m_decodeCompleteCallback { nullptr };
};

class ScopedDav1dData {
public:
    ~ScopedDav1dData() { dav1d_data_unref(&m_data); }
    NODELETE Dav1dData& Data() { return m_data; }

private:
    Dav1dData m_data;
};

class ScopedDav1dPicture {
public:
    ~ScopedDav1dPicture() { dav1d_picture_unref(&m_picture); }
    NODELETE Dav1dPicture& Picture() { return m_picture; }

private:
    Dav1dPicture m_picture;
};

constexpr char kDav1dName[] = "dav1d";

Dav1dDecoder::Dav1dDecoder()
    : m_bufferPool(false, 150)
{
}

Dav1dDecoder::~Dav1dDecoder()
{
    Release();
}

bool Dav1dDecoder::Configure(const Settings& settings)
{
    Dav1dSettings s;
    dav1d_default_settings(&s);

    // FIXME: We might want to use a more efficient configuration, at which point we might need to revise Dav1dDecoder::Decode as decoding might become async.
    s.n_threads = std::max(2, settings.number_of_cores());
    s.max_frame_delay = 1;
    s.all_layers = 0;
    s.operating_point = 31;

    return !dav1d_open(&m_context, &s);
}

int32_t Dav1dDecoder::RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback)
{
    m_decodeCompleteCallback = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t Dav1dDecoder::Release()
{
    dav1d_close(&m_context);
    if (m_context != nullptr)
        return WEBRTC_VIDEO_CODEC_MEMORY;

    m_bufferPool.Release();
    return WEBRTC_VIDEO_CODEC_OK;
}

Dav1dDecoder::DecoderInfo Dav1dDecoder::GetDecoderInfo() const
{
    DecoderInfo info;
    info.implementation_name = kDav1dName;
    info.is_hardware_accelerated = false;
    return info;
}

const char* Dav1dDecoder::ImplementationName() const
{
    return kDav1dName;
}

template<typename Sample>
static std::optional<PlanarImageConverter::DestinationPlane> bufferPlane(Sample* data, int stride, int width, int height, PlanarImageConverter::SampleFormat format)
{
    return PlanarImageConverter::makePlane(unsafeMakeSpan(data, (height - 1) * stride + width), 0, stride, width, height, format);
}

static std::optional<PlanarImageConverter::SourcePlane> picturePlane(const Dav1dPicture& picture, size_t index, int width, int height, PlanarImageConverter::SampleFormat format)
{
    // dav1d strides count bytes, and both chroma planes share the second one.
    size_t stride = std::span { picture.stride }[index ? 1 : 0];
    auto* data = static_cast<const uint8_t*>(std::span { picture.data }[index]);
    return PlanarImageConverter::makePlane(unsafeMakeSpan(data, (height - 1) * stride + width * format.sampleSize()), 0, stride, width, height, format);
}

static void fillPlane(const PlanarImageConverter::DestinationPlane& plane, uint16_t value)
{
    for (size_t y = 0; y < plane.height; ++y) {
        auto row = plane.row(y);
        if (plane.format.sampleSize() == 1)
            memsetSpan(row, static_cast<uint8_t>(value));
        else
            std::ranges::fill(spanReinterpretCast<uint16_t>(row), value);
    }
}

template<typename Buffer>
static webrtc::scoped_refptr<webrtc::VideoFrameBuffer> copyPicture(webrtc::scoped_refptr<Buffer>&& buffer, const Dav1dPicture& picture, PlanarImageConverter::SampleFormat format)
{
    if (!buffer)
        return nullptr;

    int width = buffer->width();
    int height = buffer->height();
    int chromaWidth = buffer->ChromaWidth();
    int chromaHeight = buffer->ChromaHeight();
    auto yPlane = bufferPlane(buffer->MutableDataY(), buffer->StrideY(), width, height, format);
    auto uPlane = bufferPlane(buffer->MutableDataU(), buffer->StrideU(), chromaWidth, chromaHeight, format);
    auto vPlane = bufferPlane(buffer->MutableDataV(), buffer->StrideV(), chromaWidth, chromaHeight, format);
    auto sourceY = picturePlane(picture, 0, width, height, format);
    if (!yPlane || !uPlane || !vPlane || !sourceY || !PlanarImageConverter::copyPlane(*sourceY, *yPlane))
        return nullptr;

    // Monochrome pictures have no chroma planes.
    if (picture.p.layout == DAV1D_PIXEL_LAYOUT_I400) {
        uint16_t neutral = 1 << (format.bitDepth - 1);
        fillPlane(*uPlane, neutral);
        fillPlane(*vPlane, neutral);
        return buffer;
    }

    auto sourceU = picturePlane(picture, 1, chromaWidth, chromaHeight, format);
    auto sourceV = picturePlane(picture, 2, chromaWidth, chromaHeight, format);
    if (!sourceU || !sourceV
        || !PlanarImageConverter::copyPlane(*sourceU, *uPlane, PlanarImageConverter::RangeConversion::None, PlanarImageConverter::Component::Chroma)
        || !PlanarImageConverter::copyPlane(*sourceV, *vPlane, PlanarImageConverter::RangeConversion::None, PlanarImageConverter::Component::Chroma))
        return nullptr;
    return buffer;
}

int32_t Dav1dDecoder::Decode(const webrtc::EncodedImage& encodedImage, bool /*missingFrames*/, int64_t /*renderTimeMs*/)
{
    if (!m_context || m_decodeCompleteCallback == nullptr)
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

    ScopedDav1dData scopedDav1dData;
    Dav1dData& dav1dData = scopedDav1dData.Data();
    zeroBytes(dav1dData);
    auto* data = encodedImage.data();
    auto size = encodedImage.size();
    if (!data || !size) {
        RELEASE_LOG_ERROR(WebRTC, "Dav1dDecoder::Decode decoding failed as data is empty");
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    auto* bitstreamBuffer =
        encodedImage.GetEncodedData().release();
    int error = dav1d_data_wrap(
        &dav1dData, encodedImage.data(), encodedImage.size(),
        /* free callback= */
        [](const uint8_t* /* buffer */, void* userData) {
            auto* buffer = static_cast<webrtc::EncodedImageBufferInterface*>(userData);
            buffer->Release();
        },
        /*user_data=*/bitstreamBuffer);

    if (error) {
        bitstreamBuffer->Release();
        RELEASE_LOG_ERROR(WebRTC, "Dav1dDecoder::Decode dav1d_data_wrap failed with error code %d", error);
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (int res = dav1d_send_data(m_context, &dav1dData)) {
        RELEASE_LOG_ERROR(WebRTC, "Dav1dDecoder::Decode decoding failed with error code %d", res);
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    ScopedDav1dPicture scopedDav1dPicture;
    Dav1dPicture& dav1dPicture = scopedDav1dPicture.Picture();
    zeroBytes(dav1dPicture);

    if (int res = dav1d_get_picture(m_context, &dav1dPicture)) {
        RELEASE_LOG_ERROR(WebRTC, "Dav1dDecoder::Decode getting picture failed with error code %d", res);
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // libwebrtc has no buffer type for 12-bit samples.
    if (dav1dPicture.p.bpc != 8 && dav1dPicture.p.bpc != 10) {
        RELEASE_LOG_ERROR(WebRTC, "Dav1dDecoder::Decode unsupported bit depth %d", dav1dPicture.p.bpc);
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    bool isTenBit = dav1dPicture.p.bpc == 10;
    auto format = isTenBit ? PlanarImageConverter::tenBitLSB : PlanarImageConverter::eightBit;
    int width = dav1dPicture.p.w;
    int height = dav1dPicture.p.h;
    auto buffer = [&] -> webrtc::scoped_refptr<webrtc::VideoFrameBuffer> {
        switch (dav1dPicture.p.layout) {
        case DAV1D_PIXEL_LAYOUT_I400:
        case DAV1D_PIXEL_LAYOUT_I420:
            if (isTenBit)
                return copyPicture(m_bufferPool.CreateI010Buffer(width, height), dav1dPicture, format);
            return copyPicture(m_bufferPool.CreateI420Buffer(width, height), dav1dPicture, format);
        case DAV1D_PIXEL_LAYOUT_I422:
            if (isTenBit)
                return copyPicture(m_bufferPool.CreateI210Buffer(width, height), dav1dPicture, format);
            return copyPicture(m_bufferPool.CreateI422Buffer(width, height), dav1dPicture, format);
        case DAV1D_PIXEL_LAYOUT_I444:
            if (isTenBit)
                return copyPicture(m_bufferPool.CreateI410Buffer(width, height), dav1dPicture, format);
            return copyPicture(m_bufferPool.CreateI444Buffer(width, height), dav1dPicture, format);
        }
        return nullptr;
    }();
    if (!buffer) {
        RELEASE_LOG_ERROR(WebRTC, "Dav1dDecoder::Decode failed to copy the decoded picture");
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    webrtc::VideoFrame decodedFrame = webrtc::VideoFrame::Builder()
        .set_video_frame_buffer(buffer)
        .set_timestamp_rtp(encodedImage.RtpTimestamp())
        .set_ntp_time_ms(encodedImage.ntp_time_ms_)
        .set_color_space(encodedImage.ColorSpace())
        .build();

    m_decodeCompleteCallback->Decoded(decodedFrame, std::nullopt, std::nullopt);
    return WEBRTC_VIDEO_CODEC_OK;
}

UniqueRef<webrtc::VideoDecoder> createLibWebRTCDav1dDecoder()
{
    return makeUniqueRef<Dav1dDecoder>();
}

} // namespace

#endif
