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
#include "WebGPUQueueImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUBufferImpl.h"
#include "WebGPUCommandBufferImpl.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebGPUTextureImpl.h"
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/IOSurface.h>
#include <WebCore/ImageBuffer.h>
#include <WebGPU/WebGPUExt.h>
#include <wtf/BlockPtr.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(QueueImpl);

QueueImpl::QueueImpl(WebGPUPtr<WGPUQueue>&& queue, ConvertToBackingContext& convertToBackingContext)
    : m_backing(WTF::move(queue))
    , m_convertToBackingContext(convertToBackingContext)
{
}

QueueImpl::~QueueImpl() = default;

void QueueImpl::submit(Vector<Ref<WebGPU::CommandBuffer>>&& commandBuffers)
{
    auto backingCommandBuffers = commandBuffers.map([&](auto commandBuffer) {
        return protect(m_convertToBackingContext)->convertToBacking(commandBuffer);
    });

    wgpuQueueSubmit(m_backing.get(), backingCommandBuffers.size(), backingCommandBuffers.span().data());
}

static void onSubmittedWorkDoneCallback(WGPUQueueWorkDoneStatus status, void* userdata)
{
    auto block = reinterpret_cast<void(^)(WGPUQueueWorkDoneStatus)>(userdata);
    block(status);
    Block_release(block); // Block_release is matched with Block_copy below in QueueImpl::submit().
}

void QueueImpl::onSubmittedWorkDone(CompletionHandler<void()>&& callback)
{
    auto blockPtr = makeBlockPtr([callback = WTF::move(callback)](WGPUQueueWorkDoneStatus) mutable {
        callback();
    });
    wgpuQueueOnSubmittedWorkDone(m_backing.get(), &onSubmittedWorkDoneCallback, Block_copy(blockPtr.get())); // Block_copy is matched with Block_release above in onSubmittedWorkDoneCallback().
}

