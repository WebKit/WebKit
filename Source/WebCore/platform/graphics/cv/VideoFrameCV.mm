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
#include "VideoFrameCV.h"

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
#include "CVUtilities.h"
#include "GraphicsContext.h"
#include "ImageTransferSessionVT.h"
#include "Logging.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include "PixelBufferConformerCV.h"
#include "ProcessIdentity.h"
#include <Accelerate/Accelerate.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/ParsingUtilities.h>

#if USE(LIBWEBRTC)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN

#include <webrtc/webkit_sdk/WebKit/WebKitUtilities.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

#include <pal/cf/AudioToolboxSoftLink.h>
#include <pal/cf/CoreMediaSoftLink.h>

#include "CoreVideoSoftLink.h"

namespace WebCore {

RefPtr<VideoFrame> VideoFrame::fromNativeImage(NativeImage& image)
{
    // FIXME: Set VideoFrame colorSpace.
    auto transferSession = ImageTransferSessionVT::create(kCVPixelFormatType_32ARGB, false);
    return transferSession->createVideoFrame(image.platformImage().get(), { }, image.size());
}

static std::span<const uint8_t> copyToCVPixelBufferPlane(CVPixelBufferRef pixelBuffer, size_t planeIndex, std::span<const uint8_t> source, size_t height, uint32_t bytesPerRowSource)
{
    auto destination = CVPixelBufferGetSpanOfPlane(pixelBuffer, planeIndex);
    uint32_t bytesPerRowDestination = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, planeIndex);
    for (unsigned i = 0; i < height; ++i) {
        memcpySpan(destination, source.first(std::min(bytesPerRowSource, bytesPerRowDestination)));
        skip(source, bytesPerRowSource);
        skip(destination, bytesPerRowDestination);
    }
    return source;
}

static vImage_Buffer makeVImageBuffer8888(std::span<uint8_t> data, size_t width, size_t height, size_t rowBytes)
{
    constexpr size_t bytesPerPixel = 4;
    size_t minRowBytes = 0;
    RELEASE_ASSERT(!__builtin_mul_overflow(bytesPerPixel, width, &minRowBytes));
    RELEASE_ASSERT(minRowBytes <= rowBytes);
    size_t minDataSize = 0;
    RELEASE_ASSERT(!__builtin_mul_overflow(rowBytes, height, &minDataSize));
    RELEASE_ASSERT(minDataSize <= data.size());

    return vImage_Buffer {
        .data = data.data(),
        .height = height,
        .width = width,
        .rowBytes = rowBytes,
    };
}

static vImage_Buffer makeVImageBuffer8888(CVPixelBufferRef buffer)
{
    return makeVImageBuffer8888(
        CVPixelBufferGetSpan(buffer),
        CVPixelBufferGetWidth(buffer),
        CVPixelBufferGetHeight(buffer),
        CVPixelBufferGetBytesPerRow(buffer));
}

RefPtr<VideoFrame> VideoFrame::createNV12(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& planeY, const ComputedPlaneLayout& planeUV, PlatformVideoColorSpace&& colorSpace)
{
    CVPixelBufferRef rawPixelBuffer = nullptr;

    auto status = CVPixelBufferCreate(kCFAllocatorDefault, width, height, kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, nullptr, &rawPixelBuffer);
    if (status != noErr || !rawPixelBuffer)
        return nullptr;
    RetainPtr pixelBuffer = adoptCF(rawPixelBuffer);

    status = CVPixelBufferLockBaseAddress(pixelBuffer.get(), 0);
    if (status != noErr)
        return nullptr;

    auto scope = makeScopeExit([pixelBuffer] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), 0);
    });
    ASSERT(span.size() >= height * planeY.sourceWidthBytes);
    if (span.size() < height * planeY.sourceWidthBytes)
        return nullptr;

    auto data = span;
    data = copyToCVPixelBufferPlane(pixelBuffer.get(), 0, data, height, planeY.sourceWidthBytes);
    if (CVPixelBufferGetPlaneCount(pixelBuffer.get()) == 2) {
        const auto heightUV = height / 2;
        ASSERT(std::to_address(span.end()) >= data.subspan(heightUV * planeUV.sourceWidthBytes).data());
        if (CVPixelBufferGetWidthOfPlane(pixelBuffer.get(), 1) != (width / 2)
            || CVPixelBufferGetHeightOfPlane(pixelBuffer.get(), 1) != heightUV
            || (data.subspan(heightUV * planeUV.sourceWidthBytes).data() > std::to_address(span.end())))
            return nullptr;
        copyToCVPixelBufferPlane(pixelBuffer.get(), 1, data, height / 2, planeUV.sourceWidthBytes);
    }

    return VideoFrameCV::create({ }, false, Rotation::None, WTFMove(pixelBuffer), WTFMove(colorSpace));
}

