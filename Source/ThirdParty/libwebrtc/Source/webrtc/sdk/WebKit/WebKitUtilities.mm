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

#include "api/make_ref_counted.h"
#include "api/video/i420_buffer.h"
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
    return rtc::make_ref_counted<ObjCFrameBuffer>(frameBuffer);
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer> toWebRTCVideoFrameBuffer(void* pointer, GetBufferCallback getBufferCallback, ReleaseBufferCallback releaseBufferCallback, int width, int height)
{
    return rtc::make_ref_counted<ObjCFrameBuffer>(ObjCFrameBuffer::BufferProvider { pointer, getBufferCallback, releaseBufferCallback }, width, height);
}

void* videoFrameBufferProvider(const VideoFrame& frame)
{
    auto buffer = frame.video_frame_buffer();
    if (buffer->type() != VideoFrameBuffer::Type::kNative)
        return nullptr;

    return static_cast<ObjCFrameBuffer*>(buffer.get())->frame_buffer_provider();
}

static bool copyI420BufferToPixelBuffer(CVPixelBufferRef pixelBuffer, const uint8_t* buffer, size_t length, size_t width, size_t height, I420BufferLayout layout)
{
    auto sourceWidthY = width;
    auto sourceHeightY = height;
    auto sourceWidthUV = (width + 1) / 2;
    auto sourceHeightUV = (height + 1) / 2;

    auto destinationWidthY = CVPixelBufferGetWidthOfPlane(pixelBuffer, 0);
    auto destinationHeightY = CVPixelBufferGetHeightOfPlane(pixelBuffer, 0);

    auto destinationWidthUV = CVPixelBufferGetWidthOfPlane(pixelBuffer, 1);
    auto destinationHeightUV = CVPixelBufferGetHeightOfPlane(pixelBuffer, 1);

    if (sourceWidthY != destinationWidthY
        || sourceHeightY != destinationHeightY
        || sourceWidthUV != destinationWidthUV
        || sourceHeightUV != destinationHeightUV)
        return false;

    uint8_t* destinationY = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
    int destinationStrideY = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
    uint8_t* destinationUV = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));
    int destinationStrideUV = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);

    auto* sourceY = buffer + layout.offsetY;
    int sourceStrideY = layout.strideY;
    auto* sourceU = buffer + layout.offsetU;
    int sourceStrideU = layout.strideU;
    auto* sourceV = buffer + layout.offsetV;
    int sourceStrideV = layout.strideV;

    return !libyuv::I420ToNV12(
        sourceY, sourceStrideY,
        sourceU, sourceStrideU,
        sourceV, sourceStrideV,
        destinationY, destinationStrideY, destinationUV, destinationStrideUV,
        width, height);

}

CVPixelBufferRef pixelBufferFromI420Buffer(const uint8_t* buffer, size_t length, size_t width, size_t height, I420BufferLayout layout)
{
    CVPixelBufferRef pixelBuffer = nullptr;

    auto status = CVPixelBufferCreate(kCFAllocatorDefault, width, height, kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, nullptr, &pixelBuffer);
    if (status != noErr || !pixelBuffer)
        return nullptr;

    if (CVPixelBufferLockBaseAddress(pixelBuffer, 0) != kCVReturnSuccess)
        return nullptr;

    bool result = copyI420BufferToPixelBuffer(pixelBuffer, buffer, length, width, height, layout);
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

    if (!result) {
        CFRelease(pixelBuffer);
        return nullptr;
    }
    return pixelBuffer;
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
    return createPixelBufferFromFrameBuffer(*frame.video_frame_buffer(), createPixelBuffer);
}

