/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "GPUBuffer.h"
#include "GPUDevice.h"
#include "GPUImageCopyExternalImage.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "JSDOMPromiseDeferred.h"
#include "OffscreenCanvas.h"
#include "PixelBuffer.h"
#include "VideoFrame.h"
#include "WebCodecsVideoFrame.h"

#if PLATFORM(COCOA)
#include "CoreVideoSoftLink.h"
#endif

namespace WebCore {

String GPUQueue::label() const
{
    return m_backing->label();
}

void GPUQueue::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

void GPUQueue::submit(Vector<RefPtr<GPUCommandBuffer>>&& commandBuffers)
{
    auto result = WTF::compactMap(commandBuffers, [](auto& commandBuffer) -> std::optional<std::reference_wrapper<WebGPU::CommandBuffer>> {
        if (commandBuffer)
            return commandBuffer->backing();
        return std::nullopt;
    });
    m_backing->submit(WTFMove(result));
}

void GPUQueue::onSubmittedWorkDone(OnSubmittedWorkDonePromise&& promise)
{
    m_backing->onSubmittedWorkDone([promise = WTFMove(promise)]() mutable {
        promise.resolve(nullptr);
    });
}

static GPUSize64 computeElementSize(const BufferSource& data)
{
    return WTF::switchOn(data.variant(),
        [&](const RefPtr<JSC::ArrayBufferView>& bufferView) {
            return static_cast<GPUSize64>(JSC::elementSize(bufferView->getType()));
        }, [&](const RefPtr<JSC::ArrayBuffer>&) {
            return static_cast<GPUSize64>(1);
        }
    );
}

ExceptionOr<void> GPUQueue::writeBuffer(
    const GPUBuffer& buffer,
    GPUSize64 bufferOffset,
    BufferSource&& data,
    std::optional<GPUSize64> optionalDataOffset,
    std::optional<GPUSize64> optionalSize)
{
    auto elementSize = computeElementSize(data);
    auto dataOffset = elementSize * optionalDataOffset.value_or(0);
    auto dataSize = data.length();
    auto contentSize = optionalSize.has_value() ? (elementSize * optionalSize.value()) : (dataSize - dataOffset);

    if (dataOffset > dataSize || dataOffset + contentSize > dataSize || (contentSize % 4))
        return Exception { ExceptionCode::OperationError };

    m_backing->writeBuffer(buffer.backing(), bufferOffset, data.data(), dataSize, dataOffset, contentSize);
    return { };
}

void GPUQueue::writeTexture(
    const GPUImageCopyTexture& destination,
    BufferSource&& data,
    const GPUImageDataLayout& imageDataLayout,
    const GPUExtent3D& size)
{
    m_backing->writeTexture(destination.convertToBacking(), data.data(), data.length(), imageDataLayout.convertToBacking(), convertToBacking(size));
}

static PixelFormat toPixelFormat(GPUTextureFormat textureFormat)
{
    switch (textureFormat) {
    case GPUTextureFormat::R8unorm:
    case GPUTextureFormat::R8snorm:
    case GPUTextureFormat::R8uint:
    case GPUTextureFormat::R8sint:
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

using ImageDataCallback = Function<void(const uint8_t*, size_t, size_t)>;
static void getImageBytesFromImageBuffer(ImageBuffer* imageBuffer, const auto& destination, ImageDataCallback&& callback)
{
    if (!imageBuffer)
        return callback(nullptr, 0, 0);

    auto size = imageBuffer->truncatedLogicalSize();
    if (!size.width() || !size.height())
        return callback(nullptr, 0, 0);

    auto pixelBuffer = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Unpremultiplied, toPixelFormat(destination.texture->format()), DestinationColorSpace::SRGB() }, { { }, size });
    ASSERT(pixelBuffer);

    callback(pixelBuffer->bytes(), pixelBuffer->sizeInBytes(), size.height());
}

#if PLATFORM(COCOA) && ENABLE(VIDEO) && ENABLE(WEB_CODECS)
static void getImageBytesFromVideoFrame(const RefPtr<VideoFrame>& videoFrame, ImageDataCallback&& callback)
{
    if (!videoFrame.get() || !videoFrame->pixelBuffer())
        return callback(nullptr, 0, 0);

    auto pixelBuffer = videoFrame->pixelBuffer();
    auto rows = CVPixelBufferGetHeight(pixelBuffer);
    auto sizeInBytes = rows * CVPixelBufferGetBytesPerRow(pixelBuffer);

    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    callback(reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer)), sizeInBytes, rows);
    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
}
#endif

