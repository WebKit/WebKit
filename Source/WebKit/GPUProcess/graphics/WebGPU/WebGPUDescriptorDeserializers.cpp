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
#include "WebGPUDescriptorDeserializers.h"

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
#include "WebGPUObjectHeap.h"
#include "WebGPUPipelineDescriptorBase.h"
#include "WebGPUPipelineLayoutDescriptor.h"
#include "WebGPUPresentationContextDescriptor.h"
#include "WebGPUProgrammableStage.h"
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
#include <wtf/Borrow.h>

namespace WebKit::WebGPU {

// The descriptor borrows the entries from entries.
std::optional<::WebGPU::BindGroupDescriptor> deserialize(ObjectHeap& objectHeap, const BindGroupDescriptor& bindGroupDescriptor, Vector<::WebGPU::BindGroupEntry>& entries)
{
    RefPtr bindGroupLayout = objectHeap.get(bindGroupDescriptor.bindGroupLayout);
    if (!bindGroupLayout)
        return std::nullopt;

    entries.reserveInitialCapacity(bindGroupDescriptor.entries.size());
    for (const auto& backingEntry : borrow(bindGroupDescriptor.entries)) {
        auto entry = deserialize(objectHeap, backingEntry);
        if (!entry)
            return std::nullopt;
        entries.append(WTF::move(*entry));
    }

    return ::WebGPU::BindGroupDescriptor { .label = bindGroupDescriptor.label, .layout = bindGroupLayout.releaseNonNull(), .entries = entries.span() };
}

std::optional<::WebGPU::BindGroupEntry> deserialize(ObjectHeap& objectHeap, const BindGroupEntry& bindGroupEntry)
{
    return WTF::switchOn(bindGroupEntry.resource, [&](WebGPUSamplerIdentifier identifier) -> std::optional<::WebGPU::BindGroupEntry> {
        RefPtr sampler = objectHeap.get(identifier);
        if (!sampler)
            return std::nullopt;
        return { { bindGroupEntry.binding, { *sampler } } };
    }, [&](WebGPUTextureIdentifier identifier) -> std::optional<::WebGPU::BindGroupEntry> {
        RefPtr texture = objectHeap.get(identifier);
        if (!texture)
            return std::nullopt;
        return { { bindGroupEntry.binding, { *texture } } };
    }, [&](WebGPUTextureViewIdentifier identifier) -> std::optional<::WebGPU::BindGroupEntry> {
        RefPtr textureView = objectHeap.get(identifier);
        if (!textureView)
            return std::nullopt;
        return { { bindGroupEntry.binding, { *textureView } } };
    }, [&](const BufferBinding& bufferBinding) -> std::optional<::WebGPU::BindGroupEntry> {
        auto convertedBufferBinding = deserialize(objectHeap, bufferBinding);
        if (!convertedBufferBinding)
            return std::nullopt;
        return { { bindGroupEntry.binding, { *convertedBufferBinding } } };
    }, [&](WebGPUExternalTextureIdentifier identifier) -> std::optional<::WebGPU::BindGroupEntry> {
        auto externalTexture = objectHeap.get(identifier);
        if (!externalTexture.get())
            return std::nullopt;
        return { { bindGroupEntry.binding, { *externalTexture.get() } } };
    });
}

// The descriptor borrows the entries from entries.
std::optional<::WebGPU::BindGroupLayoutDescriptor> deserialize(ObjectHeap&, const BindGroupLayoutDescriptor& bindGroupLayoutDescriptor, Vector<::WebGPU::BindGroupLayoutEntry>& entries)
{
    entries = bindGroupLayoutDescriptor.entries;

    return ::WebGPU::BindGroupLayoutDescriptor { .label = bindGroupLayoutDescriptor.label, .entries = entries.span() };
}

std::optional<::WebGPU::BufferBinding> deserialize(ObjectHeap& objectHeap, const BufferBinding& bufferBinding)
{
    RefPtr buffer = objectHeap.get(bufferBinding.buffer);
    if (!buffer)
        return std::nullopt;

    return { { *buffer, bufferBinding.offset, bufferBinding.size } };
}

std::optional<::WebGPU::CanvasConfiguration> deserialize(ObjectHeap& objectHeap, const CanvasConfiguration& canvasConfiguration)
{
    RefPtr device = objectHeap.get(canvasConfiguration.device);
    if (!device)
        return std::nullopt;

    return { {
        .device = device.releaseNonNull(),
        .format = canvasConfiguration.format,
        .usage = canvasConfiguration.usage,
        .viewFormats = canvasConfiguration.viewFormats.span(),
        .colorSpace = WebCore::convertToWebGPU(canvasConfiguration.colorSpace),
        .toneMappingMode = canvasConfiguration.toneMappingMode,
        .compositingAlphaMode = canvasConfiguration.compositingAlphaMode,
        .reportValidationErrors = canvasConfiguration.reportValidationErrors,
        .width = canvasConfiguration.width,
        .height = canvasConfiguration.height,
    } };
}

std::optional<::WebGPU::ComputePassDescriptor> deserialize(ObjectHeap& objectHeap, const ComputePassDescriptor& computePassDescriptor)
{
    auto timestampWrites = computePassDescriptor.timestampWrites ? deserialize(objectHeap, *computePassDescriptor.timestampWrites) : std::nullopt;

    return ::WebGPU::ComputePassDescriptor { .label = computePassDescriptor.label, .timestampWrites = WTF::move(timestampWrites) };
}

// std::nullopt when the layout does not deserialize; nullptr when it is missing and allowed to be.
static std::optional<RefPtr<::WebGPU::PipelineLayout>> deserializeLayout(ObjectHeap& objectHeap, const PipelineDescriptorBase& pipelineDescriptorBase, bool allowMissingPipelineLayout)
{
    if (!pipelineDescriptorBase.layout) {
        if (!allowMissingPipelineLayout)
            return std::nullopt;
        return { nullptr };
    }

    RefPtr layout = objectHeap.get(*pipelineDescriptorBase.layout);
    if (!layout)
        return std::nullopt;
    return { WTF::move(layout) };
}

std::optional<::WebGPU::ComputePipelineDescriptor> deserialize(ObjectHeap& objectHeap, const ComputePipelineDescriptor& computePipelineDescriptor, Vector<::WebGPU::ConstantEntry>& constantsStorage, bool allowMissingPipelineLayout)
{
    auto layout = deserializeLayout(objectHeap, computePipelineDescriptor, allowMissingPipelineLayout);
    if (!layout)
        return std::nullopt;

    auto compute = deserialize(objectHeap, computePipelineDescriptor.compute, constantsStorage);
    if (!compute)
        return std::nullopt;

    return { { computePipelineDescriptor.label, WTF::move(*layout), WTF::move(*compute) } };
}

std::optional<::WebGPU::DeviceDescriptor> deserialize(ObjectHeap&, const DeviceDescriptor& deviceDescriptor)
{
    return { { deviceDescriptor.label, deviceDescriptor.requiredFeatures.span(), deviceDescriptor.requiredLimits } };
}

std::optional<WebCore::WebGPUExternalTextureDescriptor> deserialize(ObjectHeap& objectHeap, const ExternalTextureDescriptor& externalTextureDescriptor, PixelBufferType pixelBuffer)
{
    auto base = deserialize(objectHeap, static_cast<const ObjectDescriptorBase&>(externalTextureDescriptor));
    if (!base)
        return std::nullopt;

    return { { WTF::move(*base), pixelBuffer, externalTextureDescriptor.colorSpace, externalTextureDescriptor.visibleSize } };
}

std::optional<::WebGPU::FragmentState> deserialize(ObjectHeap& objectHeap, const FragmentState& fragmentState, RenderPipelineDescriptorStorage& storage)
{
    auto stage = deserialize(objectHeap, static_cast<const ProgrammableStage&>(fragmentState), storage.fragmentConstants);
    if (!stage)
        return std::nullopt;

    storage.fragmentTargets = fragmentState.targets;

    return { { WTF::move(*stage), storage.fragmentTargets.span() } };
}

std::optional<::WebGPU::TexelCopyBufferInfo> deserialize(ObjectHeap& objectHeap, const ImageCopyBuffer& imageCopyBuffer)
{
    RefPtr buffer = objectHeap.get(imageCopyBuffer.buffer);
    if (!buffer)
        return std::nullopt;

    return { { imageCopyBuffer, buffer.releaseNonNull() } };
}

std::optional<WebCore::WebGPUExternalImageSource> deserialize(ObjectHeap&, const ImageCopyExternalImage& imageCopyExternalImage)
{
    // The source ImageBuffer cannot be resolved here; RemoteQueue looks it up through RemoteGPU
    // and fills it in, because only RemoteGPU can reach the RemoteRenderingBackend.
    return { { imageCopyExternalImage.origin, imageCopyExternalImage.flipY, nullptr, imageCopyExternalImage.premultipliedAlpha, std::nullopt, { } } };
}

#if PLATFORM(COCOA) && ENABLE(VIDEO)
std::optional<WebCore::WebGPUExternalImageSource> deserialize(ObjectHeap&, const ImageCopyExternalImageVideoSource& imageCopyExternalImage, PixelBufferType pixelBuffer, WebCore::VideoFrameRotation rotation, bool isMirrored)
{
    // RemoteQueue has already resolved the media player identifier, or read the shared frame, into
    // the pixel buffer the backing queue wraps, and read the display transform off the frame the
    // pixel buffer came from; a decoded frame is opaque, so its alpha needs neither premultiplying
    // nor undoing.
    return { { imageCopyExternalImage.origin, imageCopyExternalImage.flipY, nullptr, true, WebCore::WebGPUVideoSourceIdentifier { pixelBuffer }, imageCopyExternalImage.sourceSize, rotation, isMirrored } };
}
#endif

std::optional<::WebGPU::TexelCopyTextureInfo> deserialize(ObjectHeap& objectHeap, const ImageCopyTexture& imageCopyTexture)
{
    RefPtr texture = objectHeap.get(imageCopyTexture.texture);
    if (!texture)
        return std::nullopt;

    return { { *texture, imageCopyTexture.mipLevel, imageCopyTexture.origin.value_or(::WebGPU::Origin3D { }), imageCopyTexture.aspect } };
}

std::optional<WebCore::WebGPUImageCopyTextureTagged> deserialize(ObjectHeap& objectHeap, const ImageCopyTextureTagged& imageCopyTextureTagged)
{
    auto base = deserialize(objectHeap, static_cast<const ImageCopyTexture&>(imageCopyTextureTagged));
    if (!base)
        return std::nullopt;

    return { { WTF::move(*base), imageCopyTextureTagged.colorSpace, imageCopyTextureTagged.premultipliedAlpha } };
}

std::optional<WebCore::WebGPUObjectDescriptorBase> deserialize(ObjectHeap&, const ObjectDescriptorBase& objectDescriptorBase)
{
    return { { objectDescriptorBase.label } };
}

// The descriptor borrows the bind group layouts from bindGroupLayouts.
std::optional<::WebGPU::PipelineLayoutDescriptor> deserialize(ObjectHeap& objectHeap, const PipelineLayoutDescriptor& pipelineLayoutDescriptor, Vector<Ref<::WebGPU::BindGroupLayout>>& bindGroupLayouts)
{
    // Only the implementation creates an autogenerated layout with bind group layouts.
    if (pipelineLayoutDescriptor.isAutogenerated && !pipelineLayoutDescriptor.bindGroupLayouts.isEmpty())
        return std::nullopt;

    bindGroupLayouts.reserveInitialCapacity(pipelineLayoutDescriptor.bindGroupLayouts.size());
    for (const auto& backingBindGroupLayout : borrow(pipelineLayoutDescriptor.bindGroupLayouts)) {
        RefPtr entry = objectHeap.get(backingBindGroupLayout);
        if (!entry)
            return std::nullopt;
        bindGroupLayouts.append(entry.releaseNonNull());
    }

    return ::WebGPU::PipelineLayoutDescriptor { .label = pipelineLayoutDescriptor.label, .bindGroupLayouts = bindGroupLayouts.span(), .isAutogenerated = pipelineLayoutDescriptor.isAutogenerated };
}

std::optional<WebCore::WebGPUPresentationContextDescriptor> deserialize(ObjectHeap& objectHeap, const PresentationContextDescriptor& presentationContextDescriptor)
{
    WeakPtr compositorIntegration = objectHeap.get(presentationContextDescriptor.compositorIntegration);
    if (!compositorIntegration)
        return std::nullopt;

    return { { *compositorIntegration } };
}

std::optional<::WebGPU::ProgrammableStage> deserialize(ObjectHeap& objectHeap, const ProgrammableStage& programmableStage, Vector<::WebGPU::ConstantEntry>& constantsStorage)
{
    RefPtr shaderModule = objectHeap.get(programmableStage.module);
    if (!shaderModule)
        return std::nullopt;

    constantsStorage = programmableStage.constants.map([](auto& constant) {
        return ::WebGPU::ConstantEntry { .key = constant.key, .value = constant.value };
    });
    return { { shaderModule.releaseNonNull(), programmableStage.entryPoint, constantsStorage.span() } };
}

std::optional<::WebGPU::RenderBundleEncoderDescriptor> deserialize(ObjectHeap&, const RenderBundleEncoderDescriptor& renderBundleEncoderDescriptor)
{
    return { {
        .label = renderBundleEncoderDescriptor.label,
        .colorFormats = renderBundleEncoderDescriptor.colorFormats.span(),
        .depthStencilFormat = renderBundleEncoderDescriptor.depthStencilFormat,
        .sampleCount = renderBundleEncoderDescriptor.sampleCount,
        .depthReadOnly = renderBundleEncoderDescriptor.depthReadOnly,
        .stencilReadOnly = renderBundleEncoderDescriptor.stencilReadOnly,
    } };
}

static std::optional<::WebGPU::RenderPassAttachmentView> deserializeAttachmentView(ObjectHeap& objectHeap, const WebGPURenderPassAttachmentViewIdentifier& view)
{
    return WTF::switchOn(view, [&](WebGPUTextureViewIdentifier identifier) -> std::optional<::WebGPU::RenderPassAttachmentView> {
        RefPtr textureView = objectHeap.get(identifier);
        if (!textureView)
            return std::nullopt;
        return ::WebGPU::RenderPassAttachmentView { textureView.releaseNonNull() };
    }, [&](WebGPUTextureIdentifier identifier) -> std::optional<::WebGPU::RenderPassAttachmentView> {
        RefPtr texture = objectHeap.get(identifier);
        if (!texture)
            return std::nullopt;
        return ::WebGPU::RenderPassAttachmentView { texture.releaseNonNull() };
    });
}

std::optional<::WebGPU::RenderPassColorAttachment> deserialize(ObjectHeap& objectHeap, const RenderPassColorAttachment& renderPassColorAttachment)
{
    auto view = deserializeAttachmentView(objectHeap, renderPassColorAttachment.view);
    if (!view)
        return std::nullopt;

    std::optional<::WebGPU::RenderPassAttachmentView> resolveTarget;
    if (renderPassColorAttachment.resolveTarget) {
        resolveTarget = deserializeAttachmentView(objectHeap, *renderPassColorAttachment.resolveTarget);
        if (!resolveTarget)
            return std::nullopt;
    }

    return { { WTF::move(*view), renderPassColorAttachment.depthSlice, WTF::move(resolveTarget), renderPassColorAttachment.clearValue.value_or(::WebGPU::Color { }), renderPassColorAttachment.loadOp, renderPassColorAttachment.storeOp } };
}

std::optional<::WebGPU::RenderPassDepthStencilAttachment> deserialize(ObjectHeap& objectHeap, const RenderPassDepthStencilAttachment& renderPassDepthStencilAttachment)
{
    auto view = deserializeAttachmentView(objectHeap, renderPassDepthStencilAttachment.view);
    if (!view)
        return std::nullopt;

    return { { WTF::move(*view), renderPassDepthStencilAttachment.depthClearValue, renderPassDepthStencilAttachment.depthLoadOp, renderPassDepthStencilAttachment.depthStoreOp, renderPassDepthStencilAttachment.depthReadOnly, renderPassDepthStencilAttachment.stencilClearValue, renderPassDepthStencilAttachment.stencilLoadOp, renderPassDepthStencilAttachment.stencilStoreOp, renderPassDepthStencilAttachment.stencilReadOnly } };
}

// The descriptor borrows the color attachments from colorAttachments.
std::optional<::WebGPU::RenderPassDescriptor> deserialize(ObjectHeap& objectHeap, const RenderPassDescriptor& renderPassDescriptor, Vector<std::optional<::WebGPU::RenderPassColorAttachment>>& colorAttachments)
{
    colorAttachments.reserveInitialCapacity(renderPassDescriptor.colorAttachments.size());
    for (const auto& backingColorAttachment : borrow(renderPassDescriptor.colorAttachments)) {
        if (backingColorAttachment) {
            auto colorAttachment = deserialize(objectHeap, *backingColorAttachment);
            if (!colorAttachment)
                return std::nullopt;
            colorAttachments.append(WTF::move(*colorAttachment));
        } else
            colorAttachments.append(std::nullopt);
    }

    auto depthStencilAttachment = ([&] -> std::optional<::WebGPU::RenderPassDepthStencilAttachment> {
        if (renderPassDescriptor.depthStencilAttachment)
            return deserialize(objectHeap, *renderPassDescriptor.depthStencilAttachment);
        return std::nullopt;
    })();
    if (renderPassDescriptor.depthStencilAttachment && !depthStencilAttachment)
        return std::nullopt;

    RefPtr<::WebGPU::QuerySet> occlusionQuerySet;
    if (renderPassDescriptor.occlusionQuerySet) {
        occlusionQuerySet = objectHeap.get(renderPassDescriptor.occlusionQuerySet.value());
        if (!occlusionQuerySet)
            return std::nullopt;
    }

    auto timestampWrites = renderPassDescriptor.timestampWrites ? deserialize(objectHeap, *renderPassDescriptor.timestampWrites) : std::nullopt;

    return ::WebGPU::RenderPassDescriptor {
        .label = renderPassDescriptor.label,
        .colorAttachments = colorAttachments.span(),
        .depthStencilAttachment = WTF::move(depthStencilAttachment),
        .occlusionQuerySet = WTF::move(occlusionQuerySet),
        .timestampWrites = WTF::move(timestampWrites),
        .maxDrawCount = renderPassDescriptor.maxDrawCount,
    };
}

std::optional<::WebGPU::PassTimestampWrites> deserialize(ObjectHeap& objectHeap, const RenderPassTimestampWrites& renderPassTimestampWrite)
{
    RefPtr querySet = objectHeap.get(renderPassTimestampWrite.querySet);
    if (!querySet)
        return std::nullopt;

    return { { querySet.releaseNonNull(), renderPassTimestampWrite.beginningOfPassWriteIndex, renderPassTimestampWrite.endOfPassWriteIndex } };
}

std::optional<::WebGPU::RenderPipelineDescriptor> deserialize(ObjectHeap& objectHeap, const RenderPipelineDescriptor& renderPipelineDescriptor, RenderPipelineDescriptorStorage& storage, bool allowMissingPipelineLayout)
{
    auto layout = deserializeLayout(objectHeap, renderPipelineDescriptor, allowMissingPipelineLayout);
    if (!layout)
        return std::nullopt;

    auto vertex = deserialize(objectHeap, renderPipelineDescriptor.vertex, storage);
    if (!vertex)
        return std::nullopt;

    auto primitive = renderPipelineDescriptor.primitive.value_or(::WebGPU::PrimitiveState { });
    auto multisample = renderPipelineDescriptor.multisample.value_or(::WebGPU::MultisampleState { });

    std::optional<::WebGPU::FragmentState> fragment;
    if (renderPipelineDescriptor.fragment) {
        fragment = deserialize(objectHeap, *renderPipelineDescriptor.fragment, storage);
        if (!fragment)
            return std::nullopt;
    }

    return { { renderPipelineDescriptor.label, WTF::move(*layout), WTF::move(*vertex), primitive, renderPipelineDescriptor.depthStencil, multisample, WTF::move(fragment) } };
}

std::optional<::WebGPU::ShaderModuleDescriptor> deserialize(ObjectHeap& objectHeap, const ShaderModuleDescriptor& shaderModuleDescriptor, Vector<::WebGPU::ShaderModuleCompilationHint>& hintsStorage)
{
    hintsStorage.reserveInitialCapacity(shaderModuleDescriptor.hints.size());
    for (const auto& hint : borrow(shaderModuleDescriptor.hints)) {
        RefPtr pipelineLayout = objectHeap.get(hint.value.pipelineLayout);
        if (!pipelineLayout)
            return std::nullopt;
        hintsStorage.append({ .entryPoint = hint.key, .layout = pipelineLayout.releaseNonNull() });
    }

    return { { shaderModuleDescriptor.label, shaderModuleDescriptor.code, hintsStorage.span() } };
}

// The descriptor borrows the view formats from textureDescriptor.
std::optional<::WebGPU::TextureDescriptor> deserialize(ObjectHeap&, const TextureDescriptor& textureDescriptor)
{
    return ::WebGPU::TextureDescriptor {
        .label = textureDescriptor.label,
        .usage = textureDescriptor.usage,
        .dimension = textureDescriptor.dimension,
        .size = textureDescriptor.size,
        .format = textureDescriptor.format,
        .mipLevelCount = textureDescriptor.mipLevelCount,
        .sampleCount = textureDescriptor.sampleCount,
        .viewFormats = textureDescriptor.viewFormats.span(),
    };
}

std::optional<::WebGPU::VertexBufferLayout> deserialize(ObjectHeap&, const VertexBufferLayout& vertexBufferLayout, Vector<::WebGPU::VertexAttribute>& attributesStorage)
{
    attributesStorage = vertexBufferLayout.attributes;

    return { { vertexBufferLayout.arrayStride, vertexBufferLayout.stepMode, attributesStorage.span() } };
}

std::optional<::WebGPU::VertexState> deserialize(ObjectHeap& objectHeap, const VertexState& vertexState, RenderPipelineDescriptorStorage& storage)
{
    auto stage = deserialize(objectHeap, static_cast<const ProgrammableStage&>(vertexState), storage.vertexConstants);
    if (!stage)
        return std::nullopt;

    // The attributes of each buffer are sized up front, so that the spans into them stay valid.
    Borrow buffers { vertexState.buffers };
    storage.vertexAttributes.resize(buffers->size());
    storage.vertexBuffers.reserveInitialCapacity(buffers->size());
    for (size_t i = 0; i < buffers->size(); ++i) {
        auto& backingBuffer = buffers[i];
        if (backingBuffer) {
            auto buffer = deserialize(objectHeap, *backingBuffer, borrow(storage.vertexAttributes)[i]);
            if (!buffer)
                return std::nullopt;
            storage.vertexBuffers.append(WTF::move(*buffer));
        } else
            storage.vertexBuffers.append(std::nullopt);
    }

    return { { WTF::move(*stage), storage.vertexBuffers.span() } };
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
