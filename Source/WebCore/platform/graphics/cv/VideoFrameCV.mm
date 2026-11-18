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
#include "CMUtilities.h"
#include "CVUtilities.h"
#include "FourCC.h"
#include "GraphicsContext.h"
#include "ImageTransferSessionVT.h"
#include "Logging.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include "PixelBufferConformerCV.h"
#include "PlanarImageConverterCV.h"
#include "ProcessIdentity.h"
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>

#include <pal/cf/AudioToolboxSoftLink.h>
#include <pal/cf/CoreMediaSoftLink.h>

#include "CoreVideoSoftLink.h"

namespace WebCore {

RefPtr<VideoFrame> VideoFrame::fromNativeImage(NativeImage& image)
{
    // FIXME: Set VideoFrame colorSpace.
    auto transferSession = ImageTransferSessionVT::create(kCVPixelFormatType_32ARGB, false);
    return transferSession->createVideoFrame(image.platformImage().get(), { }, image.size(), image.hasAlpha() ? ImageTransferSessionVT::ShouldPreserveAlpha::Yes : ImageTransferSessionVT::ShouldPreserveAlpha::No);
}

static RetainPtr<CVPixelBufferRef> createPixelBuffer(size_t width, size_t height, OSType pixelFormat)
{
    CVPixelBufferRef rawPixelBuffer = nullptr;
    auto status = CVPixelBufferCreate(kCFAllocatorDefault, width, height, pixelFormat, nullptr, &rawPixelBuffer);
    if (status != kCVReturnSuccess || !rawPixelBuffer)
        return nullptr;
    return adoptCF(rawPixelBuffer);
}

RefPtr<VideoFrame> VideoFrame::createNV12(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& planeY, const ComputedPlaneLayout& planeUV, Init&& init)
{
    constexpr auto subsampling = PlanarImageConverter::ChromaSubsampling::HorizontalAndVertical;
    bool isFullRange = init.colorSpace.fullRange.value_or(false);

    auto y = PlanarImageConverter::makePlane(span, planeY.destinationOffset, planeY.destinationStride, width, height, PlanarImageConverter::eightBit);
    auto uv = PlanarImageConverter::makePlane(span, planeUV.destinationOffset, planeUV.destinationStride, PlanarImageConverter::chromaWidth(subsampling, width), PlanarImageConverter::chromaHeight(subsampling, height), PlanarImageConverter::eightBit, 2);
    if (!y || !uv)
        return nullptr;

    RetainPtr pixelBuffer = createPixelBuffer(width, height, isFullRange ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
    if (!pixelBuffer || !PlanarImageConverter::copyFrame(PlanarImageConverter::SourceBiPlanarFrame { subsampling, isFullRange, *y, *uv }, pixelBuffer.get()))
        return nullptr;

    return VideoFrameCV::create(init.presentationTime, init.isMirrored, init.rotation, WTF::move(pixelBuffer), WTF::move(init.colorSpace), VideoFrameCV::ShouldIgnoreAlpha::No);
}

static constexpr std::array<uint8_t, 4> sameChannelOrder { 0, 1, 2, 3 };
static constexpr std::array<uint8_t, 4> rgbaToARGB { 3, 0, 1, 2 };
static constexpr std::array<uint8_t, 4> argbToRGBA { 1, 2, 3, 0 };

static RetainPtr<CVPixelBufferRef> createPackedPixelBuffer(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& plane, OSType pixelFormat, std::array<uint8_t, 4> channelMap)
{
    constexpr uint8_t rgbaChannelCount = 4;
    auto source = PlanarImageConverter::makePlane(span, plane.destinationOffset, plane.destinationStride, width, height, PlanarImageConverter::eightBit, rgbaChannelCount);
    if (!source)
        return nullptr;

    RetainPtr pixelBuffer = createPixelBuffer(width, height, pixelFormat);
    if (!pixelBuffer || !PlanarImageConverter::copyToPixelBuffer(*source, pixelBuffer.get(), channelMap))
        return nullptr;
    return pixelBuffer;
}

RefPtr<VideoFrame> VideoFrame::createRGBA(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& plane, Init&& init)
{
    RetainPtr pixelBuffer = createPackedPixelBuffer(span, width, height, plane, kCVPixelFormatType_32ARGB, rgbaToARGB);
    if (!pixelBuffer)
        return nullptr;

    return VideoFrameCV::create(init.presentationTime, init.isMirrored, init.rotation, WTF::move(pixelBuffer), WTF::move(init.colorSpace), VideoFrameCV::ShouldIgnoreAlpha::No);
}

RefPtr<VideoFrame> VideoFrame::createRGBX(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& plane, Init&& init)
{
    RetainPtr pixelBuffer = createPackedPixelBuffer(span, width, height, plane, kCVPixelFormatType_32ARGB, rgbaToARGB);
    if (!pixelBuffer)
        return nullptr;

    return VideoFrameCV::create(init.presentationTime, init.isMirrored, init.rotation, WTF::move(pixelBuffer), WTF::move(init.colorSpace), VideoFrameCV::ShouldIgnoreAlpha::Yes);
}

RefPtr<VideoFrame> VideoFrame::createBGRA(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& plane, Init&& init)
{
    RetainPtr pixelBuffer = createPackedPixelBuffer(span, width, height, plane, kCVPixelFormatType_32BGRA, sameChannelOrder);
    if (!pixelBuffer)
        return nullptr;

    return VideoFrameCV::create(init.presentationTime, init.isMirrored, init.rotation, WTF::move(pixelBuffer), WTF::move(init.colorSpace), VideoFrameCV::ShouldIgnoreAlpha::No);
}

RefPtr<VideoFrame> VideoFrame::createBGRX(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& plane, Init&& init)
{
    RetainPtr pixelBuffer = createPackedPixelBuffer(span, width, height, plane, kCVPixelFormatType_32BGRA, sameChannelOrder);
    if (!pixelBuffer)
        return nullptr;

    return VideoFrameCV::create(init.presentationTime, init.isMirrored, init.rotation, WTF::move(pixelBuffer), WTF::move(init.colorSpace), VideoFrameCV::ShouldIgnoreAlpha::Yes);
}

static RetainPtr<CVPixelBufferRef> createPixelBufferFromI420(std::span<const uint8_t> buffer, size_t width, size_t height, const ComputedPlaneLayout& layoutY, const ComputedPlaneLayout& layoutU, const ComputedPlaneLayout& layoutV, const ComputedPlaneLayout* layoutA, bool isFullRange)
{
    constexpr auto subsampling = PlanarImageConverter::ChromaSubsampling::HorizontalAndVertical;
    constexpr auto format = PlanarImageConverter::eightBit;
    size_t chromaWidth = PlanarImageConverter::chromaWidth(subsampling, width);
    size_t chromaHeight = PlanarImageConverter::chromaHeight(subsampling, height);

    auto y = PlanarImageConverter::makePlane(buffer, layoutY.destinationOffset, layoutY.destinationStride, width, height, format);
    auto u = PlanarImageConverter::makePlane(buffer, layoutU.destinationOffset, layoutU.destinationStride, chromaWidth, chromaHeight, format);
    auto v = PlanarImageConverter::makePlane(buffer, layoutV.destinationOffset, layoutV.destinationStride, chromaWidth, chromaHeight, format);
    if (!y || !u || !v)
        return nullptr;

    std::optional<PlanarImageConverter::SourcePlane> alpha;
    std::optional<PlanarImageConverter::SampleFormat> alphaFormat;
    if (layoutA) {
        alpha = PlanarImageConverter::makePlane(buffer, layoutA->destinationOffset, layoutA->destinationStride, width, height, format);
        if (!alpha)
            return nullptr;
        alphaFormat = format;
    }

    auto pixelFormat = PlanarImageConverter::pixelFormat(subsampling, format, isFullRange, alphaFormat);
    if (!pixelFormat)
        return nullptr;
    RetainPtr pixelBuffer = createPixelBuffer(width, height, *pixelFormat);
    if (!pixelBuffer || !PlanarImageConverter::copyFrame(PlanarImageConverter::SourceFrame { subsampling, isFullRange, *y, *u, *v, alpha }, pixelBuffer.get()))
        return nullptr;
    return pixelBuffer;
}

RefPtr<VideoFrame> VideoFrame::createI420(std::span<const uint8_t> buffer, size_t width, size_t height, const ComputedPlaneLayout& layoutY, const ComputedPlaneLayout& layoutU, const ComputedPlaneLayout& layoutV, Init&& init)
{
    RetainPtr pixelBuffer = createPixelBufferFromI420(buffer, width, height, layoutY, layoutU, layoutV, nullptr, init.colorSpace.fullRange.value_or(false));
    if (!pixelBuffer)
        return nullptr;

    return VideoFrameCV::create(init.presentationTime, init.isMirrored, init.rotation, WTF::move(pixelBuffer), WTF::move(init.colorSpace), VideoFrameCV::ShouldIgnoreAlpha::No);
}

RefPtr<VideoFrame> VideoFrame::createI420A(std::span<const uint8_t> buffer, size_t width, size_t height, const ComputedPlaneLayout& layoutY, const ComputedPlaneLayout& layoutU, const ComputedPlaneLayout& layoutV, const ComputedPlaneLayout& layoutA, Init&& init)
{
    // The 8-bit formats with an alpha plane are video range only, so the samples are stored unchanged.
    RetainPtr pixelBuffer = createPixelBufferFromI420(buffer, width, height, layoutY, layoutU, layoutV, &layoutA, false);
    if (!pixelBuffer)
        return nullptr;

    return VideoFrameCV::create(init.presentationTime, init.isMirrored, init.rotation, WTF::move(pixelBuffer), WTF::move(init.colorSpace), VideoFrameCV::ShouldIgnoreAlpha::No);
}

template<typename Byte>
static std::optional<PlanarImageConverter::PlanarFrame<Byte>> planarFrame(std::span<Byte> buffer, const PlanarYUVFormatLayout& layout, bool isFullRange, size_t width, size_t height, size_t chromaWidth, size_t chromaHeight, std::span<const ComputedPlaneLayout, 3> layouts)
{
    auto y = PlanarImageConverter::makePlane(buffer, layouts[0].destinationOffset, layouts[0].destinationStride, width, height, layout.format);
    auto u = PlanarImageConverter::makePlane(buffer, layouts[1].destinationOffset, layouts[1].destinationStride, chromaWidth, chromaHeight, layout.format);
    auto v = PlanarImageConverter::makePlane(buffer, layouts[2].destinationOffset, layouts[2].destinationStride, chromaWidth, chromaHeight, layout.format);
    if (!y || !u || !v)
        return { };
    return PlanarImageConverter::PlanarFrame<Byte> { layout.subsampling, isFullRange, *y, *u, *v, { } };
}

RefPtr<VideoFrame> VideoFrame::createPlanarYUV(VideoPixelFormat format, std::span<const uint8_t> buffer, size_t width, size_t height, std::span<const ComputedPlaneLayout, 3> layouts, Init&& init)
{
    auto layout = planarYUVFormatLayout(format);
    if (!layout)
        return nullptr;
    auto sampleFormat = PlanarImageConverter::pixelBufferSampleFormat(layout->format);
    if (!sampleFormat)
        return nullptr;

    bool isFullRange = init.colorSpace.fullRange.value_or(false);
    auto frame = planarFrame(buffer, *layout, isFullRange, width, height, PlanarImageConverter::chromaWidth(layout->subsampling, width), PlanarImageConverter::chromaHeight(layout->subsampling, height), layouts);
    auto pixelFormat = PlanarImageConverter::pixelFormat(layout->subsampling, *sampleFormat, isFullRange);
    if (!frame || !pixelFormat)
        return nullptr;

    RetainPtr pixelBuffer = createPixelBuffer(width, height, *pixelFormat);
    if (!pixelBuffer || !PlanarImageConverter::copyFrame(*frame, pixelBuffer.get()))
        return nullptr;

    return VideoFrameCV::create(init.presentationTime, init.isMirrored, init.rotation, WTF::move(pixelBuffer), WTF::move(init.colorSpace), VideoFrameCV::ShouldIgnoreAlpha::No);
}

static Vector<PlaneLayout> planeLayouts(std::span<const ComputedPlaneLayout> computedPlaneLayout)
{
    Vector<PlaneLayout> layouts;
    for (auto& plane : computedPlaneLayout)
        layouts.append({ plane.destinationOffset, plane.destinationStride });
    return layouts;
}

static Vector<PlaneLayout> copyPacked(std::span<uint8_t> span, std::span<const ComputedPlaneLayout> computedPlaneLayout, CVPixelBufferRef pixelBuffer, std::array<uint8_t, 4> channelMap, std::optional<uint8_t> opaqueChannel)
{
    constexpr size_t bytesPerPixel = 4;
    if (computedPlaneLayout.size() != 1)
        return { };
    auto& layout = computedPlaneLayout[0];
    if (layout.sourceLeftBytes % bytesPerPixel || layout.sourceWidthBytes % bytesPerPixel)
        return { };

    auto destination = PlanarImageConverter::makePlane(span, layout.destinationOffset, layout.destinationStride, layout.sourceWidthBytes / bytesPerPixel, layout.sourceHeight, PlanarImageConverter::eightBit, bytesPerPixel);
    if (!destination || !PlanarImageConverter::copyFromPixelBuffer(pixelBuffer, layout.sourceLeftBytes / bytesPerPixel, layout.sourceTop, *destination, channelMap, opaqueChannel)) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo failed to copy the RGB data");
        return { };
    }
    return planeLayouts(computedPlaneLayout);
}

static Vector<PlaneLayout> copyNV12(std::span<uint8_t> span, std::span<const ComputedPlaneLayout> computedPlaneLayout, CVPixelBufferRef pixelBuffer)
{
    auto description = PlanarImageConverter::pixelFormatDescription(CVPixelBufferGetPixelFormatType(pixelBuffer));
    if (!description || computedPlaneLayout.size() != 2)
        return { };

    auto& layoutY = computedPlaneLayout[0];
    auto& layoutUV = computedPlaneLayout[1];
    auto y = PlanarImageConverter::makePlane(span, layoutY.destinationOffset, layoutY.destinationStride, layoutY.sourceWidthBytes, layoutY.sourceHeight, PlanarImageConverter::eightBit);
    auto uv = PlanarImageConverter::makePlane(span, layoutUV.destinationOffset, layoutUV.destinationStride, layoutUV.sourceWidthBytes / 2, layoutUV.sourceHeight, PlanarImageConverter::eightBit, 2);
    if (!y || !uv || !PlanarImageConverter::copyFrame(pixelBuffer, layoutY.sourceLeftBytes, layoutY.sourceTop, PlanarImageConverter::DestinationBiPlanarFrame { PlanarImageConverter::ChromaSubsampling::HorizontalAndVertical, description->isFullRange, *y, *uv })) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo failed to copy the NV12 planes");
        return { };
    }
    return planeLayouts(computedPlaneLayout);
}

