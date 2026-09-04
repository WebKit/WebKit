/*
 * Copyright (C) 2021-2026 Apple Inc. All rights reserved.
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
#include "GPUQueue.h"

#include "BitmapImage.h"
#include "CachedImage.h"
#include "CanvasRenderingContext.h"
#include "GPUBuffer.h"
#include "GPUDevice.h"
#include "GPUExternalTextureDescriptor.h"
#include "GPUImageCopyExternalImage.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"
#include "GraphicsContext.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "JSDOMConvertNull.h"
#include "JSDOMPromiseDeferred.h"
#include "OffscreenCanvas.h"
#include "PixelBuffer.h"
#include "PredefinedColorSpace.h"
#include "SVGImage.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "VideoFrame.h"
#include "WebCodecsVideoFrame.h"
#include <JavaScriptCore/HeapCellInlines.h>
#include <array>
#include <wtf/CheckedArithmetic.h>
#include <wtf/MallocSpan.h>

#if PLATFORM(COCOA)
#include "ColorSpaceCG.h"
#include <Accelerate/Accelerate.h>
#include <wtf/cf/VectorCF.h>

#include "CoreVideoSoftLink.h"
#endif

namespace WebCore {

GPUQueue::GPUQueue(Ref<WebGPU::Queue>&& backing, GPUDevice& device)
    : m_backing(WTF::move(backing))
    , m_device(device)
{
}

bool GPUQueue::hasActiveInspectorCanvasCallTracer() const
{
    RefPtr device = m_device;
    return device && device->hasActiveInspectorCanvasCallTracer();
}

GPUDevice* GPUQueue::device() const
{
    return m_device;
}

String GPUQueue::label() const
{
    return m_backing->label();
}

void GPUQueue::setLabel(String&& label)
{
    m_backing->setLabel(WTF::move(label));
}

void GPUQueue::submit(Vector<Ref<GPUCommandBuffer>>&& commandBuffers)
{
    auto result = WTF::map(commandBuffers, [](auto& commandBuffer) -> Ref<WebGPU::CommandBuffer> {
        return commandBuffer->backing();
    });
    m_backing->submit(WTF::move(result));

    if (RefPtr device = m_device) {
        for (Ref commandBuffer : commandBuffers) {
            commandBuffer->setOverrideLabel(commandBuffer->label());
            commandBuffer->setBacking(device->backing().invalidCommandEncoder(), device->backing().invalidCommandBuffer());
        }
    }
}

void GPUQueue::onSubmittedWorkDone(OnSubmittedWorkDonePromise&& promise)
{
    m_backing->onSubmittedWorkDone([promise = WTF::move(promise)]() mutable {
        promise.resolve(nullptr);
    });
}

static GPUSize64 computeElementSize(const BufferSource& data)
{
    return WTF::switchOn(data.variant(),
        [&](const Ref<JSC::ArrayBufferView>& bufferView) {
            return static_cast<GPUSize64>(JSC::elementSize(bufferView->getType()));
        },
        [&](const Ref<JSC::ArrayBuffer>&) {
            return static_cast<GPUSize64>(1);
        }
    );
}

ExceptionOr<void> GPUQueue::writeBuffer(
    const GPUBuffer& buffer,
    GPUSize64 bufferOffset,
    BufferSource&& data,
    GPUSize64 optionalDataOffset,
    std::optional<GPUSize64> optionalSize)
{
    auto elementSize = computeElementSize(data);
    auto dataOffset = elementSize * optionalDataOffset;
    auto dataSize = data.byteLength();
    auto contentSize = optionalSize.has_value() ? (elementSize * optionalSize.value()) : (dataSize - dataOffset);

    if (dataOffset > dataSize || dataOffset + contentSize > dataSize || (contentSize % 4))
        return Exception { ExceptionCode::OperationError };

    m_backing->writeBuffer(buffer.backing(), bufferOffset, data.span().subspan(dataOffset, contentSize), 0, contentSize);
    return { };
}

static uint32_t getExtentDimension(const GPUExtent3D& size, size_t dimension)
{
    return WTF::switchOn(size, [&](const Vector<GPUIntegerCoordinate>& v) -> uint32_t {
        return dimension < v.size() ? v[dimension] : 0u;
    }, [&](const GPUExtent3DDict& size) -> uint32_t {
        switch (dimension) {
        default:
        case 0:
            return size.width;
        case 1:
            return size.height;
        case 2:
            return size.depthOrArrayLayers;
        }
    });
}

static uint32_t width(const GPUExtent3D& extent)
{
    return getExtentDimension(extent, 0);
}
static uint32_t height(const GPUExtent3D& extent)
{
    return getExtentDimension(extent, 1);
}
static uint32_t depth(const GPUExtent3D& extent)
{
    return getExtentDimension(extent, 2);
}

static size_t requiredBytesInCopy(const GPUImageCopyTexture& destination, const GPUImageDataLayout& layout, const GPUExtent3D& copyExtent)
{
    using namespace WebGPU;

    Ref texture = destination.texture;

    auto aspectSpecificFormat = GPUTexture::aspectSpecificFormat(texture->format(), destination.aspect);
    uint32_t blockWidth = GPUTexture::texelBlockWidth(aspectSpecificFormat);
    uint32_t blockHeight = GPUTexture::texelBlockHeight(aspectSpecificFormat);
    uint32_t blockSize = GPUTexture::texelBlockSize(aspectSpecificFormat);

    auto copyExtentWidth = width(copyExtent);
    auto widthInBlocks = copyExtentWidth / blockWidth;
    if (copyExtentWidth % blockWidth)
        return 0;

    auto copyExtentHeight = height(copyExtent);
    auto heightInBlocks = copyExtentHeight / blockHeight;
    if (copyExtentHeight % blockHeight)
        return 0;

    auto bytesInLastRow = checkedProduct<uint64_t>(blockSize, widthInBlocks);
    if (bytesInLastRow.hasOverflowed())
        return 0;

    auto requiredBytesInCopy = CheckedUint64(bytesInLastRow);
    auto bytesPerImage = CheckedUint64(0);
    if (heightInBlocks > 1) {
        if (!layout.bytesPerRow.has_value())
            return 0;

        bytesPerImage = layout.bytesPerRow.value() * heightInBlocks;
        requiredBytesInCopy = bytesPerImage;
    }

    auto copyExtentDepthOrArrayLayers = depth(copyExtent);
    if (copyExtentDepthOrArrayLayers > 1) {
        if (!layout.bytesPerRow.has_value() || !layout.rowsPerImage.has_value())
            return 0;
    }

    if (layout.bytesPerRow.has_value()) {
        if (*layout.bytesPerRow < bytesInLastRow.value())
            return 0;
    }

    if (layout.rowsPerImage.has_value()) {
        if (layout.rowsPerImage < heightInBlocks)
            return 0;
    }

    if (copyExtentDepthOrArrayLayers > 0) {
        requiredBytesInCopy = CheckedUint64(0);

        if (heightInBlocks > 1)
            requiredBytesInCopy += checkedProduct<uint64_t>(*layout.bytesPerRow, checkedDifference<uint64_t>(heightInBlocks, 1));

        if (heightInBlocks > 0)
            requiredBytesInCopy += bytesInLastRow;

        if (copyExtentDepthOrArrayLayers > 1) {
            bytesPerImage = checkedProduct<uint64_t>(*layout.bytesPerRow, *layout.rowsPerImage);

            auto bytesBeforeLastImage = checkedProduct<uint64_t>(bytesPerImage, checkedDifference<uint64_t>(copyExtentDepthOrArrayLayers, 1));
            requiredBytesInCopy += bytesBeforeLastImage;
        }
    }

    return requiredBytesInCopy.hasOverflowed() ? 0 : requiredBytesInCopy.value();
}

void GPUQueue::writeTexture(
    const GPUImageCopyTexture& destination,
    BufferSource&& data,
    const GPUImageDataLayout& initialImageDataLayout,
    const GPUExtent3D& size)
{
    auto imageDataLayout = initialImageDataLayout;
    auto initialOffset = imageDataLayout.offset;
    auto span = data.span();
    auto spanLength = span.size();
    auto requiredBytes = requiredBytesInCopy(destination, imageDataLayout, size);
    if (initialOffset >= spanLength) {
        initialOffset = 0;
        requiredBytes = spanLength;
    } else {
        imageDataLayout.offset = 0;
        requiredBytes = std::min<size_t>(spanLength - initialOffset, requiredBytes);
    }

    m_backing->writeTexture(destination.convertToBacking(), span.subspan(initialOffset, requiredBytes), imageDataLayout.convertToBacking(), convertToBacking(size));
}

#if PLATFORM(COCOA) && ENABLE(VIDEO) && ENABLE(WEB_CODECS)
static PixelFormat NODELETE toPixelFormat(GPUTextureFormat textureFormat)
{
    switch (textureFormat) {
    case GPUTextureFormat::R8unorm:
    case GPUTextureFormat::R8snorm:
    case GPUTextureFormat::R8uint:
    case GPUTextureFormat::R8sint:
    case GPUTextureFormat::R16unorm:
    case GPUTextureFormat::R16snorm:
    case GPUTextureFormat::R16uint:
    case GPUTextureFormat::R16sint:
    case GPUTextureFormat::R16float:
    case GPUTextureFormat::Rg8unorm:
    case GPUTextureFormat::Rg8snorm:
    case GPUTextureFormat::Rg8uint:
    case GPUTextureFormat::Rg8sint:
    case GPUTextureFormat::R32uint:
    case GPUTextureFormat::R32sint:
    case GPUTextureFormat::R32float:
    case GPUTextureFormat::Rg16unorm:
    case GPUTextureFormat::Rg16snorm:
    case GPUTextureFormat::Rg16uint:
    case GPUTextureFormat::Rg16sint:
    case GPUTextureFormat::Rg16float:
    case GPUTextureFormat::Rgba8unorm:
    case GPUTextureFormat::Rgba8unormSRGB:
    case GPUTextureFormat::Rgba8snorm:
    case GPUTextureFormat::Rgba8uint:
    case GPUTextureFormat::Rgba8sint:
        return PixelFormat::RGBA8;

    case GPUTextureFormat::Bgra8unorm:
    case GPUTextureFormat::Bgra8unormSRGB:
        return PixelFormat::BGRA8;
        
    case GPUTextureFormat::Rgb9e5ufloat:
    case GPUTextureFormat::Rgb10a2uint:
    case GPUTextureFormat::Rgb10a2unorm:
    case GPUTextureFormat::Rg11b10ufloat:
    case GPUTextureFormat::Rg32uint:
    case GPUTextureFormat::Rg32sint:
    case GPUTextureFormat::Rg32float:
    case GPUTextureFormat::Rgba16unorm:
    case GPUTextureFormat::Rgba16snorm:
    case GPUTextureFormat::Rgba16uint:
    case GPUTextureFormat::Rgba16sint:
    case GPUTextureFormat::Rgba16float:
    case GPUTextureFormat::Rgba32uint:
    case GPUTextureFormat::Rgba32sint:
    case GPUTextureFormat::Rgba32float:
    case GPUTextureFormat::Stencil8:
    case GPUTextureFormat::Depth16unorm:
    case GPUTextureFormat::Depth24plus:
    case GPUTextureFormat::Depth24plusStencil8:
    case GPUTextureFormat::Depth32float:
    case GPUTextureFormat::Depth32floatStencil8:
    case GPUTextureFormat::Bc1RgbaUnorm:
    case GPUTextureFormat::Bc1RgbaUnormSRGB:
    case GPUTextureFormat::Bc2RgbaUnorm:
    case GPUTextureFormat::Bc2RgbaUnormSRGB:
    case GPUTextureFormat::Bc3RgbaUnorm:
    case GPUTextureFormat::Bc3RgbaUnormSRGB:
    case GPUTextureFormat::Bc4RUnorm:
    case GPUTextureFormat::Bc4RSnorm:
    case GPUTextureFormat::Bc5RgUnorm:
    case GPUTextureFormat::Bc5RgSnorm:
    case GPUTextureFormat::Bc6hRgbUfloat:
    case GPUTextureFormat::Bc6hRgbFloat:
    case GPUTextureFormat::Bc7RgbaUnorm:
    case GPUTextureFormat::Bc7RgbaUnormSRGB:
    case GPUTextureFormat::Etc2Rgb8unorm:
    case GPUTextureFormat::Etc2Rgb8unormSRGB:
    case GPUTextureFormat::Etc2Rgb8a1unorm:
    case GPUTextureFormat::Etc2Rgb8a1unormSRGB:
    case GPUTextureFormat::Etc2Rgba8unorm:
    case GPUTextureFormat::Etc2Rgba8unormSRGB:
    case GPUTextureFormat::EacR11unorm:
    case GPUTextureFormat::EacR11snorm:
    case GPUTextureFormat::EacRg11unorm:
    case GPUTextureFormat::EacRg11snorm:
    case GPUTextureFormat::Astc4x4Unorm:
    case GPUTextureFormat::Astc4x4UnormSRGB:
    case GPUTextureFormat::Astc5x4Unorm:
    case GPUTextureFormat::Astc5x4UnormSRGB:
    case GPUTextureFormat::Astc5x5Unorm:
    case GPUTextureFormat::Astc5x5UnormSRGB:
    case GPUTextureFormat::Astc6x5Unorm:
    case GPUTextureFormat::Astc6x5UnormSRGB:
    case GPUTextureFormat::Astc6x6Unorm:
    case GPUTextureFormat::Astc6x6UnormSRGB:
    case GPUTextureFormat::Astc8x5Unorm:
    case GPUTextureFormat::Astc8x5UnormSRGB:
    case GPUTextureFormat::Astc8x6Unorm:
    case GPUTextureFormat::Astc8x6UnormSRGB:
    case GPUTextureFormat::Astc8x8Unorm:
    case GPUTextureFormat::Astc8x8UnormSRGB:
    case GPUTextureFormat::Astc10x5Unorm:
    case GPUTextureFormat::Astc10x5UnormSRGB:
    case GPUTextureFormat::Astc10x6Unorm:
    case GPUTextureFormat::Astc10x6UnormSRGB:
    case GPUTextureFormat::Astc10x8Unorm:
    case GPUTextureFormat::Astc10x8UnormSRGB:
    case GPUTextureFormat::Astc10x10Unorm:
    case GPUTextureFormat::Astc10x10UnormSRGB:
    case GPUTextureFormat::Astc12x10Unorm:
    case GPUTextureFormat::Astc12x10UnormSRGB:
    case GPUTextureFormat::Astc12x12Unorm:
    case GPUTextureFormat::Astc12x12UnormSRGB:
        return PixelFormat::RGBA8;
    }

    return PixelFormat::RGBA8;
}
#endif

using ImageDataCallback = Function<void(std::span<const uint8_t>, size_t, size_t)>;
static void getImageBytesFromImageBuffer(const RefPtr<ImageBuffer>& imageBuffer, bool& needsPremultipliedAlpha, NOESCAPE const ImageDataCallback& callback)
{
    UNUSED_PARAM(needsPremultipliedAlpha);
    if (!imageBuffer)
        return callback({ }, 0, 0);

    auto size = imageBuffer->truncatedLogicalSize();
    if (!size.width() || !size.height())
        return callback({ }, 0, 0);

    auto pixelBuffer = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() }, { { }, size });
    if (!pixelBuffer)
        return callback({ }, 0, 0);

    callback(pixelBuffer->bytes(), size.width(), size.height());
}

#if PLATFORM(COCOA) && ENABLE(VIDEO) && ENABLE(WEB_CODECS)
static void clampDimension(WebGPU::Extent3D& extent3D, size_t dimension, WebGPU::IntegerCoordinate minValue)
{
    return WTF::switchOn(extent3D, [&](Vector<WebGPU::IntegerCoordinate>& vector) {
        if (dimension < vector.size())
            vector[dimension] = std::min<WebGPU::IntegerCoordinate>(minValue, vector[dimension]);
    }, [&](WebGPU::Extent3DDict& extent3D) {
        switch (dimension) {
        case 0:
            extent3D.width = std::min<WebGPU::IntegerCoordinate>(minValue, extent3D.width);
            break;
        case 1:
            extent3D.height = std::min<WebGPU::IntegerCoordinate>(minValue, extent3D.height);
            break;
        case 2:
            extent3D.depthOrArrayLayers = std::min<WebGPU::IntegerCoordinate>(minValue, extent3D.depthOrArrayLayers);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

static void getImageBytesFromVideoFrame(WebGPU::Queue& backing, const RefPtr<VideoFrame>& videoFrame, WebGPU::Extent3D& backingCopySize, NOESCAPE const ImageDataCallback& callback)
{
    if (!videoFrame)
        return callback({ }, 0, 0);

    RefPtr<NativeImage> nativeImage = backing.getNativeImage(*videoFrame);
    if (!nativeImage)
        return callback({ }, 0, 0);

    RetainPtr platformImage = nativeImage->platformImage();
    if (!platformImage)
        return callback({ }, 0, 0);
    RetainPtr pixelDataCfData = adoptCF(CGDataProviderCopyData(CGImageGetDataProvider(platformImage.get())));
    if (!pixelDataCfData)
        return callback({ }, 0, 0);

    auto width = CGImageGetWidth(platformImage.get());
    auto height = CGImageGetHeight(platformImage.get());
    if (!width || !height)
        return callback({ }, 0, 0);

    clampDimension(backingCopySize, 0, width);
    clampDimension(backingCopySize, 1, height);

    auto sizeInBytes = height * CGImageGetBytesPerRow(platformImage.get());
    auto byteSpan = span(pixelDataCfData.get());
    vImage_Buffer bgra {
        .data = const_cast<unsigned char*>(&byteSpan[0]),
        .height = height,
        .width = width,
        .rowBytes = byteSpan.size() / height
    };
    constexpr std::array<uint8_t, 4> permuteMap { 2, 1, 0, 3 };
    vImagePermuteChannels_ARGB8888(&bgra, &bgra, permuteMap.data(), kvImageNoFlags);

    return callback(byteSpan.first(sizeInBytes), width, height);
}
#endif

#if PLATFORM(COCOA) && ENABLE(VIDEO) && ENABLE(WEB_CODECS)
static void clipTo8bitsPerChannel(std::span<const uint8_t> data, size_t bitsPerComponent, Vector<uint8_t>& byteSpanBacking)
{
    RELEASE_ASSERT(bitsPerComponent != 8);

    if (bitsPerComponent == 16) {
        byteSpanBacking.resize(data.size() / 2);
        auto uint16Span = unsafeMakeSpan(static_cast<const uint16_t*>(static_cast<const void*>(data.data())), byteSpanBacking.size());
        for (size_t i = 0; i < uint16Span.size(); ++i)
            byteSpanBacking[i] = std::min<uint8_t>(255, uint16Span[i]);
    } else if (bitsPerComponent == 32) {
        byteSpanBacking.resize(data.size() / 4);
        auto uint32Span = unsafeMakeSpan(static_cast<const uint32_t*>(static_cast<const void*>(data.data())), byteSpanBacking.size());
        for (size_t i = 0; i < uint32Span.size(); ++i)
            byteSpanBacking[i] = std::min<uint8_t>(255, uint32Span[i]);
    }
}
#endif

static void imageBytesForSource(WebGPU::Queue& backing, const GPUImageCopyExternalImage& sourceDescriptor, const GPUImageCopyTextureTagged& destination, bool& needsYFlip, bool& needsPremultipliedAlpha, WebGPU::Extent3D& backingCopySize, NOESCAPE const ImageDataCallback& callback)
{
    UNUSED_PARAM(needsYFlip);
    UNUSED_PARAM(needsPremultipliedAlpha);
    UNUSED_PARAM(backing);
    UNUSED_PARAM(backingCopySize);
    UNUSED_PARAM(destination);

    const auto& source = sourceDescriptor.source;
    using ResultType = void;
    return WTF::switchOn(source,
        [&](const Ref<ImageBitmap>& imageBitmap) -> ResultType {
            return getImageBytesFromImageBuffer(imageBitmap->buffer(), needsPremultipliedAlpha, callback);
        },
#if ENABLE(VIDEO) && ENABLE(WEB_CODECS)
        [&](const Ref<ImageData>& imageData) -> ResultType {
            callback(imageData->byteArrayPixelBuffer()->bytes(), imageData->width(), imageData->height());
        },
        [&]([[maybe_unused]] const Ref<HTMLImageElement>& imageElement) -> ResultType {
#if PLATFORM(COCOA)
            RefPtr cachedImage = imageElement->cachedImage();
            if (!cachedImage)
                return callback({ }, 0, 0);
            RefPtr image = dynamicDowncast<BitmapImage>(cachedImage->image());
            RefPtr<NativeImage> nativeImage;
            bool isSVG = false;
            if (image)
                nativeImage = image->nativeImage();
            else {
                RefPtr svgImage = dynamicDowncast<SVGImage>(cachedImage->image());
                RefPtr texturePtr = destination.texture.get();
                if (texturePtr) {
                    nativeImage = svgImage->nativeImage(FloatSize(texturePtr->width(), texturePtr->height()));
                    isSVG = true;
                }
            }

            if (!nativeImage)
                return callback({ }, 0, 0);
            RetainPtr platformImage = nativeImage->platformImage();
            if (!platformImage)
                return callback({ }, 0, 0);

            if (CGColorSpaceGetModel(CGImageGetColorSpace(platformImage.get())) == kCGColorSpaceModelIndexed) {
                auto indexedWidth = CGImageGetWidth(platformImage.get());
                auto indexedHeight = CGImageGetHeight(platformImage.get());
                RetainPtr bitmapContext = adoptCF(CGBitmapContextCreate(nullptr, indexedWidth, indexedHeight, 8, indexedWidth * 4, sRGBColorSpaceSingleton(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
                if (!bitmapContext)
                    return callback({ }, 0, 0);

                CGContextSetBlendMode(bitmapContext.get(), kCGBlendModeCopy);
                CGContextSetInterpolationQuality(bitmapContext.get(), kCGInterpolationNone);
                CGContextDrawImage(bitmapContext.get(), CGRectMake(0, 0, indexedWidth, indexedHeight), platformImage.get());

                platformImage = adoptCF(CGBitmapContextCreateImage(bitmapContext.get()));
                if (!platformImage)
                    return callback({ }, 0, 0);
            }

            RetainPtr pixelDataCfData = adoptCF(CGDataProviderCopyData(RetainPtr { CGImageGetDataProvider(platformImage.get()) }.get()));
            if (!pixelDataCfData)
                return callback({ }, 0, 0);

            auto rawWidth = CGImageGetWidth(platformImage.get());
            auto rawHeight = CGImageGetHeight(platformImage.get());

            // We need to account for EXIF orientation which may swap width/height.
            auto orientation = protect(imageElement->image())->orientation().orientation();
            bool orientationSwapsDimensions = orientation == ImageOrientation::Orientation::OriginLeftTop
                || orientation == ImageOrientation::Orientation::OriginRightTop
                || orientation == ImageOrientation::Orientation::OriginRightBottom
                || orientation == ImageOrientation::Orientation::OriginLeftBottom;

            auto orientedWidth = orientationSwapsDimensions ? rawHeight : rawWidth;
            auto orientedHeight = orientationSwapsDimensions ? rawWidth : rawHeight;

            if (!orientedWidth || !orientedHeight || !rawWidth || !rawHeight)
                return callback({ }, 0, 0);

            auto sizeInBytes = rawHeight * CGImageGetBytesPerRow(platformImage.get());
            auto bitsPerComponent = CGImageGetBitsPerComponent(platformImage.get());
            auto byteSpan = span(pixelDataCfData.get());
            Vector<uint8_t> byteSpanBacking;
            if (bitsPerComponent != 8) {
                clipTo8bitsPerChannel(byteSpan, bitsPerComponent, byteSpanBacking);
                byteSpan = byteSpanBacking.span();
                sizeInBytes = byteSpan.size();
            }

            auto requiredSize = orientedWidth * orientedHeight * 4;
            auto alphaInfo = CGImageGetAlphaInfo(platformImage.get());
            bool channelLayoutIsRGB = false;
            bool isBGRA = toPixelFormat(destination.texture->format()) == PixelFormat::BGRA8;
            bool hasAlpha = false;
            static constexpr std::array channelsSVG1 { 0, 1, 2, 3 };
            static constexpr std::array channelsSVG2 { 2, 1, 0, 3 };
            static constexpr std::array channelsRGBX { 0, 1, 2, 3 };
            static constexpr std::array channelsBGRX { 2, 1, 0, 3 };
            static constexpr std::array channelsXRGB { 3, 0, 1, 2 };
            static constexpr std::array channelsXBGR { 3, 2, 1, 0 };
            auto& channels = [&] -> const std::array<int, 4>& {
                if (isSVG)
                    return isBGRA ? channelsSVG1 : channelsSVG2;

                switch (alphaInfo) {
                case kCGImageAlphaPremultipliedLast:  /* For example, premultiplied RGBA */
                case kCGImageAlphaLast:               /* For example, non-premultiplied RGBA */
                    hasAlpha = true;
                    [[fallthrough]];
                case kCGImageAlphaNone:               /* For example, RGB. */
                case kCGImageAlphaNoneSkipLast: {     /* For example, RGBX. */
                    channelLayoutIsRGB = true;
                    return isBGRA ? channelsBGRX : channelsRGBX;
                }
                case kCGImageAlphaPremultipliedFirst: /* For example, premultiplied ARGB */
                case kCGImageAlphaFirst:              /* For example, non-premultiplied ARGB */
                case kCGImageAlphaOnly:               /* No color data, alpha data only */
                    hasAlpha = true;
                    [[fallthrough]];
                case kCGImageAlphaNoneSkipFirst: {      /* For example, XRGB. */
                    return isBGRA ? channelsXBGR : channelsXRGB;
                }
                }
            }();

            if (sizeInBytes == requiredSize && channelLayoutIsRGB && orientation == ImageOrientation::Orientation::OriginTopLeft)
                return callback(byteSpan.first(sizeInBytes), rawWidth, rawHeight);

            auto bytesPerRow = CGImageGetBytesPerRow(platformImage.get()) / (bitsPerComponent / 8);
            Vector<uint8_t> tempBuffer(FillWith { }, requiredSize, 255);
            auto bytesPerPixel = sizeInBytes / (rawWidth * rawHeight);
            bool flipY = sourceDescriptor.flipY;
            needsYFlip = false;
            int direction = flipY ? -1 : 1;
            auto maxChannelIndex = bytesPerPixel - 1;
            auto alphaIndex = 0;
            if (hasAlpha && maxChannelIndex > 0) {
                --maxChannelIndex;
                alphaIndex = 1;
            }

            auto mapDestinationToSource = [&orientation, &rawWidth, &rawHeight](size_t x, size_t y) -> std::pair<size_t, size_t> {
                switch (orientation) {
                case ImageOrientation::Orientation::OriginTopRight:
                    return { rawWidth - 1 - x, y };
                case ImageOrientation::Orientation::OriginBottomRight:
                    return { rawWidth - 1 - x, rawHeight - 1 - y };
                case ImageOrientation::Orientation::OriginBottomLeft:
                    return { x, rawHeight - 1 - y };
                case ImageOrientation::Orientation::OriginLeftTop:
                    return { y, x };
                case ImageOrientation::Orientation::OriginRightTop:
                    return { y, rawHeight - 1 - x };
                case ImageOrientation::Orientation::OriginRightBottom:
                    return { rawWidth - 1 - y, rawHeight - 1 - x };
                case ImageOrientation::Orientation::OriginLeftBottom:
                    return { rawWidth - 1 - y, x };
                default:
                    return { x, y };
                }
            };

            for (size_t y = 0, y0 = flipY ? (orientedHeight - 1) : 0; y < orientedHeight; ++y, y0 += direction) {
                for (size_t x = 0; x < orientedWidth; ++x) {
                    auto [sourceX, sourceY] = mapDestinationToSource(x, y);

                    for (size_t c = 0; c < 4; ++c) {
                        if (channels[c] == 3 && bytesPerPixel < 4)
                            tempBuffer[y0 * (orientedWidth * 4) + x * 4 + channels[c]] = hasAlpha ? byteSpan[sourceY * bytesPerRow + sourceX * bytesPerPixel + alphaIndex] : 255;
                        else
                            tempBuffer[y0 * (orientedWidth * 4) + x * 4 + channels[c]] = byteSpan[sourceY * bytesPerRow + sourceX * bytesPerPixel + std::min<size_t>(maxChannelIndex, c)];
                    }
                }
            }
            callback(tempBuffer.span(), orientedWidth, orientedHeight);
