/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include "WebKitUtilities.h"

#include "api/video/video_frame.h"
#include "native/src/objc_frame_buffer.h"
#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "libyuv/cpu_id.h"
#include "libyuv/row.h"
#include "Framework/Headers/WebRTC/RTCVideoFrameBuffer.h"
#include "third_party/libyuv/include/libyuv.h"

namespace webrtc {

void setApplicationStatus(bool isActive)
{
/*
    if (isActive)
        [[RTCUIApplicationStatusObserver sharedInstance] setActive];
    else
        [[RTCUIApplicationStatusObserver sharedInstance] setInactive];
 */
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer> pixelBufferToFrame(CVPixelBufferRef pixelBuffer)
{
    RTCCVPixelBuffer *frameBuffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBuffer];
    return new rtc::RefCountedObject<ObjCFrameBuffer>(frameBuffer);
}

static bool CopyVideoFrameToPixelBuffer(const webrtc::I420BufferInterface* frame, CVPixelBufferRef pixel_buffer) {
    RTC_DCHECK(pixel_buffer);
    RTC_DCHECK(CVPixelBufferGetPixelFormatType(pixel_buffer) == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange || CVPixelBufferGetPixelFormatType(pixel_buffer) == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
    RTC_DCHECK_EQ(CVPixelBufferGetHeightOfPlane(pixel_buffer, 0), static_cast<size_t>(frame->height()));
    RTC_DCHECK_EQ(CVPixelBufferGetWidthOfPlane(pixel_buffer, 0), static_cast<size_t>(frame->width()));

    if (CVPixelBufferLockBaseAddress(pixel_buffer, 0) != kCVReturnSuccess)
        return false;

    auto src_width_y = frame->width();
    auto src_height_y = frame->height();
    auto src_width_uv = frame->ChromaWidth();
    auto src_height_uv = frame->ChromaHeight();

    uint8_t* dst_y = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0));
    int dst_stride_y = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0);
    auto dst_width_y = CVPixelBufferGetWidthOfPlane(pixel_buffer, 0);
    auto dst_height_y = CVPixelBufferGetHeightOfPlane(pixel_buffer, 0);

    uint8_t* dst_uv = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1));
    int dst_stride_uv = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1);
    auto dst_width_uv = CVPixelBufferGetWidthOfPlane(pixel_buffer, 1);
    auto dst_height_uv = CVPixelBufferGetHeightOfPlane(pixel_buffer, 1);

    if (src_width_y != dst_width_y
        || src_height_y != dst_height_y
        || src_width_uv != dst_width_uv
        || src_height_uv != dst_height_uv)
        return false;

    int result = libyuv::I420ToNV12(
        frame->DataY(), frame->StrideY(),
        frame->DataU(), frame->StrideU(),
        frame->DataV(), frame->StrideV(),
        dst_y, dst_stride_y, dst_uv, dst_stride_uv,
        frame->width(), frame->height());

    CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);

    if (result)
        return false;

    return true;
}

static bool CopyVideoFrameToPixelBuffer(const webrtc::I010BufferInterface* frame, CVPixelBufferRef pixel_buffer)
{
    RTC_DCHECK(pixel_buffer);
    RTC_DCHECK(CVPixelBufferGetPixelFormatType(pixel_buffer) == kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange || CVPixelBufferGetPixelFormatType(pixel_buffer) == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange);
    RTC_DCHECK_EQ(CVPixelBufferGetHeightOfPlane(pixel_buffer, 0), static_cast<size_t>(frame->height()));
    RTC_DCHECK_EQ(CVPixelBufferGetWidthOfPlane(pixel_buffer, 0), static_cast<size_t>(frame->width()));

    if (CVPixelBufferLockBaseAddress(pixel_buffer, 0) != kCVReturnSuccess)
        return false;

    auto src_y = const_cast<uint16_t*>(frame->DataY());
    auto src_u = const_cast<uint16_t*>(frame->DataU());
    auto src_v = const_cast<uint16_t*>(frame->DataV());
    auto src_width_y = frame->width();
    auto src_height_y = frame->height();
    auto src_stride_y = frame->StrideY();
    auto src_width_uv = frame->ChromaWidth();
    auto src_height_uv = frame->ChromaHeight();
    auto src_stride_u = frame->StrideU();
    auto src_stride_v = frame->StrideV();

    auto* dst_y = reinterpret_cast<uint16_t*>(CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0));
    auto dst_stride_y = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0) / 2;
    auto dst_width_y = CVPixelBufferGetWidthOfPlane(pixel_buffer, 0);
    auto dst_height_y = CVPixelBufferGetHeightOfPlane(pixel_buffer, 0);

    auto* dst_uv = reinterpret_cast<uint16_t*>(CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1));
    auto dst_stride_uv = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1) / 2;
    auto dst_width_uv = CVPixelBufferGetWidthOfPlane(pixel_buffer, 1);
    auto dst_height_uv = CVPixelBufferGetHeightOfPlane(pixel_buffer, 1);

    if (src_width_y != dst_width_y
        || src_height_y != dst_height_y
        || src_width_uv != dst_width_uv
        || src_height_uv != dst_height_uv)
        return false;

    libyuv::I010ToP010(src_y, src_stride_y,
                       src_u, src_stride_u,
                       src_v, src_stride_v,
                       dst_y, dst_stride_y, dst_uv, dst_stride_uv,
                       src_width_y, src_height_y);

    CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);
    return true;
}

CVPixelBufferRef createPixelBufferFromFrame(const VideoFrame& frame, const std::function<CVPixelBufferRef(size_t, size_t, BufferType)>& createPixelBuffer)
{
    auto buffer = frame.video_frame_buffer();
    if (buffer->type() != VideoFrameBuffer::Type::kNative) {
        auto type = buffer->type();
        if (type != VideoFrameBuffer::Type::kI420 && type != VideoFrameBuffer::Type::kI010) {
            RTC_LOG(WARNING) << "Video frame buffer type is not expected.";
            return nullptr;
        }

        auto pixelBuffer = createPixelBuffer(buffer->width(), buffer->height(), type == VideoFrameBuffer::Type::kI420 ? BufferType::I420 : BufferType::I010);
        if (!pixelBuffer) {
            RTC_LOG(WARNING) << "Pixel buffer creation failed.";
            return nullptr;
        }

        if (type == VideoFrameBuffer::Type::kI420)
            CopyVideoFrameToPixelBuffer(buffer->GetI420(), pixelBuffer);
        else
            CopyVideoFrameToPixelBuffer(buffer->GetI010(), pixelBuffer);
        return pixelBuffer;
    }

    auto *frameBuffer = static_cast<ObjCFrameBuffer*>(buffer.get())->wrapped_frame_buffer();
    if (![frameBuffer isKindOfClass:[RTCCVPixelBuffer class]])
        return nullptr;

    auto *rtcPixelBuffer = (RTCCVPixelBuffer *)frameBuffer;
    return CVPixelBufferRetain(rtcPixelBuffer.pixelBuffer);
}

}