RefPtr<VideoFrame> VideoFrame::createRGBA(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& plane, PlatformVideoColorSpace&& colorSpace)
{
    CVPixelBufferRef rawPixelBuffer = nullptr;

    auto status = CVPixelBufferCreate(kCFAllocatorDefault, width, height, kCVPixelFormatType_32ARGB, nullptr, &rawPixelBuffer);
    if (status != noErr || !rawPixelBuffer)
        return nullptr;
    RetainPtr pixelBuffer = adoptCF(rawPixelBuffer);

    status = CVPixelBufferLockBaseAddress(pixelBuffer.get(), 0);
    if (status != noErr)
        return nullptr;

    auto scope = makeScopeExit([pixelBuffer] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), 0);
    });

    // Convert from RGBA to ARGB.
    auto sourceBuffer = makeVImageBuffer8888(spanConstCast<uint8_t>(span), width, height, plane.sourceWidthBytes);
    auto destinationBuffer = makeVImageBuffer8888(pixelBuffer.get());
    uint8_t channelMap[4] = { 3, 0, 1, 2 };
    auto error = vImagePermuteChannels_ARGB8888(&sourceBuffer, &destinationBuffer, channelMap, kvImageDoNotTile);
    // Permutation will not fail as long as the provided arguments are valid.
    ASSERT_UNUSED(error, error == kvImageNoError);

    return VideoFrameCV::create({ }, false, Rotation::None, WTFMove(pixelBuffer), WTFMove(colorSpace));
}

RefPtr<VideoFrame> VideoFrame::createBGRA(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& plane, PlatformVideoColorSpace&& colorSpace)
{
    CVPixelBufferRef rawPixelBuffer = nullptr;

    auto status = CVPixelBufferCreate(kCFAllocatorDefault, width, height, kCVPixelFormatType_32BGRA, nullptr, &rawPixelBuffer);
    if (status != noErr || !rawPixelBuffer)
        return nullptr;
    RetainPtr pixelBuffer = adoptCF(rawPixelBuffer);

    status = CVPixelBufferLockBaseAddress(pixelBuffer.get(), 0);
    if (status != noErr)
        return nullptr;

    auto scope = makeScopeExit([pixelBuffer] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), 0);
    });

    auto sourceBuffer = makeVImageBuffer8888(spanConstCast<uint8_t>(span), width, height, plane.sourceWidthBytes);
    auto destinationBuffer = makeVImageBuffer8888(pixelBuffer.get());
    auto error = vImageCopyBuffer(&sourceBuffer, &destinationBuffer, 4, kvImageDoNotTile);
    // Copy will not fail as long as the provided arguments are valid.
    ASSERT_UNUSED(error, error == kvImageNoError);

    return VideoFrameCV::create({ }, false, Rotation::None, WTFMove(pixelBuffer), WTFMove(colorSpace));
}