#else
            return callback({ }, 0, 0);
#endif
        },
        [&]([[maybe_unused]] const Ref<HTMLVideoElement>& videoElement) -> ResultType {
#if PLATFORM(COCOA)
            if (RefPtr player = videoElement->player(); player && player->isVideoPlayer())
                return getImageBytesFromVideoFrame(backing, player->videoFrameForCurrentTime(), backingCopySize, callback);
#endif
            return callback({ }, 0, 0);
        },
        [&]([[maybe_unused]] const Ref<WebCodecsVideoFrame>& webCodecsFrame) -> ResultType {
#if PLATFORM(COCOA)
            return getImageBytesFromVideoFrame(backing, webCodecsFrame->internalFrame(), backingCopySize, callback);
#else
            return callback({ }, 0, 0);
#endif
        },
#endif
        [&](const Ref<HTMLCanvasElement>& canvasElement) -> ResultType {
            return getImageBytesFromImageBuffer(canvasElement->makeRenderingResultsAvailable(ShouldApplyPostProcessingToDirtyRect::No), needsPremultipliedAlpha, callback);
        }
#if ENABLE(OFFSCREEN_CANVAS)
        , [&](const Ref<OffscreenCanvas>& offscreenCanvasElement) -> ResultType {
            return getImageBytesFromImageBuffer(offscreenCanvasElement->makeRenderingResultsAvailable(ShouldApplyPostProcessingToDirtyRect::No), needsPremultipliedAlpha, callback);
        }