// 4 layouts for I420A.
static Vector<PlaneLayout> copyI420OrI420A(std::span<uint8_t> span, std::span<const ComputedPlaneLayout> computedPlaneLayout, CVPixelBufferRef pixelBuffer)
{
    auto description = PlanarImageConverter::pixelFormatDescription(CVPixelBufferGetPixelFormatType(pixelBuffer));
    if (!description || computedPlaneLayout.size() < 3)
        return { };

    auto destinationPlane = [&](const ComputedPlaneLayout& layout) {
        return PlanarImageConverter::makePlane(span, layout.destinationOffset, layout.destinationStride, layout.sourceWidthBytes, layout.sourceHeight, PlanarImageConverter::eightBit);
    };
    auto y = destinationPlane(computedPlaneLayout[0]);
    auto u = destinationPlane(computedPlaneLayout[1]);
    auto v = destinationPlane(computedPlaneLayout[2]);
    std::optional<PlanarImageConverter::DestinationPlane> alpha;
    if (computedPlaneLayout.size() > 3) {
        alpha = destinationPlane(computedPlaneLayout[3]);
        if (!alpha)
            return { };
    }

    auto& layoutY = computedPlaneLayout[0];
    if (!y || !u || !v || !PlanarImageConverter::copyFrame(pixelBuffer, layoutY.sourceLeftBytes, layoutY.sourceTop, PlanarImageConverter::DestinationFrame { PlanarImageConverter::ChromaSubsampling::HorizontalAndVertical, description->isFullRange, *y, *u, *v, alpha })) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo failed to copy the I420 planes");
        return { };
    }
    return planeLayouts(computedPlaneLayout);
}