RefPtr<VideoFrame> VideoFrame::createI420(std::span<const uint8_t> buffer, size_t width, size_t height, const ComputedPlaneLayout& layoutY, const ComputedPlaneLayout& layoutU, const ComputedPlaneLayout& layoutV, PlatformVideoColorSpace&& colorSpace)
{
#if USE(LIBWEBRTC)
    size_t offsetLayoutU = layoutY.sourceLeftBytes + layoutY.sourceWidthBytes * height;
    size_t offsetLayoutV = offsetLayoutU + layoutU.sourceLeftBytes + layoutU.sourceWidthBytes * ((height + 1) / 2);
    webrtc::I420BufferLayout layout {
        layoutY.sourceLeftBytes, layoutY.sourceWidthBytes,
        offsetLayoutU, layoutU.sourceWidthBytes,
        offsetLayoutV, layoutV.sourceWidthBytes
    };
    auto pixelBuffer = adoptCF(webrtc::createPixelBufferFromI420Buffer(buffer.data(), buffer.size(), width, height, layout));

    if (!pixelBuffer)
        return nullptr;

    return VideoFrameCV::create({ }, false, Rotation::None, WTFMove(pixelBuffer), WTFMove(colorSpace));
#else
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(layoutY);
    UNUSED_PARAM(layoutU);
    UNUSED_PARAM(layoutV);
    UNUSED_PARAM(colorSpace);
    return nullptr;
#endif
}

RefPtr<VideoFrame> VideoFrame::createI420A(std::span<const uint8_t> buffer, size_t width, size_t height, const ComputedPlaneLayout& layoutY, const ComputedPlaneLayout& layoutU, const ComputedPlaneLayout& layoutV, const ComputedPlaneLayout& layoutA, PlatformVideoColorSpace&& colorSpace)
{
#if USE(LIBWEBRTC)
    size_t offsetLayoutU = layoutY.sourceLeftBytes + layoutY.sourceWidthBytes * height;
    size_t offsetLayoutV = offsetLayoutU + layoutU.sourceLeftBytes + layoutU.sourceWidthBytes * ((height + 1) / 2);
    size_t offsetLayoutA = offsetLayoutV + layoutV.sourceLeftBytes + layoutV.sourceWidthBytes * ((height + 1) / 2);
    webrtc::I420ABufferLayout layout {
        {
            layoutY.sourceLeftBytes, layoutY.sourceWidthBytes,
            offsetLayoutU, layoutU.sourceWidthBytes,
            offsetLayoutV, layoutV.sourceWidthBytes
        },
        offsetLayoutA, layoutA.sourceWidthBytes
    };
    auto pixelBuffer = adoptCF(webrtc::createPixelBufferFromI420ABuffer(buffer.data(), buffer.size(), width, height, layout));

    if (!pixelBuffer)
        return nullptr;

    return VideoFrameCV::create({ }, false, Rotation::None, WTFMove(pixelBuffer), WTFMove(colorSpace));
#else
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(layoutY);
    UNUSED_PARAM(layoutU);
    UNUSED_PARAM(layoutV);
    UNUSED_PARAM(layoutA);
    UNUSED_PARAM(colorSpace);
    return nullptr;
#endif
}

static void copyPlane(std::span<uint8_t> destination, std::span<const uint8_t> source, uint64_t sourceStride, const ComputedPlaneLayout& spanPlaneLayout, NOESCAPE const Function<void(std::span<uint8_t>, std::span<const uint8_t>)>& copyRow)
{
    uint64_t sourceOffset = spanPlaneLayout.sourceTop * sourceStride;
    sourceOffset += spanPlaneLayout.sourceLeftBytes;
    uint64_t destinationOffset = spanPlaneLayout.destinationOffset;
    uint64_t rowBytes = spanPlaneLayout.sourceWidthBytes;
    for (size_t cptr = 0; cptr < spanPlaneLayout.sourceHeight; ++cptr) {
        copyRow(destination.data() ? destination.subspan(destinationOffset) : destination, source.subspan(sourceOffset, rowBytes));
        sourceOffset += sourceStride;
        destinationOffset += spanPlaneLayout.destinationStride;
    }
}