#endif
    );
}

#if HAVE(IOSURFACE)
// Whether the backing queue can wrap an accelerated ImageBuffer of this pixel format in a texture.
// Kept in sync with QueueImpl::copyExternalImageToTexture, which does the wrapping: a format it
// cannot express has to be rejected here, while there is still a CPU path to fall back to.
static bool isSupportedGPUSourcePixelFormat(PixelFormat pixelFormat)
{
    switch (pixelFormat) {
    case PixelFormat::RGBA8:
    case PixelFormat::BGRX8:
    case PixelFormat::BGRA8:
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
#endif
        return true;
#if ENABLE(PIXEL_FORMAT_RGB10)
    case PixelFormat::RGB10:
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case PixelFormat::RGB10A8:
#endif
#if ENABLE(PIXEL_FORMAT_RGB10) || ENABLE(PIXEL_FORMAT_RGB10A8)
        // Packed 10-bit surfaces have no single-plane MTLPixelFormat equivalent.
        return false;
#endif
    }

    ASSERT_NOT_REACHED();
    return false;
}
#endif // HAVE(IOSURFACE)

// Returns the source's ImageBuffer when its pixels are already GPU-resident, so that
// GPUQueue::copyExternalImageToTexture can hand the buffer to the GPU process and let Metal do the
// copy. Sources which have no ImageBuffer, or whose ImageBuffer is unaccelerated and so has no
// IOSurface to wrap in an MTLTexture, return null and take the CPU readback path below.
struct GPUResidentSource {
    RefPtr<ImageBuffer> imageBuffer;
    // An ImageBitmap created with premultiplyAlpha: "none", and an ImageData, hold unpremultiplied
    // pixels. Everything else here was composited by a 2D context, which premultiplies.
    bool premultipliedAlpha { true };
    // Set instead of imageBuffer when the source is a video: a decoded frame is not an ImageBuffer,
    // it is a CVPixelBuffer the GPU process already holds.
    std::optional<WebGPU::VideoSourceIdentifier> videoSource { };

