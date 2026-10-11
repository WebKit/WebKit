/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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
#include "WebGPUImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUCompositorIntegrationImpl.h"
#include "WebGPUExternalTextureDescriptor.h"
#include "WebGPUImageCopyExternalImage.h"
#include "WebGPUImageCopyTextureTagged.h"
#include "WebGPUPresentationContextDescriptor.h"
#include <WebCore/ColorSpace.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/IOSurface.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/IntSize.h>
#include <WebCore/NativeImage.h>
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/BlockPtr.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebGPUIntegrationImpl);

WebGPUIntegrationImpl::WebGPUIntegrationImpl(Ref<::WebGPU::Instance>&& instance)
    : m_backing(WTF::move(instance))
{
}

WebGPUIntegrationImpl::~WebGPUIntegrationImpl() = default;

void WebGPUIntegrationImpl::requestAdapter(const WebGPU::RequestAdapterOptions& options, CompletionHandler<void(RefPtr<WebGPU::Adapter>&&)>&& callback)
{
    auto backingOptions = options;
#if CPU(X86_64)
    backingOptions.powerPreference = WebGPU::PowerPreference::HighPerformance;
#endif
    m_backing->requestAdapter(backingOptions, WTF::move(callback));
}

RefPtr<WebGPU::PresentationContext> WebGPUIntegrationImpl::createPresentationContext(const WebGPUPresentationContextDescriptor& presentationContextDescriptor)
{
    // Every WebCore::WebGPUCompositorIntegration that WebGPUIntegrationImpl creates is a WebGPUCompositorIntegrationImpl.
    Ref compositorIntegration = downcast<WebGPUCompositorIntegrationImpl>(presentationContextDescriptor.compositorIntegration.get());

    RefPtr result = m_backing->createPresentationContext({
        .registerCompositorIntegration = [&](auto&& renderBuffersWereRecreated, auto&& onSubmittedWorkScheduled) {
            compositorIntegration->registerCallbacks(WTF::move(renderBuffersWereRecreated), WTF::move(onSubmittedWorkScheduled));
        },
    });
    if (result)
        compositorIntegration->setPresentationContext(*result);
    return result;
}

RefPtr<WebGPUCompositorIntegration> WebGPUIntegrationImpl::createCompositorIntegration()
{
    return WebGPUCompositorIntegrationImpl::create();
}

#if ENABLE(VIDEO)
static ::WebGPU::VideoFrameRotation NODELETE convertToAPI(VideoFrameRotation rotation)
{
    switch (rotation) {
    case VideoFrameRotation::None:
        return ::WebGPU::VideoFrameRotation::None;
    case VideoFrameRotation::Right:
        return ::WebGPU::VideoFrameRotation::Right;
    case VideoFrameRotation::UpsideDown:
        return ::WebGPU::VideoFrameRotation::UpsideDown;
    case VideoFrameRotation::Left:
        return ::WebGPU::VideoFrameRotation::Left;
    }

    ASSERT_NOT_REACHED();
    return ::WebGPU::VideoFrameRotation::None;
}
#endif

// The IOSurface format an accelerated ImageBuffer of this pixel format is backed by, expressed as the
// equivalent texture format, plus whether its alpha channel holds meaningful data. std::nullopt for
// the formats GPUQueue::copyExternalImageToTexture keeps on the CPU readback path.
struct SourceTextureFormat {
    ::WebGPU::TextureFormat format;
    bool hasAlpha;
};