static Vector<PlaneLayout> copyRGBData(std::span<uint8_t> span, const ComputedPlaneLayout& spanPlaneLayout, CVPixelBufferRef pixelBuffer, NOESCAPE const Function<void(std::span<uint8_t>, std::span<const uint8_t>)>& copyRow)
{
    auto result = CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo lock failed: %d", result);
        return { };
    }

    auto scope = makeScopeExit([pixelBuffer = RetainPtr { pixelBuffer }] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    });

    auto planeA = CVPixelBufferGetSpanOfPlane(pixelBuffer, 0);
    if (!planeA.data()) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo plane A is null");
        return { };
    }

    auto width = CVPixelBufferGetWidth(pixelBuffer);
    auto bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);

    PlaneLayout planeLayout { spanPlaneLayout.destinationOffset, spanPlaneLayout.destinationStride ? spanPlaneLayout.destinationStride : 4 * width };
    copyPlane(span, planeA, bytesPerRow, spanPlaneLayout, copyRow);

    Vector<PlaneLayout> planeLayouts;
    planeLayouts.append(planeLayout);
    return planeLayouts;
}

static Vector<PlaneLayout> copyNV12(std::span<uint8_t> span, const ComputedPlaneLayout& spanPlaneLayoutY, const ComputedPlaneLayout& spanPlaneLayoutUV, CVPixelBufferRef pixelBuffer)
{
    auto result = CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo lock failed: %d", result);
        return { };
    }

    auto scope = makeScopeExit([pixelBuffer = RetainPtr { pixelBuffer }] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    });

    auto planeY = CVPixelBufferGetSpanOfPlane(pixelBuffer, 0);
    if (!planeY.data()) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo plane Y is null");
        return { };
    }
    auto planeUV = CVPixelBufferGetSpanOfPlane(pixelBuffer, 1);
    if (!planeUV.data()) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo plane UV is null");
        return { };
    }

    auto widthY = CVPixelBufferGetWidthOfPlane(pixelBuffer, 0);
    auto bytesPerRowY = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
    PlaneLayout planeLayoutY { spanPlaneLayoutY.destinationOffset, spanPlaneLayoutY.destinationStride ? spanPlaneLayoutY.destinationStride : widthY };

    copyPlane(span, planeY, bytesPerRowY, spanPlaneLayoutY, [](std::span<uint8_t> destination, std::span<const uint8_t> source) {
        memcpySpan(destination, source);
    });

    auto widthUV = CVPixelBufferGetWidthOfPlane(pixelBuffer, 1);
    auto bytesPerRowUV = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
    PlaneLayout planeLayoutUV { spanPlaneLayoutUV.destinationOffset, spanPlaneLayoutUV.destinationStride ? spanPlaneLayoutUV.destinationStride : widthUV };

    copyPlane(span, planeUV, bytesPerRowUV, spanPlaneLayoutUV, [](std::span<uint8_t> destination, std::span<const uint8_t> source) {
        memcpySpan(destination, source);
    });

    Vector<PlaneLayout> planeLayouts;
    planeLayouts.append(planeLayoutY);
    planeLayouts.append(planeLayoutUV);
    return planeLayouts;
}