    bool isGPUResident() const { return imageBuffer || videoSource; }
};

#if HAVE(IOSURFACE) && ENABLE(VIDEO) && ENABLE(WEB_CODECS)
// A video's current frame is decoded in the GPU process, so the copy only has to name it: either by
// the media player which owns it, or - for a WebCodecs frame, which has no player - by the frame
// itself, which reaches the GPU process through shared memory rather than being read back here. This
// is the same identification importExternalTexture() performs, and the backing queue resolves it the
// same way, by wrapping the frame's planes in textures.
static GPUResidentSource gpuResidentSourceForVideo(const GPUVideoSource& source)
{
    auto videoSource = GPUExternalTextureDescriptor::mediaIdentifierForSource(source);

    // A media element which is not backed by a video player, or has not decoded a frame yet, and a
    // closed WebCodecs frame, all name nothing; there is nothing for either path to copy.
    if (auto* mediaIdentifier = std::get_if<std::optional<MediaPlayerIdentifier>>(&videoSource); mediaIdentifier && !*mediaIdentifier)
        return { };
    if (auto* videoFrame = std::get_if<RefPtr<VideoFrame>>(&videoSource); videoFrame && !*videoFrame)
        return { };

    // A decoded frame is opaque, so premultiplied and unpremultiplied alpha describe the same
    // pixels and the backing queue does not have to undo anything.
    return { nullptr, true, WTF::move(videoSource) };
}

