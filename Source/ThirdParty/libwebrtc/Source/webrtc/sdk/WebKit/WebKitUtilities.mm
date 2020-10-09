/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "native/src/objc_frame_buffer.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "libyuv/cpu_id.h"
#include "libyuv/row.h"
#include "Framework/Headers/WebRTC/RTCVideoFrameBuffer.h"

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

    uint8_t* dst_y = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0));
    int dst_stride_y = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0);

    uint8_t* dst_uv = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1));
    int dst_stride_uv = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1);

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

// MergeUVPlane_16 merges two separate U and V planes into a single, interleaved
// plane, while simultaneously scaling the output. Use '64' in the scale field to
// shift 10-bit data from the LSB of the 16-bit int to the MSB.
// NOTE: This method is based on libyuv::MergeUVPlane. If libyuv ever supports
// this operation directly, we should replace the below with it.
// NOTE: libyuv only has an opmitization of MergeUVRow_16 for AVX2 intrinsics.
// Calling this method on a CPU without AVX2 will fall back to a standard C
// implementation, and will probably be super slow. Add new MergeUVRow_16
// implementations as they become available in libyuv.
void MergeUVPlane_16(const uint16_t* src_u, int src_stride_u, const uint16_t* src_v, int src_stride_v, uint16_t* dst_uv, int dst_stride_uv, int width, int height, int scale) {
    void (*MergeUVRow_16)(const uint16_t* src_u, const uint16_t* src_v, uint16_t* dst_uv, int scale, int width) = libyuv::MergeUVRow_16_C;
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_uv = dst_uv + (height - 1) * dst_stride_uv;
        dst_stride_uv = -dst_stride_uv;
    }
    // Coalesce rows.
    if (src_stride_u == width && src_stride_v == width && dst_stride_uv == width * 2) {
        width *= height;
        height = 1;
        src_stride_u = src_stride_v = dst_stride_uv = 0;
    }
#if defined(HAS_MERGEUVROW_16_AVX2)
    if (libyuv::TestCpuFlag(libyuv::kCpuHasAVX2))
        MergeUVRow_16 = libyuv::MergeUVRow_16_AVX2;
#endif

    for (int y = 0; y < height; ++y) {
        // Merge a row of U and V into a row of UV.
        MergeUVRow_16(src_u, src_v, dst_uv, scale, width);
        src_u += src_stride_u / sizeof(uint16_t);
        src_v += src_stride_v / sizeof(uint16_t);
        dst_uv += dst_stride_uv / sizeof(uint16_t);
    }
}

// CopyPlane_16 will copy a plane of 16-bit data from one location to another,
// while simultaneously scaling the output. Use '64' in the scale field to
// shift 10-bit data from the LSB of a 16-bit int to the MSB.
// NOTE: This method is based on MergeUVPlane_16 above, but operates on a
// single plane, rater than interleaving two planes. If libyuv ever supports
// this operation directly, we should replace the below with it.
// NOTE: libyuv only has an opmitization of MergeUVRow_16 for AVX2 intrinsics.
// Calling this method on a CPU without AVX2 will fall back to a standard C
// implementation, and will probably be super slow. Add new MergeUVRow_16
// implementations as they become available in libyuv.
void CopyPlane_16(const uint16_t* src, int src_stride, uint16_t* dst, int dst_stride, int width, int height, int scale)
{
    void (*MultiplyRow_16)(const uint16_t* src_y, uint16_t* dst_y, int scale, int width) = libyuv::MultiplyRow_16_C;
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dst_stride;
        dst_stride = -dst_stride;
    }
    // Coalesce rows.
    if (src_stride == width && dst_stride == width * 2) {
        width *= height;
        height = 1;
        src_stride = dst_stride = 0;
    }
#if defined(HAS_MERGEUVROW_16_AVX2)
    if (libyuv::TestCpuFlag(libyuv::kCpuHasAVX2))
        MultiplyRow_16 = libyuv::MultiplyRow_16_AVX2;