static Vector<PlaneLayout> copyI420OrI420A(std::span<uint8_t> span, const ComputedPlaneLayout& spanPlaneLayoutY, const ComputedPlaneLayout& spanPlaneLayoutU, const ComputedPlaneLayout& spanPlaneLayoutV, const ComputedPlaneLayout* spanPlaneLayoutA, CVPixelBufferRef pixelBuffer)
{
    auto result = CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo lock failed: %d", result);
        return { };
    }

    auto scope = makeScopeExit([pixelBuffer = RetainPtr { pixelBuffer }] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    });

    auto planeY = CVPixelBufferGetSpanOfPlane(pixelBuffer, 0);
    if (!planeY.data()) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo plane Y is null");
        return { };
    }
    auto planeUV = CVPixelBufferGetSpanOfPlane(pixelBuffer, 1);
    if (!planeUV.data()) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo plane UV is null");
        return { };
    }

    auto widthY = CVPixelBufferGetWidthOfPlane(pixelBuffer, 0);
    auto bytesPerRowY = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
    PlaneLayout planeLayoutY { spanPlaneLayoutY.destinationOffset, spanPlaneLayoutY.destinationStride ? spanPlaneLayoutY.destinationStride : widthY };

    copyPlane(span, planeY, bytesPerRowY, spanPlaneLayoutY, [](std::span<uint8_t> destination, std::span<const uint8_t> source) {
        memcpySpan(destination, source);
    });

    auto widthUV = CVPixelBufferGetWidthOfPlane(pixelBuffer, 1);
    auto bytesPerRowUV = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);

    PlaneLayout planeLayoutU { spanPlaneLayoutU.destinationOffset, spanPlaneLayoutU.destinationStride ? spanPlaneLayoutU.destinationStride : widthUV / 2 };
    PlaneLayout planeLayoutV { spanPlaneLayoutV.destinationOffset, spanPlaneLayoutV.destinationStride ? spanPlaneLayoutV.destinationStride : widthUV / 2 };

    // We store I420 as NV12, so we need to recompute the spanPlaneLayoutUV from spanPlaneLayoutU.
    auto spanPlaneLayoutUV = spanPlaneLayoutU;
    spanPlaneLayoutUV.sourceLeftBytes += spanPlaneLayoutUV.sourceLeftBytes;
    spanPlaneLayoutUV.sourceWidthBytes += spanPlaneLayoutUV.sourceWidthBytes;

    auto destinationU = span.subspan(spanPlaneLayoutU.destinationOffset);
    auto destinationV = span.subspan(spanPlaneLayoutV.destinationOffset);
    copyPlane({ }, planeUV, bytesPerRowUV, spanPlaneLayoutUV, [&destinationU, &destinationV, strideU = planeLayoutU.stride, strideV = planeLayoutV.stride](std::span<uint8_t>, std::span<const uint8_t> source) {
        for (size_t cptr = 0, destinationIndex = 0; cptr < source.size(); ++destinationIndex) {
            destinationU[destinationIndex] = source[cptr++];
            destinationV[destinationIndex] = source[cptr++];
        }
        skip(destinationU, strideU);
        skip(destinationV, strideV);
    });

    Vector<PlaneLayout> planeLayouts;
    planeLayouts.append(planeLayoutY);
    planeLayouts.append(planeLayoutU);
    planeLayouts.append(planeLayoutV);

    if (spanPlaneLayoutA) {
        auto planeA = CVPixelBufferGetSpanOfPlane(pixelBuffer, 2);
        if (!planeA.data()) {
            RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo plane A is null");
            return { };
        }

        auto widthA = CVPixelBufferGetWidthOfPlane(pixelBuffer, 2);
        auto bytesPerRowA = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 2);

        PlaneLayout planeLayoutA { spanPlaneLayoutA->destinationOffset, spanPlaneLayoutA->destinationStride ? spanPlaneLayoutA->destinationStride : widthA };

        size_t planeAEnd;
        if (!WTF::safeMultiply(planeLayoutA.stride, spanPlaneLayoutA->sourceHeight, planeAEnd))
            return { };
        if (!WTF::safeAdd(planeLayoutA.offset, planeAEnd, planeAEnd))
            return { };
        if (planeAEnd > span.size())
            return { };

        copyPlane(span, planeA, bytesPerRowA, *spanPlaneLayoutA, [](std::span<uint8_t> destination, std::span<const uint8_t> source) {
            memcpySpan(destination, source);
        });
        planeLayouts.append(planeLayoutA);
    }

    return planeLayouts;
}