// An ImageData is a plain byte array, so it has to be moved into an accelerated buffer before Metal
// can read it. Its channels go in unpremultiplied, exactly as the caller supplied them: 8-bit
// channels do not survive being premultiplied here and unpremultiplied again in the shader.
static GPUResidentSource gpuResidentSourceForImageData(ScriptExecutionContext& context, const ImageData& imageData)
{
    IntSize size { imageData.width(), imageData.height() };
    RefPtr imageBuffer = ImageBitmap::createImageBuffer(context, size, toDestinationColorSpace(imageData.colorSpace()));
    if (!imageBuffer)
        return { };

    imageBuffer->putPixelBuffer(imageData.byteArrayPixelBuffer().get(), { { }, size }, { }, AlphaPremultiplication::Unpremultiplied);
    return { WTF::move(imageBuffer), false };
}

// A decoded image's pixels live in CoreGraphics' own decode buffer rather than an IOSurface, so they
// have to be drawn into an accelerated buffer first. Doing it this way lets CoreGraphics resolve the
// EXIF orientation, indexed palettes and the image's colour space, which is what the CPU path below
// hand-rolls. Only bitmap images come this way: a vector image has no pixels of its own, and the CPU
// path rasterizes it at the destination texture's size.
static GPUResidentSource gpuResidentSourceForImageElement(ScriptExecutionContext& context, HTMLImageElement& imageElement)
{
    RefPtr cachedImage = imageElement.cachedImage();
    if (!cachedImage)
        return { };

    RefPtr image = dynamicDowncast<BitmapImage>(cachedImage->image());
    if (!image)
        return { };

    // The source rectangle is in the image's own coordinate space, the destination in the space the
    // orientation maps it onto, which is the one the copy's origin and size are expressed in.
    auto sourceRect = FloatRect { { }, image->size(ImageOrientation::Orientation::None) };
    auto destinationRect = FloatRect { { }, image->size() };
    if (sourceRect.isEmpty() || destinationRect.isEmpty())
        return { };

    // The image is converted into sRGB rather than kept in its own colour space, because that is the
    // space copyExternalImageToTexture's destination is described relative to, and because the
    // backing queue can only convert between sRGB and Display P3 itself.
    RefPtr imageBuffer = ImageBitmap::createImageBuffer(context, destinationRect.size(), DestinationColorSpace::SRGB());
    if (!imageBuffer)
        return { };

    // CoreGraphics decodes to premultiplied alpha, so compositing over the buffer's transparent
    // black leaves the decoded channels untouched; the copy is lossless for as long as the
    // destination wants premultiplied alpha too.
    imageBuffer->context().drawImage(*image, destinationRect, sourceRect, { CompositeOperator::Copy, ImageOrientation::Orientation::FromImage });
    return { WTF::move(imageBuffer) };
}
#endif // HAVE(IOSURFACE) && ENABLE(VIDEO) && ENABLE(WEB_CODECS)

static GPUResidentSource imageBufferForSource([[maybe_unused]] ScriptExecutionContext& context, const GPUImageCopyExternalImage::SourceType& source)
{
#if !HAVE(IOSURFACE)
    UNUSED_PARAM(source);
    return { };
#else
    using ResultType = GPUResidentSource;
    auto result = WTF::switchOn(source,
        [&](const Ref<ImageBitmap>& imageBitmap) -> ResultType {
            return { imageBitmap->buffer(), imageBitmap->premultiplyAlpha() };
        },
#if ENABLE(VIDEO) && ENABLE(WEB_CODECS)
        [&](const Ref<ImageData>& imageData) -> ResultType {
            return gpuResidentSourceForImageData(context, imageData);
        },
        [&](const Ref<HTMLImageElement>& imageElement) -> ResultType {
            return gpuResidentSourceForImageElement(context, imageElement);
        },
        [&](const Ref<HTMLVideoElement>& videoElement) -> ResultType {
            return gpuResidentSourceForVideo(videoElement);
        },
        [&](const Ref<WebCodecsVideoFrame>& videoFrame) -> ResultType {
            return gpuResidentSourceForVideo(videoFrame);
        },
#endif
        [&](const Ref<HTMLCanvasElement>& canvasElement) -> ResultType {
            return { canvasElement->makeRenderingResultsAvailable(ShouldApplyPostProcessingToDirtyRect::No) };
        }
#if ENABLE(OFFSCREEN_CANVAS)
        , [&](const Ref<OffscreenCanvas>& offscreenCanvasElement) -> ResultType {
            return { offscreenCanvasElement->makeRenderingResultsAvailable(ShouldApplyPostProcessingToDirtyRect::No) };
        }
#endif
    );

    // A video source is a CVPixelBuffer rather than an ImageBuffer, so none of the checks below apply
    // to it: it is not resampled, it has no unpremultiplied channels to preserve, and the backing
    // queue converts out of its own colour space using the matrices the frame itself carries.
    if (result.videoSource)
        return result;

    RefPtr imageBuffer = result.imageBuffer;
    if (!imageBuffer || imageBuffer->renderingMode() != RenderingMode::Accelerated)
        return { };

    auto size = imageBuffer->truncatedLogicalSize();
    if (!size.width() || !size.height())
        return { };

    // The backing queue copies one surface texel per destination texel, so the surface has to be in the
    // same coordinate space as the copy. A scaled backing store is not, and resampling it is what the
    // CPU path below already does.
    if (imageBuffer->resolutionScale() != 1)
        return { };

    if (!isSupportedGPUSourcePixelFormat(imageBuffer->pixelFormat()))
        return { };

    // The backing queue converts between sRGB and Display P3 only; anything else would need a colour
    // matrix it does not have.
    auto colorSpace = imageBuffer->colorSpace();
    bool isSupportedColorSpace = colorSpace == DestinationColorSpace::SRGB();
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
    isSupportedColorSpace = isSupportedColorSpace || colorSpace == DestinationColorSpace::DisplayP3();
#endif
    if (!isSupportedColorSpace)
        return { };

    return result;
#endif // HAVE(IOSURFACE)
}

static bool isOriginClean(const auto& source, [[maybe_unused]] ScriptExecutionContext& context)
{
    using ResultType = bool;
    return WTF::switchOn(source,
        [&](const Ref<ImageBitmap>& imageBitmap) -> ResultType {
            return imageBitmap->originClean();
        },
#if ENABLE(VIDEO) && ENABLE(WEB_CODECS)
        [&](const Ref<ImageData>&) -> ResultType {
            return true;
        },
        [&](const Ref<HTMLImageElement>& imageElement) -> ResultType {
            return imageElement->originClean(*protect(context.securityOrigin()));
        },
        [&]([[maybe_unused]] const Ref<HTMLVideoElement>& videoElement) -> ResultType {
#if PLATFORM(COCOA)
            return !videoElement->taintsOrigin(*protect(context.securityOrigin()));
#else
            return true;
#endif
        },
        [&](const Ref<WebCodecsVideoFrame>&) -> ResultType {
            return true;
        },
#endif
        [&](const Ref<HTMLCanvasElement>& canvasElement) -> ResultType {
            return canvasElement->originClean();
        }
#if ENABLE(OFFSCREEN_CANVAS)
        , [&](const Ref<OffscreenCanvas>& offscreenCanvasElement) -> ResultType {
            return offscreenCanvasElement->originClean();
        }
#endif
    );
}