static Vector<PlaneLayout> copyPlanarYUV(std::span<uint8_t> span, const PlanarYUVFormatLayout& layout, std::span<const ComputedPlaneLayout> computedPlaneLayout, CVPixelBufferRef pixelBuffer)
{
    if (computedPlaneLayout.size() != 3)
        return { };

    auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
    auto description = PlanarImageConverter::pixelFormatDescription(pixelFormat);
    if (!description || description->alpha || description->subsampling != layout.subsampling || description->color != PlanarImageConverter::pixelBufferSampleFormat(layout.format)) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo pixel buffer format %" PUBLIC_LOG_STRING " doesn't match the requested format", FourCC(pixelFormat).string().data());
        return { };
    }

    auto& layoutY = computedPlaneLayout[0];
    auto& layoutU = computedPlaneLayout[1];
    size_t sampleSize = layout.format.sampleSize();
    if (layoutY.sourceWidthBytes % sampleSize || layoutU.sourceWidthBytes % sampleSize || layoutY.sourceLeftBytes % sampleSize)
        return { };

    auto frame = planarFrame(span, layout, description->isFullRange, layoutY.sourceWidthBytes / sampleSize, layoutY.sourceHeight, layoutU.sourceWidthBytes / sampleSize, layoutU.sourceHeight, computedPlaneLayout.first<3>());
    if (!frame || !PlanarImageConverter::copyFrame(pixelBuffer, layoutY.sourceLeftBytes / sampleSize, layoutY.sourceTop, *frame)) {
        RELEASE_LOG_ERROR(WebRTC, "VideoFrame::copyTo failed to copy the planes");
        return { };
    }
    return planeLayouts(computedPlaneLayout);
}