void VideoFrame::copyTo(std::span<uint8_t> span, VideoPixelFormat format, Vector<ComputedPlaneLayout>&& computedPlaneLayout, CompletionHandler<void(std::optional<Vector<PlaneLayout>>&&)>&& callback)
{
    // FIXME: We should get the pixel buffer and copy the bytes asynchronously.
    if (format == VideoPixelFormat::NV12) {
        callback(copyNV12(span, computedPlaneLayout[0], computedPlaneLayout[1], this->protectedPixelBuffer().get()));
        return;
    }

    if (format == VideoPixelFormat::I420) {
        callback(copyI420OrI420A(span, computedPlaneLayout[0], computedPlaneLayout[1], computedPlaneLayout[2], nullptr, this->protectedPixelBuffer().get()));
        return;
    }

    if (format == VideoPixelFormat::I420A) {
        callback(copyI420OrI420A(span, computedPlaneLayout[0], computedPlaneLayout[1], computedPlaneLayout[2], &computedPlaneLayout[3], this->protectedPixelBuffer().get()));
        return;
    }

    if (format == VideoPixelFormat::RGBA) {
        ComputedPlaneLayout planeLayout;
        if (!computedPlaneLayout.isEmpty())
            planeLayout = computedPlaneLayout[0];
        auto planeLayouts = copyRGBData(span, planeLayout, this->protectedPixelBuffer().get(), [](std::span<uint8_t> destination, std::span<const uint8_t> source) {
            ASSERT(!(source.size() % 4));
            auto pixelCount = source.size() / 4;
            size_t i = 0;
            while (pixelCount-- > 0) {
                // ARGB -> RGBA.
                destination[i] = source[i + 1];
                destination[i + 1] = source[i + 2];
                destination[i + 2] = source[i + 3];
                destination[i + 3] = source[i];
                i += 4;
            }
        });
        callback(WTFMove(planeLayouts));
        return;
    }

    if (format == VideoPixelFormat::BGRA) {
        ComputedPlaneLayout planeLayout;
        if (!computedPlaneLayout.isEmpty())
            planeLayout = computedPlaneLayout[0];

        auto planeLayouts = copyRGBData(span, planeLayout, this->protectedPixelBuffer().get(), [](std::span<uint8_t> destination, std::span<const uint8_t> source) {
            memcpySpan(destination, source);
        });
        callback(WTFMove(planeLayouts));
        return;
    }

    callback({ });
}

RefPtr<NativeImage> VideoFrameCV::copyNativeImage() const
{
    // FIXME: It is not efficient to create a conformer everytime. We might want to make it more efficient, for instance by storing it in GraphicsContext.
    auto conformer = makeUnique<PixelBufferConformerCV>(kCVPixelFormatType_32BGRA);
    return NativeImage::create(conformer->createImageFromPixelBuffer(protectedPixelBuffer().get()));
}

Ref<VideoFrameCV> VideoFrameCV::create(CMSampleBufferRef sampleBuffer, bool isMirrored, Rotation rotation)
{
    RetainPtr pixelBuffer = static_cast<CVPixelBufferRef>(PAL::CMSampleBufferGetImageBuffer(sampleBuffer));
    auto timeStamp = PAL::CMSampleBufferGetOutputPresentationTimeStamp(sampleBuffer);
    if (CMTIME_IS_INVALID(timeStamp))
        timeStamp = PAL::CMSampleBufferGetPresentationTimeStamp(sampleBuffer);

    return VideoFrameCV::create(PAL::toMediaTime(timeStamp), isMirrored, rotation, pixelBuffer.get());
}

Ref<VideoFrameCV> VideoFrameCV::create(MediaTime presentationTime, bool isMirrored, Rotation rotation, RetainPtr<CVPixelBufferRef>&& pixelBuffer, std::optional<PlatformVideoColorSpace>&& colorSpace)
{
    ASSERT(pixelBuffer);
    return adoptRef(*new VideoFrameCV(presentationTime, isMirrored, rotation, WTFMove(pixelBuffer), WTFMove(colorSpace)));
}