static GPUIntegerCoordinate dimension(const GPUExtent3D& extent3D, size_t dimension)
{
    return WTF::switchOn(extent3D, [&](const Vector<GPUIntegerCoordinate>& vector) -> GPUIntegerCoordinate {
        return dimension < vector.size() ? vector[dimension] : 0;
    }, [&](const GPUExtent3DDict& extent3D) -> GPUIntegerCoordinate {
        switch (dimension) {
        case 0:
            return extent3D.width;
        case 1:
            return extent3D.height;
        case 2:
            return extent3D.depthOrArrayLayers;
        default:
            ASSERT_NOT_REACHED();
            return 0;
        }
    });
}

static GPUIntegerCoordinate dimension(const GPUOrigin2D& origin, size_t dimension)
{
    return WTF::switchOn(origin, [&](const Vector<GPUIntegerCoordinate>& vector) -> GPUIntegerCoordinate {
        return dimension < vector.size() ? vector[dimension] : 0;
    }, [&](const GPUOrigin2DDict& origin2D) -> GPUIntegerCoordinate {
        switch (dimension) {
        case 0:
            return origin2D.x;
        case 1:
            return origin2D.y;
        default:
            ASSERT_NOT_REACHED();
            return 0;
        }
    });
}

static bool isStateValid(const auto& source, const std::optional<GPUOrigin2D>& origin, const GPUExtent3D& copySize, ExceptionCode& errorCode)
{
    using ResultType = bool;
    auto checkedHorizontalDimension = checkedSum<uint32_t>((origin ? dimension(*origin, 0) : 0), dimension(copySize, 0));
    auto checkedVerticalDimension = checkedSum<uint32_t>((origin ? dimension(*origin, 1) : 0), dimension(copySize, 1));
    auto depthDimension = dimension(copySize, 2);
    if (depthDimension > 1 || checkedHorizontalDimension.hasOverflowed() || checkedVerticalDimension.hasOverflowed()) {
        errorCode = ExceptionCode::OperationError;
        return false;
    }

    uint32_t horizontalDimension = checkedHorizontalDimension.value();
    uint32_t verticalDimension = checkedVerticalDimension.value();

    return WTF::switchOn(source,
        [&](const Ref<ImageBitmap>& imageBitmap) -> ResultType {
            if (!imageBitmap->buffer()) {
                errorCode = ExceptionCode::InvalidStateError;
                return false;
            }
            if (horizontalDimension > imageBitmap->width() || verticalDimension > imageBitmap->height()) {
                errorCode = ExceptionCode::OperationError;
                return false;
            }
            return true;
        },
#if ENABLE(VIDEO) && ENABLE(WEB_CODECS)
        [&](const Ref<ImageData>& imageData) -> ResultType {
            auto width = imageData->width();
            auto height = imageData->height();
            if (width < 0 || height < 0 || horizontalDimension > static_cast<uint32_t>(width) || verticalDimension > static_cast<uint32_t>(height)) {
                errorCode = ExceptionCode::OperationError;
                return false;
            }
            auto size = width * height;
            if (!size) {
                errorCode = ExceptionCode::InvalidStateError;
                return false;
            }
            return true;
        },
        [&](const Ref<HTMLImageElement>& imageElement) -> ResultType {
            if (!imageElement->cachedImage()) {
                errorCode = ExceptionCode::InvalidStateError;
                return false;
            }
            if (horizontalDimension > imageElement->width() || verticalDimension > imageElement->height()) {
                errorCode = ExceptionCode::OperationError;
                return false;
            }
            return true;
        },
        [&](const Ref<HTMLVideoElement>&) -> ResultType {
            return true;
        },
        [&](const Ref<WebCodecsVideoFrame>&) -> ResultType {
            return true;
        },
#endif
        [&](const Ref<HTMLCanvasElement>& canvas) -> ResultType {
            if (!canvas->renderingContext()) {
                errorCode = ExceptionCode::OperationError;
                return false;
            }
            if (canvas->renderingContext()->isPlaceholder()) {
                errorCode = ExceptionCode::InvalidStateError;
                return false;
            }
            if (horizontalDimension > canvas->width() || verticalDimension > canvas->height()) {
                errorCode = ExceptionCode::OperationError;
                return false;
            }
            return true;
        }
#if ENABLE(OFFSCREEN_CANVAS)
        , [&](const Ref<OffscreenCanvas>& offscreenCanvas) -> ResultType {
            if (offscreenCanvas->isDetached()) {
                errorCode = ExceptionCode::InvalidStateError;
                return false;
            }
            if (!offscreenCanvas->renderingContext()) {
                errorCode = ExceptionCode::OperationError;
                return false;
            }
            if (offscreenCanvas->renderingContext()->isPlaceholder()) {
                errorCode = ExceptionCode::InvalidStateError;
                return false;
            }
            if (horizontalDimension > offscreenCanvas->width() || verticalDimension > offscreenCanvas->height()) {
                errorCode = ExceptionCode::OperationError;
                return false;
            }
            return true;
        }
#endif
    );
}

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=263692 - this code should be removed, it is to unblock
// compiler <-> pipeline dependencies
#if PLATFORM(COCOA)
static uint32_t NODELETE convertRGBA8888ToRGB10A2(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    uint32_t r0 = (static_cast<uint32_t>(r) << 2) | (static_cast<uint32_t>(r) >> 6);
    uint32_t g0 = (static_cast<uint32_t>(g) << 2) | (static_cast<uint32_t>(g) >> 6);
    uint32_t b0 = (static_cast<uint32_t>(b) << 2) | (static_cast<uint32_t>(b) >> 6);
    uint32_t a0 = (static_cast<uint32_t>(a) >> 6);
    return r0 | (g0 << 10) | (b0 << 20) | (a0 << 30);
}
#endif

static void populdateXYFromOrigin(const GPUOrigin2D& origin2D, uint32_t& x, uint32_t& y)
{
    WTF::switchOn(origin2D, [&](const Vector<GPUIntegerCoordinate>& vector) {
        x = vector.size() ? vector[0] : 0;
        y = vector.size() > 1 ? vector[1] : 0;
    }, [&](const GPUOrigin2DDict& origin2D) {
        x = origin2D.x;
        y = origin2D.y;
    });
}

