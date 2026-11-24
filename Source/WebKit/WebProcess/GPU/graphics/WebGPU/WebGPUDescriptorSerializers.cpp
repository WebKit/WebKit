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
#include "WebGPUDescriptorSerializers.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUBindGroupDescriptor.h"
#include "WebGPUBindGroupEntry.h"
#include "WebGPUBindGroupLayoutDescriptor.h"
#include "WebGPUBufferBinding.h"
#include "WebGPUCanvasConfiguration.h"
#include "WebGPUComputePassDescriptor.h"
#include "WebGPUComputePipelineDescriptor.h"
#include "WebGPUDeviceDescriptor.h"
#include "WebGPUExternalTextureDescriptor.h"
#include "WebGPUFragmentState.h"
#include "WebGPUImageCopyBuffer.h"
#include "WebGPUImageCopyExternalImage.h"
#include "WebGPUImageCopyTexture.h"
#include "WebGPUImageCopyTextureTagged.h"
#include "WebGPUObjectDescriptorBase.h"
#include "WebGPUPipelineLayoutDescriptor.h"
#include "WebGPUPresentationContextDescriptor.h"
#include "WebGPUProgrammableStage.h"
#include "WebGPUProxyIdentifiers.h"
#include "WebGPURenderBundleEncoderDescriptor.h"
#include "WebGPURenderPassColorAttachment.h"
#include "WebGPURenderPassDepthStencilAttachment.h"
#include "WebGPURenderPassDescriptor.h"
#include "WebGPURenderPassTimestampWrites.h"
#include "WebGPURenderPipelineDescriptor.h"
#include "WebGPUShaderModuleDescriptor.h"
#include "WebGPUTextureDescriptor.h"
#include "WebGPUVertexBufferLayout.h"
#include "WebGPUVertexState.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/WebGPUCanvasConfiguration.h>
#include <WebCore/WebGPUComputePipelineDescriptor.h>
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUDevice.h>
#include <WebCore/WebGPUDeviceDescriptor.h>
#include <WebCore/WebGPUExternalTextureDescriptor.h>
#include <WebCore/WebGPUFragmentState.h>
#include <WebCore/WebGPUImageCopyExternalImage.h>
#include <WebCore/WebGPUImageCopyTextureTagged.h>
#include <WebCore/WebGPUObjectDescriptorBase.h>
#include <WebCore/WebGPUPresentationContextDescriptor.h>
#include <WebCore/WebGPUProgrammableStage.h>
#include <WebCore/WebGPURenderBundleEncoderDescriptor.h>
#include <WebCore/WebGPURenderPipelineDescriptor.h>
#include <WebCore/WebGPUShaderModuleDescriptor.h>
#include <WebCore/WebGPUVertexBufferLayout.h>
#include <WebCore/WebGPUVertexState.h>