static void imageBytesForSource(const auto& source, const auto& destination, ImageDataCallback&& callback)
{
    using ResultType = void;
    return WTF::switchOn(source, [&](const RefPtr<ImageBitmap>& imageBitmap) -> ResultType {
        return getImageBytesFromImageBuffer(imageBitmap->buffer(), destination, WTFMove(callback));
#if ENABLE(VIDEO) && ENABLE(WEB_CODECS)
    }, [&](const RefPtr<ImageData> imageData) -> ResultType {
        auto rows = imageData->height();
        auto pixelBuffer = imageData->pixelBuffer();
        auto sizeInBytes = pixelBuffer->data().byteLength();
        callback(pixelBuffer->data().data(), sizeInBytes, rows);
    }, [&](const RefPtr<HTMLImageElement> imageElement) -> ResultType {
#if PLATFORM(COCOA)
        if (auto* cachedImage = imageElement->cachedImage()) {
            if (auto* image = cachedImage->image(); image && image->isBitmapImage()) {
                if (auto nativeImage = static_cast<BitmapImage*>(image)->nativeImage(); nativeImage.get()) {
                    if (auto platformImage = nativeImage->platformImage()) {
                        auto pixelDataCfData = adoptCF(CGDataProviderCopyData(CGImageGetDataProvider(platformImage.get())));
                        if (!pixelDataCfData)
                            return callback(nullptr, 0, 0);

                        auto width = CGImageGetWidth(platformImage.get());
                        auto height = CGImageGetHeight(platformImage.get());
                        auto sizeInBytes = height * CGImageGetBytesPerRow(platformImage.get());
                        auto bytePointer = reinterpret_cast<const uint8_t*>(CFDataGetBytePtr(pixelDataCfData.get()));
                        auto requiredSize = width * height * 4;
                        if (sizeInBytes == requiredSize)
                            return callback(bytePointer, sizeInBytes, height);

                        auto bytesPerRow = CGImageGetBytesPerRow(platformImage.get());
                        Vector<uint8_t> tempBuffer(requiredSize, 255);
                        auto bytesPerPixel = sizeInBytes / (width * height);
                        for (size_t y = 0; y < height; ++y) {
                            for (size_t x = 0; x < width; ++x) {
                                for (size_t c = 0; c < bytesPerPixel; ++c)
                                    tempBuffer[y * (width * 4) + x * 4 + c] = bytePointer[y * bytesPerRow + x * bytesPerPixel + c];
                            }
                        }
                        callback(&tempBuffer[0], tempBuffer.size(), height);
                    }
                }
            }
        }
#else
        UNUSED_PARAM(imageElement);
#endif
        return callback(nullptr, 0, 0);
    }, [&](const RefPtr<HTMLVideoElement> videoElement) -> ResultType {
#if PLATFORM(COCOA)
        if (auto player = videoElement->player(); player->isVideoPlayer())
            return getImageBytesFromVideoFrame(player->videoFrameForCurrentTime(), WTFMove(callback));
#endif
        UNUSED_PARAM(videoElement);
        return callback(nullptr, 0, 0);
    }, [&](const RefPtr<WebCodecsVideoFrame> webCodecsFrame) -> ResultType {
#if PLATFORM(COCOA)
        return getImageBytesFromVideoFrame(webCodecsFrame->internalFrame(), WTFMove(callback));
#else
        UNUSED_PARAM(webCodecsFrame);
        return callback(nullptr, 0, 0);
#endif
#endif
    }, [&](const RefPtr<HTMLCanvasElement>& canvasElement) -> ResultType {
        return getImageBytesFromImageBuffer(canvasElement->buffer(), destination, WTFMove(callback));
    }
#if ENABLE(OFFSCREEN_CANVAS)
    , [&](const RefPtr<OffscreenCanvas>& offscreenCanvasElement) -> ResultType {
        return getImageBytesFromImageBuffer(offscreenCanvasElement->buffer(), destination, WTFMove(callback));
    }
#endif
    );
}

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=263692 - this code should be removed, it is to unblock
// compiler <-> pipeline dependencies
static void* copyToDestinationFormat(const uint8_t* rgbaBytes, GPUTextureFormat format, size_t& sizeInBytes)
{
#if PLATFORM(COCOA)
    switch (format) {
    case GPUTextureFormat::R8unorm:
    case GPUTextureFormat::R8snorm:
    case GPUTextureFormat::R8uint:
    case GPUTextureFormat::R8sint: {
        uint8_t* data = (uint8_t*)malloc(sizeInBytes / 4);
        for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
            data[i0] = rgbaBytes[i];

        sizeInBytes = sizeInBytes / 4;
        return data;
    }

    // 16-bit formats
    case GPUTextureFormat::R16uint:
    case GPUTextureFormat::R16sint: {
        uint16_t* data = (uint16_t*)malloc(sizeInBytes / 2);
        for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
            data[i0] = rgbaBytes[i];

        sizeInBytes = sizeInBytes / 2;
        return data;
    }

    case GPUTextureFormat::R16float: {
        __fp16* data = (__fp16*)malloc(sizeInBytes / 2);
        for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
            data[i0] = rgbaBytes[i] / 255.f;

        sizeInBytes = sizeInBytes / 2;
        return data;
    }

    case GPUTextureFormat::Rg8unorm:
    case GPUTextureFormat::Rg8snorm:
    case GPUTextureFormat::Rg8uint:
    case GPUTextureFormat::Rg8sint: {
        uint8_t* data = (uint8_t*)malloc(sizeInBytes / 2);
        for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 2, ++i0) {
            data[i0] = rgbaBytes[i];
            data[i0 + 1] = rgbaBytes[i + 1];
        }

        sizeInBytes = sizeInBytes / 2;
        return data;
    }

    // 32-bit formats
    case GPUTextureFormat::R32uint:
    case GPUTextureFormat::R32sint: {
        uint32_t* data = (uint32_t*)malloc(sizeInBytes);
        for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
            data[i0] = rgbaBytes[i];

        return data;
    }

    case GPUTextureFormat::R32float: {
        float* data = (float*)malloc(sizeInBytes);
        for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, ++i0)
            data[i0] = rgbaBytes[i] / 255.f;

        return data;
    }

    case GPUTextureFormat::Rg16uint:
    case GPUTextureFormat::Rg16sint: {
        uint16_t* data = (uint16_t*)malloc(sizeInBytes);
        for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, i0 += 2) {
            data[i0] = rgbaBytes[i];
            data[i0 + 1] = rgbaBytes[i + 1];
        }

        return data;
    }

    case GPUTextureFormat::Rg16float: {
        __fp16* data = (__fp16*)malloc(sizeInBytes);
        for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, i0 += 2) {
            data[i0] = rgbaBytes[i] / 255.f;
            data[i0 + 1] = rgbaBytes[i + 1] / 255.f;
        }

        return data;
    }

    case GPUTextureFormat::Rgba8unorm:
    case GPUTextureFormat::Rgba8unormSRGB:
    case GPUTextureFormat::Rgba8snorm:
    case GPUTextureFormat::Rgba8uint:
    case GPUTextureFormat::Rgba8sint:
    case GPUTextureFormat::Bgra8unorm:
    case GPUTextureFormat::Bgra8unormSRGB:
    case GPUTextureFormat::Rgb9e5ufloat:
    case GPUTextureFormat::Rgb10a2uint:
    case GPUTextureFormat::Rg11b10ufloat:
        return nullptr;
    case GPUTextureFormat::Rgb10a2unorm: {
        ASSERT_NOT_REACHED("Remapping to 10 bits per channel is not implemented");
        return nullptr;
    }

    // 64-bit formats
    case GPUTextureFormat::Rg32uint:
    case GPUTextureFormat::Rg32sint: {
        uint32_t* data = (uint32_t*)malloc((sizeInBytes / 2) * sizeof(uint32_t));
        for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, i0 += 2) {
            data[i0] = rgbaBytes[i];
            data[i0 + 1] = rgbaBytes[i + 1];
        }

        sizeInBytes = sizeInBytes * sizeof(uint32_t);
        return data;
    }

    case GPUTextureFormat::Rg32float: {
        float* data = (float*)malloc((sizeInBytes / 2) * sizeof(float));
        for (size_t i = 0, i0 = 0; i < sizeInBytes; i += 4, i0 += 2) {
            data[i0] = rgbaBytes[i];
            data[i0 + 1] = rgbaBytes[i + 1];
        }

        sizeInBytes = sizeInBytes * sizeof(float);
        return data;
    }

    case GPUTextureFormat::Rgba16uint:
    case GPUTextureFormat::Rgba16sint: {
        uint16_t* data = (uint16_t*)malloc(sizeInBytes * sizeof(uint16_t));
        for (size_t i = 0; i < sizeInBytes; ++i)
            data[i] = rgbaBytes[i];

        sizeInBytes = sizeInBytes * sizeof(uint16_t);
        return data;
    }

    case GPUTextureFormat::Rgba16float: {
        __fp16* data = (__fp16*)malloc(sizeInBytes * sizeof(__fp16));
        for (size_t i = 0; i < sizeInBytes; ++i)
            data[i] = rgbaBytes[i] / 255.f;

        sizeInBytes = sizeInBytes * sizeof(__fp16);
        return data;
    }

    // 128-bit formats
    case GPUTextureFormat::Rgba32uint:
    case GPUTextureFormat::Rgba32sint: {
        uint32_t* data = (uint32_t*)malloc(sizeInBytes * sizeof(uint32_t));
        for (size_t i = 0; i < sizeInBytes; ++i)
            data[i] = rgbaBytes[i];

        sizeInBytes = sizeInBytes * sizeof(uint32_t);
        return data;
    }

    case GPUTextureFormat::Rgba32float: {
        float* data = (float*)malloc(sizeInBytes * sizeof(float));
        for (size_t i = 0; i < sizeInBytes; ++i)
            data[i] = rgbaBytes[i] / 255.f;

        sizeInBytes = sizeInBytes * sizeof(float);
        return data;
    }

    // Depth/stencil formats
    case GPUTextureFormat::Stencil8:
    case GPUTextureFormat::Depth16unorm:
    case GPUTextureFormat::Depth24plus:
    case GPUTextureFormat::Depth24plusStencil8:
    case GPUTextureFormat::Depth32float:

        // depth32float-stencil8 feature
    case GPUTextureFormat::Depth32floatStencil8:

    // BC compressed formats usable if texture-compression-bc is both
    // supported by the device/user agent and enabled in requestDevice.
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

    // ETC2 compressed formats usable if texture-compression-etc2 is both
    // supported by the device/user agent and enabled in requestDevice.
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

    // ASTC compressed formats usable if texture-compression-astc is both
    // supported by the device/user agent and enabled in requestDevice.
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
        return nullptr;
    }

    return nullptr;
#else
    UNUSED_PARAM(format);
    UNUSED_PARAM(sizeInBytes);
    UNUSED_PARAM(rgbaBytes);

    return nullptr;
#endif
}

void GPUQueue::copyExternalImageToTexture(const GPUImageCopyExternalImage& source, const GPUImageCopyTextureTagged& destination, const GPUExtent3D& copySize)
{
    imageBytesForSource(source.source, destination, [&](const uint8_t* imageBytes, size_t sizeInBytes, size_t rows) {
        if (!imageBytes || !sizeInBytes || !destination.texture)
            return;

        auto* newImageBytes = copyToDestinationFormat(imageBytes, destination.texture->format(), sizeInBytes);
        GPUImageDataLayout dataLayout { 0, sizeInBytes / rows, rows };
        m_backing->writeTexture(destination.convertToBacking(), newImageBytes ? newImageBytes : imageBytes, sizeInBytes, dataLayout.convertToBacking(), convertToBacking(copySize));
        if (newImageBytes)
            free(newImageBytes);
    });
}

} // namespace WebCore