void VideoFrame::copyTo(std::span<uint8_t> span, VideoPixelFormat format, ColorSpace colorSpace, Vector<ComputedPlaneLayout>&& computedPlaneLayout, CompletionHandler<void(std::optional<Vector<PlaneLayout>>&&)>&& callback)
{
    // FIXME: We should get the pixel buffer and copy the bytes asynchronously.
    RetainPtr pixelBuffer = this->pixelBuffer();
    auto currentVideoFramePixelFormat = convertVideoFramePixelFormat(pixelFormat());

    if (isRGBVideoPixelFormat(format)) {
        auto shouldConvert = [](auto sourceFormat, auto sourceColorSpace, auto destinationFormat, auto destinationColorSpace) -> std::optional<OSType> {
            bool isDestinationFormatRGB = destinationFormat == VideoPixelFormat::RGBA || destinationFormat == VideoPixelFormat::RGBX;
            if (!sourceFormat || !isRGBVideoPixelFormat(*sourceFormat))
                return isDestinationFormatRGB ? kCVPixelFormatType_32ARGB : kCVPixelFormatType_32BGRA;

            bool isDestinationColorSpaceSRGB = destinationColorSpace == ColorSpace::SRGB();
            bool isSourceColorSpaceSRGB = sourceColorSpace == srgbColorSpace();
            if (!isDestinationColorSpaceSRGB || !isSourceColorSpaceSRGB)
                return isDestinationFormatRGB ? kCVPixelFormatType_32ARGB : kCVPixelFormatType_32BGRA;

            // FIXME: We could directly support RGBA -> BGRA at copyTo without doing a conversion.
            bool isSourceFormatRGB = *sourceFormat == VideoPixelFormat::RGBA || *sourceFormat == VideoPixelFormat::RGBX;
            if (isSourceFormatRGB != isDestinationFormatRGB)
                return isDestinationFormatRGB ? kCVPixelFormatType_32ARGB : kCVPixelFormatType_32BGRA;

            return { };
        };
        if (auto conversionFormat = shouldConvert(currentVideoFramePixelFormat, this->colorSpace(), format, colorSpace)) {
            ImageTransferSessionVT::DestinationColorSpace destinationColorSpace = ImageTransferSessionVT::DestinationColorSpace::SRGB;
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
            // FIXME: Decide whether to support linear variants when https://github.com/w3c/webcodecs/issues/973 is fixed.
            if (colorSpace == ColorSpace::DisplayP3())
                destinationColorSpace = ImageTransferSessionVT::DestinationColorSpace::DisplayP3;
#endif
            pixelBuffer = ImageTransferSessionVT::convertPixelBuffer(pixelBuffer, *conversionFormat, destinationColorSpace);
        }
    }

    if (!pixelBuffer) {
        callback({ });
        return;
    }

    if (format == VideoPixelFormat::NV12) {
        callback(copyNV12(span, computedPlaneLayout.span(), pixelBuffer));
        return;
    }

    if (format == VideoPixelFormat::I420 || format == VideoPixelFormat::I420A) {
        callback(copyI420OrI420A(span, computedPlaneLayout.span(), pixelBuffer));
        return;
    }

    if (auto layout = planarYUVFormatLayout(format)) {
        callback(copyPlanarYUV(span, *layout, computedPlaneLayout.span(), pixelBuffer));
        return;
    }

    // The alpha channel is the last byte of RGBA and BGRA pixels.
    constexpr uint8_t alphaChannel = 3;
    std::optional<uint8_t> opaqueChannel;
    if (format == VideoPixelFormat::RGBX || format == VideoPixelFormat::BGRX || shouldIgnoreAlpha())
        opaqueChannel = alphaChannel;

    if (format == VideoPixelFormat::RGBA || format == VideoPixelFormat::RGBX) {
        // The pixel buffer is ARGB.
        callback(copyPacked(span, computedPlaneLayout.span(), pixelBuffer, argbToRGBA, opaqueChannel));
        return;
    }

    if (format == VideoPixelFormat::BGRA || format == VideoPixelFormat::BGRX) {
        callback(copyPacked(span, computedPlaneLayout.span(), pixelBuffer, sameChannelOrder, opaqueChannel));
        return;
    }

    callback({ });
}

