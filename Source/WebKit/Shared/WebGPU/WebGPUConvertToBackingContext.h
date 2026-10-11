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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "WebGPUColor.h"
#include "WebGPUCommandEncoderDescriptor.h"
#include "WebGPUCompilationMessage.h"
#include "WebGPUComputePassTimestampWrites.h"
#include "WebGPUError.h"
#include "WebGPUExtent3D.h"
#include "WebGPUIdentifier.h"
#include "WebGPUOrigin2D.h"
#include "WebGPUOrigin3D.h"
#include "WebGPURenderPassTimestampWrites.h"
#include "WebGPUTextureViewDescriptor.h"
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUDevice.h>
#include <WebCore/WebGPUError.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class WebGPUCompositorIntegration;
class WebGPUIntegration;
struct WebGPUExternalImageSource;
struct WebGPUExternalTextureDescriptor;
struct WebGPUImageCopyTextureTagged;
struct WebGPUObjectDescriptorBase;
struct WebGPUPresentationContextDescriptor;
}

namespace WebKit::WebGPU {

struct BindGroupDescriptor;
struct BindGroupEntry;
struct BindGroupLayoutDescriptor;
struct BindGroupLayoutEntry;
struct BlendComponent;
struct BlendState;
struct BufferBinding;
struct BufferBindingLayout;
struct CanvasConfiguration;
struct ColorTargetState;
struct ComputePassDescriptor;
struct ComputePipelineDescriptor;
struct DepthStencilState;
struct DeviceDescriptor;
struct ExternalTextureBindingLayout;
struct ExternalTextureDescriptor;
struct FragmentState;
struct Identifier;
struct ImageCopyBuffer;
struct ImageCopyExternalImage;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
struct ImageCopyExternalImageVideoSource;
#endif
struct ImageCopyTexture;
struct ImageCopyTextureTagged;
struct ImageDataLayout;
struct InternalError;
struct MultisampleState;
struct ObjectDescriptorBase;
struct OutOfMemoryError;
struct PipelineDescriptorBase;
struct PipelineLayoutDescriptor;
struct PresentationContextDescriptor;
struct CanvasConfiguration;
struct PrimitiveState;
struct ProgrammableStage;
class RemoteCompositorIntegrationProxy;
struct RenderBundleEncoderDescriptor;
struct RenderPassColorAttachment;
struct RenderPassDepthStencilAttachment;
struct RenderPassDescriptor;
struct RenderPassLayout;
struct RenderPipelineDescriptor;
struct RequestAdapterOptions;
struct SamplerBindingLayout;
struct ShaderModuleCompilationHint;
struct ShaderModuleDescriptor;
struct StencilFaceState;
struct StorageTextureBindingLayout;
struct SupportedFeatures;
struct SupportedLimits;
struct TextureBindingLayout;
struct TextureDescriptor;
struct ValidationError;
struct VertexAttribute;
struct VertexBufferLayout;
struct VertexState;

class ConvertToBackingContext : public RefCounted<ConvertToBackingContext> {
public:
    virtual ~ConvertToBackingContext() = default;