static MallocSpan<uint8_t> copyToDestinationFormat(std::span<const uint8_t> rgbaBytes, GPUTextureFormat format, bool& supportedFormat, size_t rows, bool flipY, bool premultiplyAlpha, const std::optional<GPUOrigin2D>& sourceOrigin)
{
    uint32_t sourceX = 0, sourceY = 0;
    if (sourceOrigin)
        populdateXYFromOrigin(*sourceOrigin, sourceX, sourceY);

#if PLATFORM(COCOA)
    auto sizeInBytes = rgbaBytes.size();
    auto flipAndPremultiply = [&](auto& data, bool flipY, bool premultiplyAlpha, auto oneValue) {
        if (!rows || (!flipY && !premultiplyAlpha))
            return;

        auto typedBytes = data.mutableSpan();
        size_t widthInElements = typedBytes.size() / rows;
        if (premultiplyAlpha) {
            RELEASE_ASSERT(!(widthInElements % 4));
            using T = decltype(oneValue);
            float invOneValue = 1.f / oneValue;
            for (size_t i = 0; i < sizeInBytes; i += 4) {
                float alpha = typedBytes[i + 3];
                typedBytes[i] = static_cast<T>(typedBytes[i] * alpha * invOneValue);
                typedBytes[i + 1] = static_cast<T>(typedBytes[i + 1] * alpha * invOneValue);
                typedBytes[i + 2] = static_cast<T>(typedBytes[i + 2] * alpha * invOneValue);
            }
        }

        if (flipY && sourceY < rows && !sourceY && !sourceX) {
            for (size_t y = 0, y0 = rows - 1 - sourceY; y < y0; ++y, --y0) {
                for (size_t x = 0; x < widthInElements; ++x)
                    std::swap(typedBytes[y0 * widthInElements + x], typedBytes[y * widthInElements + x]);
            }
        }
    };
#endif

    supportedFormat = true;
#if PLATFORM(COCOA)
    switch (format) {
    case GPUTextureFormat::R8unorm: {
        auto data = MallocSpan<uint8_t>::malloc(sizeInBytes / 4);
        if (premultiplyAlpha) {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
                data[i0] = static_cast<uint8_t>((rgbaBytes[i] * static_cast<uint32_t>(rgbaBytes[i + 3])) / 255);
        } else {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
                data[i0] = rgbaBytes[i];
        }

        flipAndPremultiply(data, flipY, false, 255);
        return data;
    }

    // 16-bit formats
    case GPUTextureFormat::R16float: {
        auto data = MallocSpan<__fp16>::malloc(sizeInBytes / 2);
        if (premultiplyAlpha) {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
                data[i0] = (rgbaBytes[i] / 255.f) * (rgbaBytes[i + 3] / 255.f);
        } else {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
                data[i0] = rgbaBytes[i] / 255.f;
        }

        flipAndPremultiply(data, flipY, false, 1.f);
        return data;
    }

    case GPUTextureFormat::Rg8unorm: {
        auto data = MallocSpan<uint8_t>::malloc(sizeInBytes / 2);
        if (premultiplyAlpha) {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, i0 += 2) {
                data[i0] = static_cast<uint8_t>((rgbaBytes[i] * static_cast<uint32_t>(rgbaBytes[i + 3])) / 255);
                data[i0 + 1] = static_cast<uint8_t>((rgbaBytes[i + 1] * static_cast<uint32_t>(rgbaBytes[i + 3])) / 255);
            }
        } else {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, i0 += 2) {
                data[i0] = rgbaBytes[i];
                data[i0 + 1] = rgbaBytes[i + 1];
            }
        }

        flipAndPremultiply(data, flipY, false, 255);
        return data;
    }

    // 32-bit formats
    case GPUTextureFormat::R32float: {
        auto data = MallocSpan<float>::malloc(sizeInBytes);
        if (premultiplyAlpha) {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
                data[i0] = (rgbaBytes[i] / 255.f) * (rgbaBytes[i + 3] / 255.f);
        } else {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
                data[i0] = rgbaBytes[i] / 255.f;
        }

        flipAndPremultiply(data, flipY, false, 1.f);
        return data;
    }

    case GPUTextureFormat::Rg16float: {
        auto data = MallocSpan<__fp16>::malloc(sizeInBytes);
        if (premultiplyAlpha) {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, i0 += 2) {
                data[i0] = (rgbaBytes[i] / 255.f) * (rgbaBytes[i + 3] / 255.f);
                data[i0 + 1] = (rgbaBytes[i + 1] / 255.f) * (rgbaBytes[i + 3] / 255.f);
            }
        } else {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, i0 += 2) {
                data[i0] = rgbaBytes[i] / 255.f;
                data[i0 + 1] = rgbaBytes[i + 1] / 255.f;
            }
        }

        flipAndPremultiply(data, flipY, false, 1.f);
        return data;
    }

    case GPUTextureFormat::Rgba8unorm:
    case GPUTextureFormat::Rgba8unormSRGB: {
        if (flipY || premultiplyAlpha || sourceX || sourceY) {
            auto data = MallocSpan<uint8_t>::malloc(sizeInBytes);
            memcpySpan(data.mutableSpan(), rgbaBytes);
            flipAndPremultiply(data, flipY, premultiplyAlpha, 255);
            return data;
        }
        return { };
    }
    case GPUTextureFormat::Bgra8unorm:
    case GPUTextureFormat::Bgra8unormSRGB: {
        auto data = MallocSpan<uint8_t>::malloc(sizeInBytes);
        for (size_t i = 0; i < sizeInBytes; i += 4) {
            data[i] = rgbaBytes[i + 2];
            data[i + 1] = rgbaBytes[i + 1];
            data[i + 2] = rgbaBytes[i];
            data[i + 3] = rgbaBytes[i + 3];
        }
        flipAndPremultiply(data, flipY, premultiplyAlpha, 255);
        return data;
    }
    case GPUTextureFormat::Rgb10a2unorm: {
        auto data = MallocSpan<uint32_t>::malloc(sizeInBytes);
        if (flipY || premultiplyAlpha || sourceX || sourceY) {
            auto copySpan = MallocSpan<uint8_t>::malloc(sizeInBytes);
            memcpySpan(copySpan.mutableSpan(), rgbaBytes);
            flipAndPremultiply(copySpan, flipY, premultiplyAlpha, 255);
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
                data[i0] = convertRGBA8888ToRGB10A2(copySpan[i], copySpan[i + 1], copySpan[i + 2], copySpan[i + 3]);
        } else {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
                data[i0] = convertRGBA8888ToRGB10A2(rgbaBytes[i], rgbaBytes[i + 1], rgbaBytes[i + 2], rgbaBytes[i + 3]);
        }

        return data;
    }

    // 64-bit formats
    case GPUTextureFormat::Rg32float: {
        auto data = MallocSpan<float>::malloc((sizeInBytes / 2) * sizeof(float));
        if (premultiplyAlpha) {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, i0 += 2) {
                data[i0] = (rgbaBytes[i] / 255.f) * (rgbaBytes[i + 3] / 255.f);
                data[i0 + 1] = (rgbaBytes[i + 1] / 255.f) * (rgbaBytes[i + 3] / 255.f);
            }
        } else {
            for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, i0 += 2) {
                data[i0] = (rgbaBytes[i] / 255.f);
                data[i0 + 1] = (rgbaBytes[i + 1] / 255.f);
            }
        }

        flipAndPremultiply(data, flipY, false, 1.f);
        return data;
    }

    case GPUTextureFormat::Rgba16float: {
        auto data = MallocSpan<__fp16>::malloc(sizeInBytes * sizeof(__fp16));
        for (size_t i = 0; i < sizeInBytes; ++i)
            data[i] = rgbaBytes[i] / 255.f;

        flipAndPremultiply(data, flipY, premultiplyAlpha, 1.f);
        return data;
    }

    // 128-bit formats
    case GPUTextureFormat::Rgba32float: {
        auto data = MallocSpan<float>::malloc(sizeInBytes * sizeof(float));
        for (size_t i = 0; i < sizeInBytes; ++i)
            data[i] = rgbaBytes[i] / 255.f;

        flipAndPremultiply(data, flipY, premultiplyAlpha, 1.f);
        return data;
    }

    // Formats which are not allowed
    case GPUTextureFormat::R8snorm:
    case GPUTextureFormat::R8uint:
    case GPUTextureFormat::R8sint:
    case GPUTextureFormat::R16unorm:
    case GPUTextureFormat::R16snorm:
    case GPUTextureFormat::R16uint:
    case GPUTextureFormat::R16sint:
    case GPUTextureFormat::Rg8snorm:
    case GPUTextureFormat::Rg8uint:
    case GPUTextureFormat::Rg8sint:
    case GPUTextureFormat::R32uint:
    case GPUTextureFormat::R32sint:
    case GPUTextureFormat::Rg16unorm:
    case GPUTextureFormat::Rg16snorm:
    case GPUTextureFormat::Rg16uint:
    case GPUTextureFormat::Rg16sint:
    case GPUTextureFormat::Rgba32uint:
    case GPUTextureFormat::Rgba32sint:
    case GPUTextureFormat::Rgba8snorm:
    case GPUTextureFormat::Rgba8uint:
    case GPUTextureFormat::Rgba8sint:
    case GPUTextureFormat::Rgb9e5ufloat:
    case GPUTextureFormat::Rgb10a2uint:
    case GPUTextureFormat::Rg11b10ufloat:
    case GPUTextureFormat::Rg32uint:
    case GPUTextureFormat::Rg32sint:
    case GPUTextureFormat::Rgba16unorm:
    case GPUTextureFormat::Rgba16snorm:
    case GPUTextureFormat::Rgba16uint:
    case GPUTextureFormat::Rgba16sint:
    case GPUTextureFormat::Stencil8:
    case GPUTextureFormat::Depth16unorm:
    case GPUTextureFormat::Depth24plus:
    case GPUTextureFormat::Depth24plusStencil8:
    case GPUTextureFormat::Depth32float:
    case GPUTextureFormat::Depth32floatStencil8:
    case GPUTextureFormat::Bc1RgbaUnorm:
    case GPUTextureFormat::Bc1RgbaUnormSRGB:
    case GPUTextureFormat::Bc2RgbaUnorm:
    case GPUTextureFormat::Bc2RgbaUnormSRGB:
    case GPUTextureFormat::Bc3RgbaUnorm:
    case GPUTextureFormat::Bc3RgbaUnormSRGB:
    case GPUTextureFormat::Bc4RUnorm:
    case GPUTextureFormat::Bc4RSnorm:
    case GPUTextureFormat::Bc5RgUnorm:
    case GPUTextureFormat::Bc5RgSnorm:
    case GPUTextureFormat::Bc6hRgbUfloat:
    case GPUTextureFormat::Bc6hRgbFloat:
    case GPUTextureFormat::Bc7RgbaUnorm:
    case GPUTextureFormat::Bc7RgbaUnormSRGB:
    case GPUTextureFormat::Etc2Rgb8unorm:
    case GPUTextureFormat::Etc2Rgb8unormSRGB:
    case GPUTextureFormat::Etc2Rgb8a1unorm:
    case GPUTextureFormat::Etc2Rgb8a1unormSRGB:
    case GPUTextureFormat::Etc2Rgba8unorm:
    case GPUTextureFormat::Etc2Rgba8unormSRGB:
    case GPUTextureFormat::EacR11unorm:
    case GPUTextureFormat::EacR11snorm:
    case GPUTextureFormat::EacRg11unorm:
    case GPUTextureFormat::EacRg11snorm:
    case GPUTextureFormat::Astc4x4Unorm:
    case GPUTextureFormat::Astc4x4UnormSRGB:
    case GPUTextureFormat::Astc5x4Unorm:
    case GPUTextureFormat::Astc5x4UnormSRGB:
    case GPUTextureFormat::Astc5x5Unorm:
    case GPUTextureFormat::Astc5x5UnormSRGB:
    case GPUTextureFormat::Astc6x5Unorm:
    case GPUTextureFormat::Astc6x5UnormSRGB:
    case GPUTextureFormat::Astc6x6Unorm:
    case GPUTextureFormat::Astc6x6UnormSRGB:
    case GPUTextureFormat::Astc8x5Unorm:
    case GPUTextureFormat::Astc8x5UnormSRGB:
    case GPUTextureFormat::Astc8x6Unorm:
    case GPUTextureFormat::Astc8x6UnormSRGB:
    case GPUTextureFormat::Astc8x8Unorm:
    case GPUTextureFormat::Astc8x8UnormSRGB:
    case GPUTextureFormat::Astc10x5Unorm:
    case GPUTextureFormat::Astc10x5UnormSRGB:
    case GPUTextureFormat::Astc10x6Unorm:
    case GPUTextureFormat::Astc10x6UnormSRGB:
    case GPUTextureFormat::Astc10x8Unorm:
    case GPUTextureFormat::Astc10x8UnormSRGB:
    case GPUTextureFormat::Astc10x10Unorm:
    case GPUTextureFormat::Astc10x10UnormSRGB:
    case GPUTextureFormat::Astc12x10Unorm:
    case GPUTextureFormat::Astc12x10UnormSRGB:
    case GPUTextureFormat::Astc12x12Unorm:
    case GPUTextureFormat::Astc12x12UnormSRGB:
        supportedFormat = false;
        return { };
    }

    return { };