CVPixelBufferRef createPixelBufferFromFrameBuffer(VideoFrameBuffer& buffer, const std::function<CVPixelBufferRef(size_t, size_t, BufferType)>& createPixelBuffer)
{
    if (buffer.type() != VideoFrameBuffer::Type::kNative) {
        auto type = buffer.type();
        if (type != VideoFrameBuffer::Type::kI420 && type != VideoFrameBuffer::Type::kI010) {
            RTC_LOG(LS_WARNING) << "Video frame buffer type is not expected.";
            return nullptr;
        }

        auto pixelBuffer = createPixelBuffer(buffer.width(), buffer.height(), type == VideoFrameBuffer::Type::kI420 ? BufferType::I420 : BufferType::I010);
        if (!pixelBuffer) {
            RTC_LOG(LS_WARNING) << "Pixel buffer creation failed.";
            return nullptr;
        }

        if (type == VideoFrameBuffer::Type::kI420)
            CopyVideoFrameToPixelBuffer(buffer.GetI420(), pixelBuffer);
        else
            CopyVideoFrameToPixelBuffer(buffer.GetI010(), pixelBuffer);
        return pixelBuffer;
    }

    auto *frameBuffer = static_cast<ObjCFrameBuffer*>(&buffer)->wrapped_frame_buffer();
    if (![frameBuffer isKindOfClass:[RTCCVPixelBuffer class]])
        return nullptr;

    auto *rtcPixelBuffer = (RTCCVPixelBuffer *)frameBuffer;
    return CVPixelBufferRetain(rtcPixelBuffer.pixelBuffer);
}

CVPixelBufferRef pixelBufferFromFrame(const VideoFrame& frame)
{
    auto buffer = frame.video_frame_buffer();
    if (buffer->type() != VideoFrameBuffer::Type::kNative)
        return nullptr;

    auto *frameBuffer = static_cast<ObjCFrameBuffer*>(buffer.get())->wrapped_frame_buffer();
    if (![frameBuffer isKindOfClass:[RTCCVPixelBuffer class]])
        return nullptr;

    auto *rtcPixelBuffer = (RTCCVPixelBuffer *)frameBuffer;
    return CVPixelBufferRetain(rtcPixelBuffer.pixelBuffer);
}

bool copyVideoFrameBuffer(VideoFrameBuffer& buffer, uint8_t* data)
{
    if (buffer.type() == VideoFrameBuffer::Type::kNative)
        return false;

    auto type = buffer.type();
    if (type == VideoFrameBuffer::Type::kI420) {
        auto* i420Frame = buffer.GetI420();
        auto* dataY = data;
        auto strideY = i420Frame->width();
        auto strideUV = i420Frame->width();
        auto* dataUV = data + (i420Frame->width() * i420Frame->height());
        return !libyuv::I420ToNV12(i420Frame->DataY(), i420Frame->StrideY(),
                                   i420Frame->DataU(), i420Frame->StrideU(),
                                   i420Frame->DataV(), i420Frame->StrideV(),
                                   dataY, strideY, dataUV, strideUV,
                                   i420Frame->width(), i420Frame->height());
    }
    if (type == VideoFrameBuffer::Type::kI010) {
        auto* i010Frame = buffer.GetI010();
        auto* dataY = reinterpret_cast<uint16_t*>(data);
        auto strideY = i010Frame->width();
        auto strideUV = i010Frame->width();
        auto* dataUV = dataY + (i010Frame->width() * i010Frame->height());
        return !libyuv::I010ToP010(i010Frame->DataY(), i010Frame->StrideY(),
                                   i010Frame->DataU(), i010Frame->StrideU(),
                                   i010Frame->DataV(), i010Frame->StrideV(),
                                   dataY, strideY, dataUV, strideUV,
                                   i010Frame->width(), i010Frame->height());
    }
    return false;
}

bool convertBGRAToYUV(CVPixelBufferRef sourceBuffer, CVPixelBufferRef destinationBuffer)
{
    int width = CVPixelBufferGetWidth(sourceBuffer);
    int height = CVPixelBufferGetHeight(sourceBuffer);

    CVPixelBufferLockBaseAddress(destinationBuffer, 0);
    CVPixelBufferLockBaseAddress(sourceBuffer, kCVPixelBufferLock_ReadOnly);

    auto* destinationY = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(destinationBuffer, 0));
    auto* destinationUV = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(destinationBuffer, 1));
    uint32_t bytesPerRowDestinationY = CVPixelBufferGetBytesPerRowOfPlane(destinationBuffer, 0);
    uint32_t bytesPerRowDestinationUV = CVPixelBufferGetBytesPerRowOfPlane(destinationBuffer, 1);

    auto result = libyuv::ARGBToNV12(
        static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(sourceBuffer)),
        CVPixelBufferGetBytesPerRow(sourceBuffer),
        destinationY,
        bytesPerRowDestinationY,
        destinationUV,
        bytesPerRowDestinationUV,
        width,
        height);

    CVPixelBufferUnlockBaseAddress(sourceBuffer, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferUnlockBaseAddress(destinationBuffer, 0);

    if (result)
        return false;

    return true;
}

}