RefPtr<NativeImage> VideoFrame::copyNativeImage() const
{
    RetainPtr source = pixelBuffer();
    RetainPtr bgraSource = CVPixelBufferGetPixelFormatType(source) != kCVPixelFormatType_32BGRA ? ImageTransferSessionVT::convertPixelBuffer(source.get(), kCVPixelFormatType_32BGRA, ImageTransferSessionVT::DestinationColorSpace::Unspecified) : source;
    if (!bgraSource)
        return nullptr;

    // Prefer the frame's own color space (if fully described), to avoid falling back to SDR sRGB.
    RetainPtr<CGColorSpaceRef> cgColorSpace;
    const auto& frameColorSpace = colorSpace();
    if (frameColorSpace.primaries && frameColorSpace.transfer) {
        attachColorSpaceToPixelBuffer(frameColorSpace, bgraSource.get());
        cgColorSpace = createCGColorSpaceForCVPixelBuffer(bgraSource.get());
    } else
        cgColorSpace = createCGColorSpaceForCVPixelBuffer(source.get());

    auto alphaInfo = shouldIgnoreAlpha() ? kCGImageAlphaNoneSkipFirst : alphaInfoForCVPixelBuffer(bgraSource);
    return NativeImage::create(WTF::move(bgraSource), alphaInfo, cgColorSpace);
}