RefPtr<VideoFrame> VideoFrame::createFromPixelBuffer(Ref<PixelBuffer>&& pixelBuffer, PlatformVideoColorSpace&& colorSpace)
{
    auto size = pixelBuffer->size();
    auto width = size.width();
    auto height = size.height();
    auto dataBaseAddress = pixelBuffer->bytes().data();
    
    auto derefBuffer = [] (void* context, const void*) {
        static_cast<PixelBuffer*>(context)->deref();
    };

    CVPixelBufferRef cvPixelBufferRaw = nullptr;
    auto status = CVPixelBufferCreateWithBytes(kCFAllocatorDefault, width, height, kCVPixelFormatType_32BGRA, dataBaseAddress, width * 4, derefBuffer, pixelBuffer.ptr(), nullptr, &cvPixelBufferRaw);

    auto cvPixelBuffer = adoptCF(cvPixelBufferRaw);
    if (!cvPixelBuffer) {
        return nullptr;
    }

    // The CVPixelBuffer now owns the pixelBuffer and will call `deref()` on it when it gets destroyed.
    pixelBuffer->ref();

    ASSERT_UNUSED(status, !status);
    return RefPtr { VideoFrameCV::create({ }, false, Rotation::None, WTFMove(cvPixelBuffer), WTFMove(colorSpace)) };
}

static PlatformVideoColorSpace computeVideoFrameColorSpace(CVPixelBufferRef pixelBuffer)
{
    if (!pixelBuffer)
        return { };

    std::optional<PlatformVideoColorPrimaries> primaries;
    auto pixelPrimaries = CVBufferGetAttachment(pixelBuffer, kCVImageBufferColorPrimariesKey, nil);
    if (safeCFEqual(pixelPrimaries, kCVImageBufferColorPrimaries_ITU_R_709_2))
        primaries = PlatformVideoColorPrimaries::Bt709;
    else if (safeCFEqual(pixelPrimaries, kCVImageBufferColorPrimaries_EBU_3213))
        primaries = PlatformVideoColorPrimaries::JedecP22Phosphors;
    else if (safeCFEqual(pixelPrimaries, PAL::kCMFormatDescriptionColorPrimaries_DCI_P3))
        primaries = PlatformVideoColorPrimaries::SmpteRp431;
    else if (safeCFEqual(pixelPrimaries, PAL::kCMFormatDescriptionColorPrimaries_P3_D65))
        primaries = PlatformVideoColorPrimaries::SmpteEg432;
    else if (safeCFEqual(pixelPrimaries, PAL::kCMFormatDescriptionColorPrimaries_ITU_R_2020))
        primaries = PlatformVideoColorPrimaries::Bt2020;

    std::optional<PlatformVideoTransferCharacteristics> transfer;
    auto pixelTransfer = CVBufferGetAttachment(pixelBuffer, kCVImageBufferTransferFunctionKey, nil);
    if (safeCFEqual(pixelTransfer, kCVImageBufferTransferFunction_ITU_R_709_2))
        transfer = PlatformVideoTransferCharacteristics::Bt709;
    else if (safeCFEqual(pixelTransfer, kCVImageBufferTransferFunction_SMPTE_240M_1995))
        transfer = PlatformVideoTransferCharacteristics::Smpte240m;
    else if (safeCFEqual(pixelTransfer, PAL::kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ))
        transfer = PlatformVideoTransferCharacteristics::SmpteSt2084;
    else if (safeCFEqual(pixelTransfer, PAL::kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1))
        transfer = PlatformVideoTransferCharacteristics::SmpteSt4281;
    else if (safeCFEqual(pixelTransfer, PAL::kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG))
        transfer = PlatformVideoTransferCharacteristics::AribStdB67Hlg;
    else if (safeCFEqual(pixelTransfer, PAL::kCMFormatDescriptionTransferFunction_Linear))
        transfer = PlatformVideoTransferCharacteristics::Linear;
    else if (PAL::canLoad_CoreMedia_kCMFormatDescriptionTransferFunction_sRGB() && safeCFEqual(pixelTransfer, PAL::kCMFormatDescriptionTransferFunction_sRGB))
        transfer = PlatformVideoTransferCharacteristics::Iec6196621;

    std::optional<PlatformVideoMatrixCoefficients> matrix;
    auto pixelMatrix = CVBufferGetAttachment(pixelBuffer, kCVImageBufferYCbCrMatrixKey, nil);
    if (safeCFEqual(pixelMatrix, PAL::kCMFormatDescriptionYCbCrMatrix_ITU_R_2020))
        matrix = PlatformVideoMatrixCoefficients::Bt2020NonconstantLuminance;
    else if (safeCFEqual(pixelMatrix, kCVImageBufferYCbCrMatrix_ITU_R_709_2))
        matrix = PlatformVideoMatrixCoefficients::Bt709;
    else if (safeCFEqual(pixelMatrix, kCVImageBufferYCbCrMatrix_SMPTE_240M_1995))
        matrix = PlatformVideoMatrixCoefficients::Smpte240m;

    auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
    bool isFullRange = pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;

    return { primaries, transfer, matrix, isFullRange };
}

