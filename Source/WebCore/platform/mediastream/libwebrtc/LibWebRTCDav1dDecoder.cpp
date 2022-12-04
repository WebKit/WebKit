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

#if USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "Logging.h"
#include <algorithm>
#include <wtf/FastMalloc.h>
#include <wtf/UniqueRef.h>

ALLOW_UNUSED_PARAMETERS_BEGIN
ALLOW_COMMA_BEGIN

#include <dav1d/dav1d.h>
#include <webrtc/api/scoped_refptr.h>
#include <webrtc/api/video/encoded_image.h>
#include <webrtc/api/video/i420_buffer.h>
#include <webrtc/common_video/include/video_frame_buffer_pool.h>
#include <webrtc/modules/video_coding/include/video_error_codes.h>
#include <webrtc/rtc_base/logging.h>

ALLOW_UNUSED_PARAMETERS_END
ALLOW_COMMA_END

namespace libyuv {
extern "C" int I420Copy(const uint8_t* src_y,
             int src_stride_y,
             const uint8_t* src_u,
             int src_stride_u,
             const uint8_t* src_v,
             int src_stride_v,
             uint8_t* dst_y,
             int dst_stride_y,
             uint8_t* dst_u,
             int dst_stride_u,
             uint8_t* dst_v,
             int dst_stride_v,
             int width,
             int height);
}

namespace WebCore {

class Dav1dDecoder final : public webrtc::VideoDecoder {
    WTF_MAKE_FAST_ALLOCATED;
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
    Dav1dData& Data() { return m_data; }

private:
    Dav1dData m_data;
};

class ScopedDav1dPicture {
public:
    ~ScopedDav1dPicture() { dav1d_picture_unref(&m_picture); }
    Dav1dPicture& Picture() { return m_picture; }

private:
    Dav1dPicture m_picture;
};

constexpr char kDav1dName[] = "dav1d";

// Calling `dav1d_data_wrap` requires a `free_callback` to be registered.
static void NullFreeCallback(const uint8_t*, void*)
{
}

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

int32_t Dav1dDecoder::Decode(const webrtc::EncodedImage& encodedImage, bool /*missingFrames*/, int64_t /*renderTimeMs*/)
{
    if (!m_context || m_decodeCompleteCallback == nullptr)
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

    ScopedDav1dData scopedDav1dData;
    Dav1dData& dav1dData = scopedDav1dData.Data();
    memset(&dav1dData, 0, sizeof(Dav1dData));
    auto* data = encodedImage.data();
    auto size = encodedImage.size();
    if (!data || !size) {
        RELEASE_LOG_ERROR(WebRTC, "Dav1dDecoder::Decode decoding failed as data is empty");
        return WEBRTC_VIDEO_CODEC_ERROR;
    }
    dav1d_data_wrap(&dav1dData, data, size, &NullFreeCallback, nullptr);

    if (int res = dav1d_send_data(m_context, &dav1dData)) {
        RELEASE_LOG_ERROR(WebRTC, "Dav1dDecoder::Decode decoding failed with error code %d", res);
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    ScopedDav1dPicture scopedDav1dPicture;
    Dav1dPicture& dav1dPicture = scopedDav1dPicture.Picture();
    memset(&dav1dPicture, 0, sizeof(Dav1dPicture));

    if (int res = dav1d_get_picture(m_context, &dav1dPicture)) {
        RELEASE_LOG_ERROR(WebRTC, "Dav1dDecoder::Decode getting picture failed with error code %d", res);
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // Only accept I420 pixel format and 8 bit depth.
    if (dav1dPicture.p.layout != DAV1D_PIXEL_LAYOUT_I420 || dav1dPicture.p.bpc != 8)
        return WEBRTC_VIDEO_CODEC_ERROR;

    auto buffer = m_bufferPool.CreateI420Buffer(dav1dPicture.p.w, dav1dPicture.p.h);
    if (!buffer.get()) {
        RELEASE_LOG_ERROR(WebRTC, "Dav1dDecoder::Decode failed to get frame from the buffer pool");
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    auto* yData = static_cast<uint8_t*>(dav1dPicture.data[0]);
    auto* uData = static_cast<uint8_t*>(dav1dPicture.data[1]);
    auto* vData = static_cast<uint8_t*>(dav1dPicture.data[2]);
    int yStride = dav1dPicture.stride[0];
    int uvStride = dav1dPicture.stride[1];
    libyuv::I420Copy(yData, yStride,
        uData, uvStride,
        vData, uvStride,
        buffer->MutableDataY(), buffer->StrideY(),
        buffer->MutableDataU(), buffer->StrideU(),
        buffer->MutableDataV(), buffer->StrideV(),
        dav1dPicture.p.w,
        dav1dPicture.p.h);

    webrtc::VideoFrame decodedFrame = webrtc::VideoFrame::Builder()
        .set_video_frame_buffer(buffer)
        .set_timestamp_rtp(encodedImage.Timestamp())
        .set_ntp_time_ms(encodedImage.ntp_time_ms_)
        .set_color_space(encodedImage.ColorSpace())
        .build();

    m_decodeCompleteCallback->Decoded(decodedFrame, absl::nullopt, absl::nullopt);
    return WEBRTC_VIDEO_CODEC_OK;
}

UniqueRef<webrtc::VideoDecoder> createLibWebRTCDav1dDecoder()
{
    return makeUniqueRef<Dav1dDecoder>();
}

} // namespace

#endif