#else
    UNUSED_PARAM(format);
    UNUSED_PARAM(rgbaBytes);
    UNUSED_PARAM(rows);
    UNUSED_PARAM(flipY);
    UNUSED_PARAM(premultiplyAlpha);

    return { };
#endif
}

ExceptionOr<void> GPUQueue::copyExternalImageToTexture(ScriptExecutionContext& context, const GPUImageCopyExternalImage& source, const GPUImageCopyTextureTagged& destination, const GPUExtent3D& copySize)
{
    ExceptionCode outErrorCode;
    if (!isStateValid(source.source, source.origin, copySize, outErrorCode))
        return Exception { outErrorCode, "GPUQueue.copyExternalImageToTexture: External image state is not valid"_s };

    if (!isOriginClean(source.source, context))
        return Exception { ExceptionCode::SecurityError, "GPUQueue.copyExternalImageToTexture: Cross origin external images are not allowed in WebGPU"_s };

    auto backingCopySize = convertToBacking(copySize);

    // When the source is already GPU-resident, hand it to the backing queue: the copy is performed by
    // wrapping the source's IOSurface, or the planes of a decoded video frame, in MTLTextures and
    // rendering into the destination texture, so no pixels are read back and no format conversion
    // happens on the CPU.
    if (auto gpuResidentSource = imageBufferForSource(context, source.source); gpuResidentSource.isGPUResident()) {
        // Drawing into a canvas source is still outstanding here: a 2D context hands back the buffer
        // it is drawing into untouched, and WebGLRenderingContextBase::surfaceBufferToImageBuffer
        // paints the drawing buffer into it *after* flushing it. The backing queue wraps the buffer's
        // IOSurface rather than reading its pixels, so unlike the readback below nothing else forces
        // those commands out. RemoteQueueProxy flushes again for the same reason, because there the
        // source travels by identifier on a stream unordered against the rendering backend's; this
        // call is what covers the in-process backing, which has no proxy to do it.
        if (RefPtr sourceImageBuffer = gpuResidentSource.imageBuffer)
            sourceImageBuffer->flushDrawingContext();
        m_backing->copyExternalImageToTexture(source.convertToBacking(WTF::move(gpuResidentSource.imageBuffer), gpuResidentSource.premultipliedAlpha, WTF::move(gpuResidentSource.videoSource)), destination.convertToBacking(), backingCopySize);
        return { };
    }

    bool callbackScopeIsSafe { true };
    bool needsYFlip = source.flipY;
    bool needsPremultipliedAlpha = destination.premultipliedAlpha;
    imageBytesForSource(m_backing.get(), source, destination, needsYFlip, needsPremultipliedAlpha, backingCopySize, [&](std::span<const uint8_t> imageBytes, size_t columns, size_t rows) {
        RELEASE_ASSERT(callbackScopeIsSafe);
        auto destinationTexture = destination.texture;
        auto sizeInBytes = imageBytes.size();
        if (!imageBytes.data() || !sizeInBytes || (imageBytes.size() % 4))
            return;

        bool supportedFormat;
        auto newImageBytes = copyToDestinationFormat(imageBytes, destination.texture->format(), supportedFormat, rows, needsYFlip, needsPremultipliedAlpha, source.origin);
        uint32_t sourceX = 0, sourceY = 0;
        auto widthInBytes = (newImageBytes ? newImageBytes.sizeInBytes() : sizeInBytes) / rows;
        auto channels = widthInBytes / columns;
        GPUImageDataLayout dataLayout { 0, widthInBytes, rows };

        if (source.origin && supportedFormat) {
            populdateXYFromOrigin(*source.origin, sourceX, sourceY);

            if (sourceX || sourceY) {
                RELEASE_ASSERT(newImageBytes);
                auto copySizeWidth = dimension(copySize, 0);
                auto copySizeHeight = dimension(copySize, 1);
                for (size_t y = 0; y < copySizeHeight; ++y) {
                    auto targetY = !needsYFlip ? (sourceY + y) : (sourceY + (copySizeHeight - 1 - y));
                    for (size_t x = sourceX, x0 = 0; x0 < copySizeWidth; ++x, ++x0) {
                        for (size_t c = 0; c < channels; ++c)
                            newImageBytes[y * widthInBytes + x0 * channels + c] = newImageBytes[targetY * widthInBytes + x * channels + c];
                    }
                }
                needsYFlip = false;
            }
        }

        auto copyDestination = destination.convertToBacking();

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=263692 - this code should be removed once
        // every source takes the GPU process path above. The sources which still land here are the
        // ones it turns away: an unaccelerated or scaled ImageBuffer, a 10-bit packed surface, a
        // colour space beyond sRGB and Display P3, a vector image, and every source at all on a port
        // without IOSurface. Until then a request the CPU converter cannot satisfy is made to fail
        // Metal validation, so that the destination texture is left untouched rather than written
        // with the wrong bytes.
        if (!supportedFormat || !(destinationTexture->usage() & GPUTextureUsage::RENDER_ATTACHMENT))
            copyDestination.mipLevel = INT_MAX;

        m_backing->writeTexture(copyDestination, newImageBytes ? newImageBytes.span() : imageBytes, dataLayout.convertToBacking(), backingCopySize);
    });
    callbackScopeIsSafe = false;

    return { };
}

ExceptionOr<void> GPUQueue::copyElementImageToTexture(const GPUCopyElementImageSource&, const GPUCopyElementImageDestination&)
{
    return Exception { ExceptionCode::InvalidStateError };
}

} // namespace WebCore