namespace WebKit::WebGPU {

std::optional<BindGroupDescriptor> serialize(const ::WebGPU::BindGroupDescriptor& bindGroupDescriptor)
{
    auto layout = identifier(protect(bindGroupDescriptor.layout).get());

    Vector<BindGroupEntry> entries(ReserveCapacityTag { }, bindGroupDescriptor.entries.size());
    for (const auto& entry : bindGroupDescriptor.entries) {
        auto convertedEntry = serialize(entry);
        if (!convertedEntry)
            return std::nullopt;
        entries.append(WTF::move(*convertedEntry));
    }

    return { { { bindGroupDescriptor.label }, layout, WTF::move(entries) } };
}

std::optional<BindGroupEntry> serialize(const ::WebGPU::BindGroupEntry& bindGroupEntry)
{
    return WTF::switchOn(bindGroupEntry.resource, [&](const Ref<::WebGPU::Sampler>& sampler) -> std::optional<BindGroupEntry> {
        return { { bindGroupEntry.binding, { identifier(protect(sampler.get()).get()) } } };
    }, [&](const Ref<::WebGPU::Texture>& texture) -> std::optional<BindGroupEntry> {
        return { { bindGroupEntry.binding, { identifier(protect(texture.get()).get()) } } };
    }, [&](const Ref<::WebGPU::TextureView>& textureView) -> std::optional<BindGroupEntry> {
        return { { bindGroupEntry.binding, { identifier(protect(textureView.get()).get()) } } };
    }, [&](const auto& bufferBinding) -> std::optional<BindGroupEntry> {
        auto convertedBufferBinding = serialize(bufferBinding);
        if (!convertedBufferBinding)
            return std::nullopt;

        return { { bindGroupEntry.binding, { WTF::move(*convertedBufferBinding) } } };
    }, [&](const Ref<::WebGPU::ExternalTexture>& externalTexture) -> std::optional<BindGroupEntry> {
        return { { bindGroupEntry.binding, { identifier(protect(externalTexture.get()).get()) } } };
    });
}

std::optional<BindGroupLayoutDescriptor> serialize(const ::WebGPU::BindGroupLayoutDescriptor& bindGroupLayoutDescriptor)
{
    return { { { bindGroupLayoutDescriptor.label }, Vector<BindGroupLayoutEntry> { bindGroupLayoutDescriptor.entries } } };
}

std::optional<BufferBinding> serialize(const ::WebGPU::BufferBinding& bufferBinding)
{
    auto buffer = identifier(protect(bufferBinding.buffer).get());

    return { { buffer, bufferBinding.offset, bufferBinding.size } };
}

std::optional<CanvasConfiguration> serialize(const ::WebGPU::CanvasConfiguration& canvasConfiguration)
{
    auto device = identifier(canvasConfiguration.device.get());
    return { { device, canvasConfiguration.format, canvasConfiguration.usage, Vector<::WebGPU::TextureFormat> { canvasConfiguration.viewFormats }, WebCore::convertFromWebGPU(canvasConfiguration.colorSpace), canvasConfiguration.toneMappingMode, canvasConfiguration.compositingAlphaMode, canvasConfiguration.reportValidationErrors, canvasConfiguration.width, canvasConfiguration.height } };
}

std::optional<ComputePassDescriptor> serialize(const ::WebGPU::ComputePassDescriptor& computePassDescriptor)
{
    auto timestampWrites = computePassDescriptor.timestampWrites ? serialize(*computePassDescriptor.timestampWrites) : std::nullopt;

    return { { { computePassDescriptor.label }, timestampWrites } };
}

std::optional<ComputePipelineDescriptor> serialize(const ::WebGPU::ComputePipelineDescriptor& computePipelineDescriptor)
{
    std::optional<WebGPUPipelineLayoutIdentifier> layout;
    if (computePipelineDescriptor.layout)
        layout = identifier(*protect(computePipelineDescriptor.layout));

    auto compute = serialize(computePipelineDescriptor.compute);
    if (!compute)
        return std::nullopt;

    return { { { { computePipelineDescriptor.label }, layout }, WTF::move(*compute) } };
}

std::optional<DeviceDescriptor> serialize(const ::WebGPU::DeviceDescriptor& deviceDescriptor)
{
    return { { { deviceDescriptor.label }, Vector<::WebGPU::FeatureName> { deviceDescriptor.requiredFeatures }, deviceDescriptor.requiredLimits } };
}

std::optional<ExternalTextureDescriptor> serialize(const WebCore::WebGPUExternalTextureDescriptor& externalTextureDescriptor)
{
    auto base = serialize(static_cast<const WebCore::WebGPUObjectDescriptorBase&>(externalTextureDescriptor));
    if (!base)
        return std::nullopt;

    std::optional<WebCore::MediaPlayerIdentifier> optionalMediaIdentifier { std::nullopt };
    if (auto* mediaIdentifier = std::get_if<std::optional<WebCore::MediaPlayerIdentifier>>(&externalTextureDescriptor.videoBacking))
        optionalMediaIdentifier = *mediaIdentifier;
    return { { WTF::move(*base), optionalMediaIdentifier, externalTextureDescriptor.colorSpace, externalTextureDescriptor.visibleSize
#if PLATFORM(COCOA) && ENABLE(VIDEO)
        , std::nullopt
#endif
    } };
}

std::optional<FragmentState> serialize(const ::WebGPU::FragmentState& fragmentState)
{
    auto base = serialize(fragmentState.stage);
    if (!base)
        return std::nullopt;

    return { { WTF::move(*base), Vector<std::optional<ColorTargetState>> { fragmentState.targets } } };
}

std::optional<ImageCopyBuffer> serialize(const ::WebGPU::TexelCopyBufferInfo& imageCopyBuffer)
{
    auto buffer = identifier(protect(imageCopyBuffer.buffer).get());

    return { { imageCopyBuffer.layout, buffer } };
}

std::optional<ImageCopyExternalImage> serialize(const WebCore::WebGPUExternalImageSource& imageCopyExternalImage)
{
    return { { imageCopyExternalImage.origin, imageCopyExternalImage.flipY, imageCopyExternalImage.imageBuffer ? std::optional { imageCopyExternalImage.imageBuffer->renderingResourceIdentifier() } : std::nullopt, imageCopyExternalImage.premultipliedAlpha } };
}

#if PLATFORM(COCOA) && ENABLE(VIDEO)
std::optional<ImageCopyExternalImageVideoSource> serializeVideoSource(const WebCore::WebGPUExternalImageSource& imageCopyExternalImage)
{
    ASSERT(imageCopyExternalImage.videoSource);
    if (!imageCopyExternalImage.videoSource)
        return std::nullopt;

    std::optional<WebCore::MediaPlayerIdentifier> mediaIdentifier;
    if (auto* identifier = std::get_if<std::optional<WebCore::MediaPlayerIdentifier>>(&*imageCopyExternalImage.videoSource))
        mediaIdentifier = *identifier;

    // A frame which has no media player behind it has to travel through the shared video frame
    // memory instead, which only RemoteQueueProxy can write to, so it fills sharedFrame in.
    return { { imageCopyExternalImage.origin, imageCopyExternalImage.flipY, mediaIdentifier, imageCopyExternalImage.videoSourceSize, std::nullopt } };
}
#endif

std::optional<ImageCopyTexture> serialize(const ::WebGPU::TexelCopyTextureInfo& imageCopyTexture)
{
    auto texture = identifier(protect(imageCopyTexture.texture).get());

    return { { texture, imageCopyTexture.mipLevel, imageCopyTexture.origin, imageCopyTexture.aspect } };
}

std::optional<ImageCopyTextureTagged> serialize(const WebCore::WebGPUImageCopyTextureTagged& imageCopyTextureTagged)
{
    auto base = serialize(static_cast<const ::WebGPU::TexelCopyTextureInfo&>(imageCopyTextureTagged));
    if (!base)
        return std::nullopt;

    return { { WTF::move(*base), imageCopyTextureTagged.colorSpace, imageCopyTextureTagged.premultipliedAlpha } };
}

std::optional<ObjectDescriptorBase> serialize(const WebCore::WebGPUObjectDescriptorBase& objectDescriptorBase)
{
    return { { objectDescriptorBase.label } };
}

std::optional<PipelineLayoutDescriptor> serialize(const ::WebGPU::PipelineLayoutDescriptor& pipelineLayoutDescriptor)
{
    auto bindGroupLayouts = WTF::map(pipelineLayoutDescriptor.bindGroupLayouts, [&](auto& backingBindGroupLayout) {
        return identifier(backingBindGroupLayout.get());
    });

    return { { { pipelineLayoutDescriptor.label }, WTF::move(bindGroupLayouts), pipelineLayoutDescriptor.isAutogenerated } };
}

std::optional<PresentationContextDescriptor> serialize(const WebCore::WebGPUPresentationContextDescriptor& presentationContextDescriptor)
{
    auto compositorIntegration = identifier(protect(presentationContextDescriptor.compositorIntegration));

    return { { compositorIntegration } };
}

std::optional<ProgrammableStage> serialize(const ::WebGPU::ProgrammableStage& programmableStage)
{
    auto module = identifier(protect(programmableStage.module).get());
    auto constants = WTF::map(programmableStage.constants, [](auto& constant) {
        return makeKeyValuePair(constant.key, constant.value);
    });
    return { { module, programmableStage.entryPoint, WTF::move(constants) } };
}

std::optional<RenderBundleEncoderDescriptor> serialize(const ::WebGPU::RenderBundleEncoderDescriptor& renderBundleEncoderDescriptor)
{
    return { { { { renderBundleEncoderDescriptor.label }, Vector<std::optional<::WebGPU::TextureFormat>> { renderBundleEncoderDescriptor.colorFormats }, renderBundleEncoderDescriptor.depthStencilFormat, renderBundleEncoderDescriptor.sampleCount }, renderBundleEncoderDescriptor.depthReadOnly, renderBundleEncoderDescriptor.stencilReadOnly } };
}

static WebGPURenderPassAttachmentViewIdentifier colorAttachmentViewIdentifier(const ::WebGPU::RenderPassAttachmentView& view)
{
    // RenderPassAttachmentView is a std::variant, see WebGPUCpp.h.
    if (auto* textureView = std::get_if<Ref<::WebGPU::TextureView>>(&view))
        return identifier(textureView->get());
    return identifier(std::get_if<Ref<::WebGPU::Texture>>(&view)->get());
}

std::optional<RenderPassColorAttachment> serialize(const ::WebGPU::RenderPassColorAttachment& renderPassColorAttachment)
{
    auto identifier = colorAttachmentViewIdentifier(renderPassColorAttachment.view);

    std::optional<WebGPURenderPassAttachmentViewIdentifier> resolveTarget;
    if (renderPassColorAttachment.resolveTarget)
        resolveTarget = colorAttachmentViewIdentifier(*renderPassColorAttachment.resolveTarget);

    return { { identifier, renderPassColorAttachment.depthSlice, resolveTarget, renderPassColorAttachment.clearValue, renderPassColorAttachment.loadOp, renderPassColorAttachment.storeOp } };
}

static WebGPURenderPassAttachmentViewIdentifier depthStencilAttachmentViewIdentifier(const ::WebGPU::RenderPassAttachmentView& view)
{
    // RenderPassAttachmentView is a std::variant, see WebGPUCpp.h.
    if (auto* textureView = std::get_if<Ref<::WebGPU::TextureView>>(&view))
        return identifier(textureView->get());
    return identifier(std::get_if<Ref<::WebGPU::Texture>>(&view)->get());
}

std::optional<RenderPassDepthStencilAttachment> serialize(const ::WebGPU::RenderPassDepthStencilAttachment& renderPassDepthStencilAttachment)
{
    auto identifier = depthStencilAttachmentViewIdentifier(renderPassDepthStencilAttachment.view);

    return { { identifier, renderPassDepthStencilAttachment.depthClearValue, renderPassDepthStencilAttachment.depthLoadOp, renderPassDepthStencilAttachment.depthStoreOp, renderPassDepthStencilAttachment.depthReadOnly, renderPassDepthStencilAttachment.stencilClearValue, renderPassDepthStencilAttachment.stencilLoadOp, renderPassDepthStencilAttachment.stencilStoreOp, renderPassDepthStencilAttachment.stencilReadOnly } };
}

std::optional<RenderPassDescriptor> serialize(const ::WebGPU::RenderPassDescriptor& renderPassDescriptor)
{
    Vector<std::optional<RenderPassColorAttachment>> colorAttachments(ReserveCapacityTag { }, renderPassDescriptor.colorAttachments.size());
    for (const auto& colorAttachment : renderPassDescriptor.colorAttachments) {
        if (colorAttachment) {
            auto backingColorAttachment = serialize(*colorAttachment);
            if (!backingColorAttachment)
                return std::nullopt;
            colorAttachments.append(WTF::move(*backingColorAttachment));
        } else
            colorAttachments.append(std::nullopt);
    }

    std::optional<RenderPassDepthStencilAttachment> depthStencilAttachment;
    if (renderPassDescriptor.depthStencilAttachment) {
        depthStencilAttachment = serialize(*renderPassDescriptor.depthStencilAttachment);
        if (!depthStencilAttachment)
            return std::nullopt;
    }

    std::optional<WebGPUQuerySetIdentifier> occlusionQuerySet;
    if (renderPassDescriptor.occlusionQuerySet) {
        occlusionQuerySet = identifier(*protect(renderPassDescriptor.occlusionQuerySet));
        if (!occlusionQuerySet)
            return std::nullopt;
    }

    auto timestampWrites = renderPassDescriptor.timestampWrites ? serialize(*renderPassDescriptor.timestampWrites) : std::nullopt;

    return { { { renderPassDescriptor.label }, WTF::move(colorAttachments), WTF::move(depthStencilAttachment), occlusionQuerySet, WTF::move(timestampWrites), renderPassDescriptor.maxDrawCount } };
}

std::optional<RenderPassTimestampWrites> serialize(const ::WebGPU::PassTimestampWrites& renderPassTimestampWrite)
{
    auto querySet = identifier(protect(renderPassTimestampWrite.querySet).get());

    return { { querySet, renderPassTimestampWrite.beginningOfPassWriteIndex, renderPassTimestampWrite.endOfPassWriteIndex } };
}

std::optional<RenderPipelineDescriptor> serialize(const ::WebGPU::RenderPipelineDescriptor& renderPipelineDescriptor)
{
    std::optional<WebGPUPipelineLayoutIdentifier> layout;
    if (renderPipelineDescriptor.layout)
        layout = identifier(*protect(renderPipelineDescriptor.layout));

    auto vertex = serialize(renderPipelineDescriptor.vertex);
    if (!vertex)
        return std::nullopt;


    std::optional<FragmentState> fragment;
    if (renderPipelineDescriptor.fragment) {
        fragment = serialize(*renderPipelineDescriptor.fragment);
        if (!fragment)
            return std::nullopt;
    }

    return { { { { renderPipelineDescriptor.label }, layout }, WTF::move(*vertex), renderPipelineDescriptor.primitive, renderPipelineDescriptor.depthStencil, renderPipelineDescriptor.multisample, WTF::move(fragment) } };
}

std::optional<ShaderModuleDescriptor> serialize(const ::WebGPU::ShaderModuleDescriptor& shaderModuleDescriptor)
{
    auto hints = WTF::map(shaderModuleDescriptor.hints, [&](auto& hint) {
        return makeKeyValuePair(hint.entryPoint, ShaderModuleCompilationHint { identifier(hint.layout.get()) });
    });
    return { { { shaderModuleDescriptor.label }, shaderModuleDescriptor.code, WTF::move(hints) } };
}

std::optional<TextureDescriptor> serialize(const ::WebGPU::TextureDescriptor& textureDescriptor)
{
    return { { { textureDescriptor.label }, textureDescriptor.size, textureDescriptor.mipLevelCount, textureDescriptor.sampleCount, textureDescriptor.dimension, textureDescriptor.format, textureDescriptor.usage, Vector(textureDescriptor.viewFormats) } };
}

std::optional<VertexBufferLayout> serialize(const ::WebGPU::VertexBufferLayout& vertexBufferLayout)
{
    return { { vertexBufferLayout.arrayStride, vertexBufferLayout.stepMode, Vector<VertexAttribute> { vertexBufferLayout.attributes } } };
}

std::optional<VertexState> serialize(const ::WebGPU::VertexState& vertexState)
{
    auto base = serialize(vertexState.stage);
    if (!base)
        return std::nullopt;

    Vector<std::optional<VertexBufferLayout>> buffers(ReserveCapacityTag { }, vertexState.buffers.size());
    for (const auto& buffer : vertexState.buffers) {
        if (buffer) {
            auto convertedBuffer = serialize(*buffer);
            if (!convertedBuffer)
                return std::nullopt;
            buffers.append(WTF::move(*convertedBuffer));
        } else
            buffers.append(std::nullopt);
    }

    return { { WTF::move(*base), WTF::move(buffers) } };
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