Ref<VideoFrameCV> VideoFrameCV::create(CMSampleBufferRef sampleBuffer, bool isMirrored, Rotation rotation)
{
    RetainPtr pixelBuffer = static_cast<CVPixelBufferRef>(PAL::CMSampleBufferGetImageBuffer(sampleBuffer));
    auto timeStamp = PAL::CMSampleBufferGetOutputPresentationTimeStamp(sampleBuffer);
    if (CMTIME_IS_INVALID(timeStamp))
        timeStamp = PAL::CMSampleBufferGetPresentationTimeStamp(sampleBuffer);

    return VideoFrameCV::create(PAL::toMediaTime(timeStamp), isMirrored, rotation, pixelBuffer.get());
}

Ref<VideoFrameCV> VideoFrameCV::create(MediaTime presentationTime, bool isMirrored, Rotation rotation, RetainPtr<CVPixelBufferRef>&& pixelBuffer, PlatformVideoColorSpace&& colorSpace, ShouldIgnoreAlpha shouldIgnoreAlpha)
{
    ASSERT(pixelBuffer);
    attachColorSpaceToPixelBuffer(colorSpace, pixelBuffer.get());
    return adoptRef(*new VideoFrameCV(presentationTime, isMirrored, rotation, WTF::move(pixelBuffer), WTF::move(colorSpace), shouldIgnoreAlpha));
}