VideoFrameCV::VideoFrameCV(MediaTime presentationTime, bool isMirrored, Rotation rotation, RetainPtr<CVPixelBufferRef>&& pixelBuffer, std::optional<PlatformVideoColorSpace>&& colorSpace)
    : VideoFrame(presentationTime, isMirrored, rotation, WTFMove(colorSpace).value_or(computeVideoFrameColorSpace(pixelBuffer.get())))
    , m_pixelBuffer(WTFMove(pixelBuffer))
{
}

VideoFrameCV::VideoFrameCV(MediaTime presentationTime, bool isMirrored, Rotation rotation, RetainPtr<CVPixelBufferRef>&& pixelBuffer, PlatformVideoColorSpace&& colorSpace)
    : VideoFrame(presentationTime, isMirrored, rotation, WTFMove(colorSpace))
    , m_pixelBuffer(WTFMove(pixelBuffer))
{
}

VideoFrameCV::~VideoFrameCV() = default;

WebCore::IntSize VideoFrameCV::presentationSize() const
{
    return { static_cast<int>(CVPixelBufferGetWidth(m_pixelBuffer.get())), static_cast<int>(CVPixelBufferGetHeight(m_pixelBuffer.get())) };
}

uint32_t VideoFrameCV::pixelFormat() const
{
    return CVPixelBufferGetPixelFormatType(m_pixelBuffer.get());
}

void VideoFrameCV::setOwnershipIdentity(const ProcessIdentity& resourceOwner)
{
    ASSERT(resourceOwner);
    RetainPtr buffer = pixelBuffer();
    ASSERT(buffer);
    setOwnershipIdentityForCVPixelBuffer(buffer.get(), resourceOwner);
}

ImageOrientation VideoFrameCV::orientation() const
{
    // Sample transform first flips x-coordinates, then rotates.
    switch (rotation()) {
    case VideoFrame::Rotation::None:
        return isMirrored() ? ImageOrientation::Orientation::OriginTopRight : ImageOrientation::Orientation::OriginTopLeft;
    case VideoFrame::Rotation::Right:
        return isMirrored() ? ImageOrientation::Orientation::OriginRightBottom : ImageOrientation::Orientation::OriginRightTop;
    case VideoFrame::Rotation::UpsideDown:
        return isMirrored() ? ImageOrientation::Orientation::OriginBottomLeft : ImageOrientation::Orientation::OriginBottomRight;
    case VideoFrame::Rotation::Left:
        return isMirrored() ? ImageOrientation::Orientation::OriginLeftTop : ImageOrientation::Orientation::OriginLeftBottom;
    }
}

Ref<VideoFrame> VideoFrameCV::clone()
{
    return adoptRef(*new VideoFrameCV(presentationTime(), isMirrored(), rotation(), pixelBuffer(), PlatformVideoColorSpace { colorSpace() }));
}

}

#endif