static std::optional<SourceTextureFormat> NODELETE sourceTextureFormat(PixelFormat pixelFormat)
{
    switch (pixelFormat) {
    case PixelFormat::RGBA8:
        return SourceTextureFormat { ::WebGPU::TextureFormat::Rgba8unorm, true };
    case PixelFormat::BGRA8:
        return SourceTextureFormat { ::WebGPU::TextureFormat::Bgra8unorm, true };
    case PixelFormat::BGRX8:
        // IOSurface::Format::BGRX uses the same IOSurface pixel format as BGRA, but CoreGraphics
        // renders into it with kCGImageAlphaNoneSkipFirst, so the alpha byte is undefined.
        return SourceTextureFormat { ::WebGPU::TextureFormat::Bgra8unorm, false };
    case PixelFormat::RGBX8:
        return SourceTextureFormat { ::WebGPU::TextureFormat::Rgba8unorm, false };
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
        return SourceTextureFormat { ::WebGPU::TextureFormat::Rgba16float, true };
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16)
    case PixelFormat::RGBA16:
        return SourceTextureFormat { ::WebGPU::TextureFormat::Rgba16unorm, true };
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

void WebGPUIntegrationImpl::copyExternalImageToTexture(WebGPU::Queue& queue, const WebGPUExternalImageSource& source, const WebGPUImageCopyTextureTagged& destination, const WebGPU::Extent3D& copySize)
{
    ::WebGPU::ImageCopyExternalImage backingSource;
    backingSource.origin = source.origin.value_or(WebGPU::Origin2D { });
    backingSource.flipY = source.flipY;
    backingSource.hasAlpha = false;
    // An ImageBitmap created with premultiplyAlpha: "none" was put into its buffer straight, so
    // the caller has to say; a buffer a 2D context composited is premultiplied.
    backingSource.premultipliedAlpha = source.premultipliedAlpha;

#if ENABLE(VIDEO)
    if (source.videoSource) {
        // The decoded frame the GPU process resolved for us. Its extent, its crop and its primaries
        // all travel with the frame, so the backing queue reads them off it rather than being told
        // here; and a decoded frame is opaque, so its alpha is replaced with 1 the way an external
        // texture's is.
        auto* pixelBuffer = std::get_if<RetainPtr<CVPixelBufferRef>>(&*source.videoSource);
        if (!pixelBuffer || !*pixelBuffer)
            return;

        backingSource.pixelBuffer = *pixelBuffer;
        // The display transform is the one thing about the frame its pixel buffer does not carry.
        backingSource.pixelBufferRotation = convertToAPI(source.videoSourceRotation);
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
        backingSource.sourceSize = { static_cast<uint32_t>(sourceSize.width()), static_cast<uint32_t>(sourceSize.height()) };
        backingSource.hasAlpha = sourceFormat->hasAlpha;
        backingSource.colorSpace = sourceImageBuffer->colorSpace() == ColorSpace::DisplayP3() ? ::WebGPU::PredefinedColorSpace::DisplayP3 : ::WebGPU::PredefinedColorSpace::SRGB;
    }

    queue.copyExternalImageToTexture(backingSource, {
        .texture = destination.texture,
        .mipLevel = destination.mipLevel,
        .origin = destination.origin,
        .aspect = destination.aspect,
        .colorSpace = convertToWebGPU(destination.colorSpace),
        .premultipliedAlpha = destination.premultipliedAlpha,
    }, copySize);
}

RefPtr<WebCore::NativeImage> WebGPUIntegrationImpl::nativeImage(WebGPU::Queue&, WebCore::VideoFrame&)
{
    // Only RemoteGPUProxy resolves a video frame to an image, through its video frame object heap.
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebGPU::ExternalTexture> WebGPUIntegrationImpl::importExternalTexture(WebGPU::Device& device, const WebGPUExternalTextureDescriptor& descriptor)
{
    auto* pixelBuffer = std::get_if<RetainPtr<CVPixelBufferRef>>(&descriptor.videoBacking);
    return device.importExternalTexture({
        .label = descriptor.label,
        .pixelBuffer = pixelBuffer ? *pixelBuffer : nullptr,
        .colorSpace = convertToWebGPU(descriptor.colorSpace),
        .visibleSize = {
            .width = static_cast<uint32_t>(std::max(0, descriptor.visibleSize.width())),
            .height = static_cast<uint32_t>(std::max(0, descriptor.visibleSize.height())),
        },
    });
}

#if PLATFORM(COCOA) && ENABLE(VIDEO)
void WebGPUIntegrationImpl::updateExternalTexture(WebGPU::Device&, const WebGPU::ExternalTexture&, const WebCore::MediaPlayerIdentifier&)
{
    // Only RemoteGPUProxy names a media player; the GPU process resolves it to a pixel buffer.
    RELEASE_ASSERT_NOT_REACHED();
}
#endif

void WebGPUIntegrationImpl::paintToCanvas(WebCore::NativeImage& image, const WebCore::IntSize& canvasSize, WebCore::GraphicsContext& context)
{
    auto imageSize = image.size();
    FloatRect canvasRect(FloatPoint(), canvasSize);
    GraphicsContextStateSaver stateSaver(context);
    context.setImageInterpolationQuality(InterpolationQuality::DoNotInterpolate);
    context.drawNativeImage(image, canvasRect, FloatRect(FloatPoint(), imageSize), { CompositeOperator::Copy });
}

bool WebGPUIntegrationImpl::isValid(const WebGPUCompositorIntegration&) const
{
    return true;
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::Buffer& buffer) const
{
    return buffer.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::Adapter& adapter) const
{
    return adapter.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::BindGroup& bindGroup) const
{
    return bindGroup.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::BindGroupLayout& bindGroupLayout) const
{
    return bindGroupLayout.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::CommandBuffer& commandBuffer) const
{
    return commandBuffer.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::CommandEncoder& commandEncoder) const
{
    return commandEncoder.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::ComputePassEncoder& computePassEncoder) const
{
    return computePassEncoder.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::ComputePipeline& computePipeline) const
{
    return computePipeline.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::Device& device) const
{
    return device.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::ExternalTexture& externalTexture) const
{
    return externalTexture.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::PipelineLayout& pipelineLayout) const
{
    return pipelineLayout.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::PresentationContext& presentationContext) const
{
    return presentationContext.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::QuerySet& querySet) const
{
    return querySet.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::Queue& queue) const
{
    return queue.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::RenderBundleEncoder& renderBundleEncoder) const
{
    return renderBundleEncoder.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::RenderBundle& renderBundle) const
{
    return renderBundle.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::RenderPassEncoder& renderPassEncoder) const
{
    return renderPassEncoder.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::RenderPipeline& renderPipeline) const
{
    return renderPipeline.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::Sampler& sampler) const
{
    return sampler.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::ShaderModule& shaderModule) const
{
    return shaderModule.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::Texture& texture) const
{
    return texture.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::TextureView& textureView) const
{
    return textureView.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::XRBinding& binding) const
{
    return binding.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::XRSubImage& subImage) const
{
    return subImage.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::XRProjectionLayer& layer) const
{
    return layer.isValid();
}

bool WebGPUIntegrationImpl::isValid(const WebGPU::XRView& view) const
{
    return view.isValid();
}

} // namespace WebCore

#endif // HAVE(WEBGPU_IMPLEMENTATION)