    std::optional<BindGroupDescriptor> convertToBacking(const ::WebGPU::BindGroupDescriptor&);
    std::optional<BindGroupEntry> convertToBacking(const ::WebGPU::BindGroupEntry&);
    std::optional<BindGroupLayoutDescriptor> convertToBacking(const ::WebGPU::BindGroupLayoutDescriptor&);
    std::optional<BindGroupLayoutEntry> convertToBacking(const ::WebGPU::BindGroupLayoutEntry&);
    std::optional<BlendComponent> NODELETE convertToBacking(const ::WebGPU::BlendComponent&);
    std::optional<BlendState> NODELETE convertToBacking(const ::WebGPU::BlendState&);
    std::optional<BufferBinding> convertToBacking(const ::WebGPU::BufferBinding&);
    std::optional<BufferBindingLayout> NODELETE convertToBacking(const ::WebGPU::BufferBindingLayout&);
    std::optional<CanvasConfiguration> convertToBacking(const ::WebGPU::CanvasConfiguration&);
    std::optional<ColorTargetState> convertToBacking(const ::WebGPU::ColorTargetState&);
    std::optional<ComputePassDescriptor> convertToBacking(const ::WebGPU::ComputePassDescriptor&);
    std::optional<ComputePipelineDescriptor> convertToBacking(const ::WebGPU::ComputePipelineDescriptor&);
    std::optional<DepthStencilState> convertToBacking(const ::WebGPU::DepthStencilState&);
    std::optional<DeviceDescriptor> convertToBacking(const ::WebGPU::DeviceDescriptor&);
    std::optional<ExternalTextureBindingLayout> NODELETE convertToBacking(const ::WebGPU::ExternalTextureBindingLayout&);
    std::optional<ExternalTextureDescriptor> convertToBacking(const WebCore::WebGPUExternalTextureDescriptor&);
    std::optional<FragmentState> convertToBacking(const ::WebGPU::FragmentState&);
    std::optional<ImageCopyBuffer> convertToBacking(const ::WebGPU::TexelCopyBufferInfo&);
    std::optional<ImageCopyExternalImage> convertToBacking(const WebCore::WebGPUExternalImageSource&);
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    std::optional<ImageCopyExternalImageVideoSource> convertToBackingVideoSource(const WebCore::WebGPUExternalImageSource&);
#endif
    std::optional<ImageCopyTexture> convertToBacking(const ::WebGPU::TexelCopyTextureInfo&);
    std::optional<ImageCopyTextureTagged> convertToBacking(const WebCore::WebGPUImageCopyTextureTagged&);
    std::optional<ImageDataLayout> NODELETE convertToBacking(const ::WebGPU::TexelCopyBufferLayout&);
    std::optional<MultisampleState> NODELETE convertToBacking(const ::WebGPU::MultisampleState&);
    std::optional<ObjectDescriptorBase> NODELETE convertToBacking(const WebCore::WebGPUObjectDescriptorBase&);
    std::optional<PipelineLayoutDescriptor> convertToBacking(const ::WebGPU::PipelineLayoutDescriptor&);
    std::optional<PresentationContextDescriptor> convertToBacking(const WebCore::WebGPUPresentationContextDescriptor&);
    std::optional<PrimitiveState> NODELETE convertToBacking(const ::WebGPU::PrimitiveState&);
    std::optional<ProgrammableStage> convertToBacking(const ::WebGPU::ProgrammableStage&);
    std::optional<RenderBundleEncoderDescriptor> convertToBacking(const ::WebGPU::RenderBundleEncoderDescriptor&);
    std::optional<RenderPassColorAttachment> convertToBacking(const ::WebGPU::RenderPassColorAttachment&);
    std::optional<RenderPassDepthStencilAttachment> convertToBacking(const ::WebGPU::RenderPassDepthStencilAttachment&);
    std::optional<RenderPassDescriptor> convertToBacking(const ::WebGPU::RenderPassDescriptor&);
    std::optional<RenderPassTimestampWrites> convertToBacking(const ::WebGPU::PassTimestampWrites&);
    std::optional<RenderPipelineDescriptor> convertToBacking(const ::WebGPU::RenderPipelineDescriptor&);
    std::optional<RequestAdapterOptions> NODELETE convertToBacking(const ::WebGPU::RequestAdapterOptions&);
    std::optional<SamplerBindingLayout> NODELETE convertToBacking(const ::WebGPU::SamplerBindingLayout&);
    std::optional<ShaderModuleDescriptor> convertToBacking(const ::WebGPU::ShaderModuleDescriptor&);
    std::optional<StencilFaceState> NODELETE convertToBacking(const ::WebGPU::StencilFaceState&);
    std::optional<StorageTextureBindingLayout> NODELETE convertToBacking(const ::WebGPU::StorageTextureBindingLayout&);
    std::optional<TextureBindingLayout> NODELETE convertToBacking(const ::WebGPU::TextureBindingLayout&);
    std::optional<TextureDescriptor> convertToBacking(const ::WebGPU::TextureDescriptor&);
    std::optional<VertexAttribute> NODELETE convertToBacking(const ::WebGPU::VertexAttribute&);
    std::optional<VertexBufferLayout> convertToBacking(const ::WebGPU::VertexBufferLayout&);
    std::optional<VertexState> convertToBacking(const ::WebGPU::VertexState&);

    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::Adapter&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::BindGroup&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::BindGroupLayout&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::Buffer&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::CommandBuffer&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::CommandEncoder&) = 0;
    virtual const RemoteCompositorIntegrationProxy& convertToRawBacking(const WebCore::WebGPUCompositorIntegration&) = 0;
    virtual WebGPUIdentifier convertToBacking(const WebCore::WebGPUCompositorIntegration&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::ComputePassEncoder&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::ComputePipeline&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::Device&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::ExternalTexture&) = 0;
    virtual WebGPUIdentifier convertToBacking(const WebCore::WebGPUIntegration&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::PipelineLayout&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::PresentationContext&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::QuerySet&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::Queue&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::RenderBundleEncoder&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::RenderBundle&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::RenderPassEncoder&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::RenderPipeline&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::Sampler&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::ShaderModule&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::Texture&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::TextureView&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::XRBinding&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::XRProjectionLayer&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::XRSubImage&) = 0;
    virtual WebGPUIdentifier convertToBacking(const ::WebGPU::XRView&) = 0;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