Ref<VideoFrameCV> VideoFrameCV::create(MediaTime presentationTime, bool isMirrored, Rotation rotation, RetainPtr<CVPixelBufferRef>&& pixelBuffer)
{
    ASSERT(pixelBuffer);
    return adoptRef(*new VideoFrameCV(presentationTime, isMirrored, rotation, WTF::move(pixelBuffer), computeVideoFrameColorSpace(pixelBuffer.get()), ShouldIgnoreAlpha::No));
}

RefPtr<VideoFrame> VideoFrame::createFromPixelBuffer(Ref<PixelBuffer>&& pixelBuffer, Init&& init)
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
    return RefPtr { VideoFrameCV::create(init.presentationTime, init.isMirrored, init.rotation, WTF::move(cvPixelBuffer), WTF::move(init.colorSpace), VideoFrameCV::ShouldIgnoreAlpha::No) };
}

VideoFrameCV::VideoFrameCV(MediaTime presentationTime, bool isMirrored, Rotation rotation, RetainPtr<CVPixelBufferRef>&& pixelBuffer, PlatformVideoColorSpace&& colorSpace, ShouldIgnoreAlpha shouldIgnoreAlpha)
    : VideoFrame(presentationTime, isMirrored, rotation, WTF::move(colorSpace))
    , m_pixelBuffer(WTF::move(pixelBuffer))
    , m_shouldIgnoreAlpha(shouldIgnoreAlpha == ShouldIgnoreAlpha::Yes)
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

Ref<VideoFrame> VideoFrameCV::clone()
{
    return adoptRef(*new VideoFrameCV(presentationTime(), isMirrored(), rotation(), pixelBuffer(), PlatformVideoColorSpace { colorSpace() }, shouldIgnoreAlpha() ? ShouldIgnoreAlpha::Yes : ShouldIgnoreAlpha::No));
}

}

#endif