#endif

    for (int y = 0; y < height; ++y) {
        MultiplyRow_16(src, dst, scale, width);
        src += src_stride / sizeof(uint16_t);
        dst += dst_stride / sizeof(uint16_t);
    }
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
    auto src_stride_y = frame->StrideY() * sizeof(uint16_t);
    auto src_width_uv = frame->ChromaWidth();
    auto src_height_uv = frame->ChromaHeight();
    auto src_stride_u = frame->StrideU() * sizeof(uint16_t);
    auto src_stride_v = frame->StrideV() * sizeof(uint16_t);

    auto* dst_y = reinterpret_cast<uint16_t*>(CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0));
    auto dst_stride_y = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0);
    auto dst_width_y = CVPixelBufferGetWidthOfPlane(pixel_buffer, 0);
    auto dst_height_y = CVPixelBufferGetHeightOfPlane(pixel_buffer, 0);

    auto* dst_uv = reinterpret_cast<uint16_t*>(CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1));
    auto dst_stride_uv = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1);
    auto dst_width_uv = CVPixelBufferGetWidthOfPlane(pixel_buffer, 1);
    auto dst_height_uv = CVPixelBufferGetHeightOfPlane(pixel_buffer, 1);

    if (src_width_y != dst_width_y
        || src_height_y != dst_height_y
        || src_width_uv != dst_width_uv
        || src_height_uv != dst_height_uv)
        return false;

    CopyPlane_16(src_y, src_stride_y, dst_y, dst_stride_y, dst_width_y, dst_height_y, 64);
    MergeUVPlane_16(src_u, src_stride_u, src_v, src_stride_v, dst_uv, dst_stride_uv, dst_width_uv, dst_height_uv, 64);

    CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);
    return true;
}

CVPixelBufferRef pixelBufferFromFrame(const VideoFrame& frame, const std::function<CVPixelBufferRef(size_t, size_t)>& makePixelBuffer)
{
    if (frame.video_frame_buffer()->type() != VideoFrameBuffer::Type::kNative) {
        auto pixelBuffer = makePixelBuffer(frame.video_frame_buffer()->width(), frame.video_frame_buffer()->height());
        if (!pixelBuffer)
            return nullptr;

        if (frame.video_frame_buffer()->type() == VideoFrameBuffer::Type::kI420)
            CopyVideoFrameToPixelBuffer(frame.video_frame_buffer()->GetI420(), pixelBuffer);
        else if (frame.video_frame_buffer()->type() == VideoFrameBuffer::Type::kI010)
            CopyVideoFrameToPixelBuffer(frame.video_frame_buffer()->GetI010(), pixelBuffer);
        return pixelBuffer;
    }

    auto *frameBuffer = static_cast<ObjCFrameBuffer*>(frame.video_frame_buffer().get())->wrapped_frame_buffer();
    if (![frameBuffer isKindOfClass:[RTCCVPixelBuffer class]])
        return nullptr;

    auto *rtcPixelBuffer = (RTCCVPixelBuffer *)frameBuffer;
    return rtcPixelBuffer.pixelBuffer;
}

CVPixelBufferPoolRef createPixelBufferPool(size_t width, size_t height)
{
    const OSType videoCaptureFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
    auto pixelAttributes = @{
        (__bridge NSString *)kCVPixelBufferWidthKey: @(width),
        (__bridge NSString *)kCVPixelBufferHeightKey: @(height),
        (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(videoCaptureFormat),
        (__bridge NSString *)kCVPixelBufferCGImageCompatibilityKey: @NO,
#if defined(WEBRTC_IOS)
        (__bridge NSString *)kCVPixelFormatOpenGLESCompatibility : @YES,
#else
        (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @YES,
#endif
        (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ }
    };

    CVPixelBufferPoolRef pool = nullptr;
    auto status = CVPixelBufferPoolCreate(kCFAllocatorDefault, nullptr, (__bridge CFDictionaryRef)pixelAttributes, &pool);

    if (status != kCVReturnSuccess)
        return nullptr;

    return pool;
}

}