void QueueImpl::writeBuffer(
    const Buffer&,
    Size64,
    std::span<const uint8_t>,
    Size64,
    std::optional<Size64>)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void QueueImpl::writeTexture(
    const ImageCopyTexture&,
    std::span<const uint8_t>,
    const ImageDataLayout&,
    const Extent3D&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void QueueImpl::writeBufferNoCopy(
    const Buffer& buffer,
    Size64 bufferOffset,
    std::span<uint8_t> source,
    Size64 dataOffset,
    std::optional<Size64> size)
{
    wgpuQueueWriteBuffer(m_backing.get(), protect(m_convertToBackingContext)->convertToBacking(buffer), bufferOffset, source.subspan(dataOffset, size.value_or(source.size() - dataOffset)));
}

void QueueImpl::writeTexture(
    const ImageCopyTexture& destination,
    std::span<uint8_t> source,
    const ImageDataLayout& dataLayout,
    const Extent3D& size)
{
    Ref convertToBackingContext = m_convertToBackingContext;

    WGPUImageCopyTexture backingDestination {
        .texture = convertToBackingContext->convertToBacking(protect(destination.texture)),
        .mipLevel = destination.mipLevel,
        .origin = destination.origin ? convertToBackingContext->convertToBacking(*destination.origin) : WGPUOrigin3D { 0, 0, 0 },
        .aspect = convertToBackingContext->convertToBacking(destination.aspect),
    };

    WGPUTextureDataLayout backingDataLayout {
        .offset = dataLayout.offset,
        .bytesPerRow = dataLayout.bytesPerRow.value_or(WGPU_COPY_STRIDE_UNDEFINED),
        .rowsPerImage = dataLayout.rowsPerImage.value_or(WGPU_COPY_STRIDE_UNDEFINED),
    };

    WGPUExtent3D backingSize = convertToBackingContext->convertToBacking(size);

    wgpuQueueWriteTexture(m_backing.get(), &backingDestination, source, &backingDataLayout, &backingSize);
}

static WGPUColorSpace NODELETE convertToColorSpace(PredefinedColorSpace colorSpace)
{
    switch (colorSpace) {
    case PredefinedColorSpace::SRGB:
        return WGPUColorSpace::SRGB;
    case PredefinedColorSpace::SRGBLinear:
        return WGPUColorSpace::SRGBLinear;
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    case PredefinedColorSpace::DisplayP3:
        return WGPUColorSpace::DisplayP3;
    case PredefinedColorSpace::DisplayP3Linear:
        return WGPUColorSpace::DisplayP3Linear;
#endif
    }

    ASSERT_NOT_REACHED();
    return WGPUColorSpace::SRGB;
}

#if ENABLE(VIDEO) && PLATFORM(COCOA)
static WGPUVideoFrameRotation NODELETE convertToVideoFrameRotation(VideoFrameRotation rotation)
{
    switch (rotation) {
    case VideoFrameRotation::None:
        return WGPUVideoFrameRotation_None;
    case VideoFrameRotation::Right:
        return WGPUVideoFrameRotation_Right;
    case VideoFrameRotation::UpsideDown:
        return WGPUVideoFrameRotation_UpsideDown;
    case VideoFrameRotation::Left:
        return WGPUVideoFrameRotation_Left;
    }

    ASSERT_NOT_REACHED();
    return WGPUVideoFrameRotation_None;
}
#endif

// The IOSurface format an accelerated ImageBuffer of this pixel format is backed by, expressed as the
// equivalent WGPUTextureFormat, plus whether its alpha channel holds meaningful data. std::nullopt for
// the formats GPUQueue::copyExternalImageToTexture keeps on the CPU readback path.
struct SourceTextureFormat {
    WGPUTextureFormat format;
    bool hasAlpha;
};

static std::optional<SourceTextureFormat> NODELETE sourceTextureFormat(PixelFormat pixelFormat)
{
    switch (pixelFormat) {
    case PixelFormat::RGBA8:
        return SourceTextureFormat { WGPUTextureFormat_RGBA8Unorm, true };
    case PixelFormat::BGRA8:
        return SourceTextureFormat { WGPUTextureFormat_BGRA8Unorm, true };
    case PixelFormat::BGRX8:
        // IOSurface::Format::BGRX uses the same IOSurface pixel format as BGRA, but CoreGraphics
        // renders into it with kCGImageAlphaNoneSkipFirst, so the alpha byte is undefined.
        return SourceTextureFormat { WGPUTextureFormat_BGRA8Unorm, false };
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
        return SourceTextureFormat { WGPUTextureFormat_RGBA16Float, true };
#endif
#if ENABLE(PIXEL_FORMAT_RGB10)
    case PixelFormat::RGB10:
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case PixelFormat::RGB10A8:
#endif
#if ENABLE(PIXEL_FORMAT_RGB10) || ENABLE(PIXEL_FORMAT_RGB10A8)
        // Packed 10-bit surfaces have no single-plane MTLPixelFormat equivalent.
        return std::nullopt;
#endif
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

void QueueImpl::copyExternalImageToTexture(
    const ImageCopyExternalImage& source,
    const ImageCopyTextureTagged& destination,
    const Extent3D& copySize)
{
    Ref convertToBackingContext = m_convertToBackingContext;

    auto sourceOrigin = source.origin ? convertToBackingContext->convertToBacking(*source.origin) : WGPUOrigin3D { 0, 0, 0 };

    WGPUImageCopyExternalImage backingSource {
        .source = nullptr,
        .pixelBuffer = nullptr,
        .pixelBufferRotation = WGPUVideoFrameRotation_None,
        .pixelBufferIsMirrored = false,
        .sourceFormat = WGPUTextureFormat_Undefined,
        .originX = sourceOrigin.x,
        .originY = sourceOrigin.y,
        .sourceWidth = 0,
        .sourceHeight = 0,
        .flipY = source.flipY,
        .hasAlpha = false,
        // An ImageBitmap created with premultiplyAlpha: "none" was put into its buffer straight, so
        // the caller has to say; a buffer a 2D context composited is premultiplied.
        .premultipliedAlpha = source.premultipliedAlpha,
        .colorSpace = WGPUColorSpace::SRGB,
    };

#if ENABLE(VIDEO) && PLATFORM(COCOA)
    if (source.videoSource) {
        // The decoded frame the GPU process resolved for us. Its extent, its crop and its primaries
        // all travel with the frame, so the backing queue reads them off it rather than being told
        // here; and a decoded frame is opaque, so its alpha is replaced with 1 the way an external
        // texture's is.
        auto* pixelBuffer = std::get_if<RetainPtr<CVPixelBufferRef>>(&*source.videoSource);
        if (!pixelBuffer || !*pixelBuffer)
            return;

        backingSource.pixelBuffer = pixelBuffer->get();
        // The display transform is the one thing about the frame its pixel buffer does not carry.
        backingSource.pixelBufferRotation = convertToVideoFrameRotation(source.videoSourceRotation);
        backingSource.pixelBufferIsMirrored = source.videoSourceIsMirrored;
        backingSource.premultipliedAlpha = true;
    } else
#endif
    {
        RefPtr sourceImageBuffer = source.imageBuffer;
        if (!sourceImageBuffer)
            return;

        // Only accelerated ImageBuffers have an IOSurface to wrap in an MTLTexture. GPUQueue rejects
        // unaccelerated sources before we get here, but the backing may have been dropped since.
        auto* surface = sourceImageBuffer->surface();
        if (!surface)
            return;

        auto sourceSize = sourceImageBuffer->truncatedLogicalSize();
        if (!sourceSize.width() || !sourceSize.height())
            return;

        auto sourceFormat = sourceTextureFormat(sourceImageBuffer->pixelFormat());
        if (!sourceFormat)
            return;

        backingSource.source = surface->surface();
        backingSource.sourceFormat = sourceFormat->format;
        backingSource.sourceWidth = static_cast<uint32_t>(sourceSize.width());
        backingSource.sourceHeight = static_cast<uint32_t>(sourceSize.height());
        backingSource.hasAlpha = sourceFormat->hasAlpha;
        backingSource.colorSpace = sourceImageBuffer->colorSpace() == DestinationColorSpace::DisplayP3() ? WGPUColorSpace::DisplayP3 : WGPUColorSpace::SRGB;
    }

    WGPUImageCopyTextureTagged backingDestination {
        .texture = convertToBackingContext->convertToBacking(protect(destination.texture)),
        .mipLevel = destination.mipLevel,
        .origin = destination.origin ? convertToBackingContext->convertToBacking(*destination.origin) : WGPUOrigin3D { 0, 0, 0 },
        .aspect = convertToBackingContext->convertToBacking(destination.aspect),
        .colorSpace = convertToColorSpace(destination.colorSpace),
        .premultipliedAlpha = destination.premultipliedAlpha,
    };

    WGPUExtent3D backingCopySize = convertToBackingContext->convertToBacking(copySize);

    wgpuQueueCopyExternalImageToTexture(m_backing.get(), &backingSource, &backingDestination, &backingCopySize);
}

void QueueImpl::setLabelInternal(const String& label)
{
    wgpuQueueSetLabel(m_backing.get(), label.utf8().data());
}

RefPtr<WebCore::NativeImage> QueueImpl::getNativeImage(WebCore::VideoFrame&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
