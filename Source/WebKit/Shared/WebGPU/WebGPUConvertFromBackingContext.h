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
#include <optional>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

#if ENABLE(VIDEO) && PLATFORM(COCOA)
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer* CVPixelBufferRef;

namespace WebCore {
enum class VideoFrameRotation : uint16_t;
}
#endif

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
struct CanvasConfiguration;
struct PresentationContextDescriptor;
struct PrimitiveState;
struct ProgrammableStage;
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

// Owns the arrays that a converted WebGPU::RenderPipelineDescriptor borrows.
struct RenderPipelineDescriptorStorage {
    Vector<::WebGPU::ConstantEntry> vertexConstants;
    Vector<Vector<::WebGPU::VertexAttribute>> vertexAttributes;
    Vector<std::optional<::WebGPU::VertexBufferLayout>> vertexBuffers;
    Vector<::WebGPU::ConstantEntry> fragmentConstants;
    Vector<std::optional<::WebGPU::ColorTargetState>> fragmentTargets;
};

class ConvertFromBackingContext {
public:
    virtual ~ConvertFromBackingContext() = default;

    std::optional<::WebGPU::BindGroupDescriptor> convertFromBacking(const BindGroupDescriptor&, Vector<::WebGPU::BindGroupEntry>& entriesStorage);
    std::optional<::WebGPU::BindGroupEntry> convertFromBacking(const BindGroupEntry&);
    std::optional<::WebGPU::BindGroupLayoutDescriptor> convertFromBacking(const BindGroupLayoutDescriptor&, Vector<::WebGPU::BindGroupLayoutEntry>& entriesStorage);
    std::optional<::WebGPU::BindGroupLayoutEntry> convertFromBacking(const BindGroupLayoutEntry&);
    std::optional<::WebGPU::BlendComponent> NODELETE convertFromBacking(const BlendComponent&);
    std::optional<::WebGPU::BlendState> NODELETE convertFromBacking(const BlendState&);
    std::optional<::WebGPU::BufferBinding> convertFromBacking(const BufferBinding&);
    std::optional<::WebGPU::BufferBindingLayout> NODELETE convertFromBacking(const BufferBindingLayout&);
    std::optional<::WebGPU::CanvasConfiguration> convertFromBacking(const CanvasConfiguration&);
    std::optional<::WebGPU::ColorTargetState> convertFromBacking(const ColorTargetState&);
    std::optional<::WebGPU::ComputePassDescriptor> convertFromBacking(const ComputePassDescriptor&);
    std::optional<::WebGPU::ComputePipelineDescriptor> convertFromBacking(const ComputePipelineDescriptor&, Vector<::WebGPU::ConstantEntry>& constantsStorage, bool allowMissingPipelineLayout = false);
    std::optional<::WebGPU::DepthStencilState> convertFromBacking(const DepthStencilState&);
    std::optional<::WebGPU::DeviceDescriptor> convertFromBacking(const DeviceDescriptor&);
    std::optional<::WebGPU::ExternalTextureBindingLayout> NODELETE convertFromBacking(const ExternalTextureBindingLayout&);
#if ENABLE(VIDEO) && PLATFORM(COCOA)
    using PixelBufferType = RetainPtr<CVPixelBufferRef>;
#else
    using PixelBufferType = void*;
#endif
    std::optional<WebCore::WebGPUExternalTextureDescriptor> convertFromBacking(const ExternalTextureDescriptor&, PixelBufferType);
    std::optional<::WebGPU::FragmentState> convertFromBacking(const FragmentState&, RenderPipelineDescriptorStorage&);
    std::optional<::WebGPU::TexelCopyBufferInfo> convertFromBacking(const ImageCopyBuffer&);
    std::optional<WebCore::WebGPUExternalImageSource> convertFromBacking(const ImageCopyExternalImage&);
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    std::optional<WebCore::WebGPUExternalImageSource> convertFromBacking(const ImageCopyExternalImageVideoSource&, PixelBufferType, WebCore::VideoFrameRotation, bool isMirrored);
#endif
    std::optional<::WebGPU::TexelCopyTextureInfo> convertFromBacking(const ImageCopyTexture&);
    std::optional<WebCore::WebGPUImageCopyTextureTagged> convertFromBacking(const ImageCopyTextureTagged&);
    std::optional<::WebGPU::TexelCopyBufferLayout> NODELETE convertFromBacking(const ImageDataLayout&);
    std::optional<::WebGPU::MultisampleState> NODELETE convertFromBacking(const MultisampleState&);
    std::optional<WebCore::WebGPUObjectDescriptorBase> NODELETE convertFromBacking(const ObjectDescriptorBase&);
    // std::nullopt when the layout does not convert; nullptr when it is missing and allowed to be.
    std::optional<RefPtr<::WebGPU::PipelineLayout>> convertLayoutFromBacking(const PipelineDescriptorBase&, bool allowMissingPipelineLayout);
    std::optional<::WebGPU::PipelineLayoutDescriptor> convertFromBacking(const PipelineLayoutDescriptor&, Vector<Ref<::WebGPU::BindGroupLayout>>& bindGroupLayoutsStorage);
    std::optional<WebCore::WebGPUPresentationContextDescriptor> convertFromBacking(const PresentationContextDescriptor&);
    std::optional<::WebGPU::PrimitiveState> NODELETE convertFromBacking(const PrimitiveState&);
    std::optional<::WebGPU::ProgrammableStage> convertFromBacking(const ProgrammableStage&, Vector<::WebGPU::ConstantEntry>& constantsStorage);
    std::optional<::WebGPU::RenderBundleEncoderDescriptor> convertFromBacking(const RenderBundleEncoderDescriptor&);
    std::optional<::WebGPU::RenderPassColorAttachment> convertFromBacking(const RenderPassColorAttachment&);
    std::optional<::WebGPU::RenderPassDepthStencilAttachment> convertFromBacking(const RenderPassDepthStencilAttachment&);
    std::optional<::WebGPU::RenderPassDescriptor> convertFromBacking(const RenderPassDescriptor&, Vector<std::optional<::WebGPU::RenderPassColorAttachment>>& colorAttachmentsStorage);
    std::optional<::WebGPU::PassTimestampWrites> convertFromBacking(const RenderPassTimestampWrites&);
    std::optional<::WebGPU::RenderPipelineDescriptor> convertFromBacking(const RenderPipelineDescriptor&, RenderPipelineDescriptorStorage&, bool allowMissingPipelineLayout = false);
    std::optional<::WebGPU::RequestAdapterOptions> NODELETE convertFromBacking(const RequestAdapterOptions&);
    std::optional<::WebGPU::SamplerBindingLayout> NODELETE convertFromBacking(const SamplerBindingLayout&);
    std::optional<::WebGPU::ShaderModuleDescriptor> convertFromBacking(const ShaderModuleDescriptor&, Vector<::WebGPU::ShaderModuleCompilationHint>& hintsStorage);
    std::optional<::WebGPU::StencilFaceState> NODELETE convertFromBacking(const StencilFaceState&);
    std::optional<::WebGPU::StorageTextureBindingLayout> NODELETE convertFromBacking(const StorageTextureBindingLayout&);
    std::optional<::WebGPU::TextureBindingLayout> NODELETE convertFromBacking(const TextureBindingLayout&);
    std::optional<::WebGPU::TextureDescriptor> convertFromBacking(const TextureDescriptor&);
    std::optional<::WebGPU::VertexAttribute> NODELETE convertFromBacking(const VertexAttribute&);
    std::optional<::WebGPU::VertexBufferLayout> convertFromBacking(const VertexBufferLayout&, Vector<::WebGPU::VertexAttribute>& attributesStorage);
    std::optional<::WebGPU::VertexState> convertFromBacking(const VertexState&, RenderPipelineDescriptorStorage&);

    virtual RefPtr<::WebGPU::Adapter> convertAdapterFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::BindGroup> convertBindGroupFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::BindGroupLayout> convertBindGroupLayoutFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::Buffer> convertBufferFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::CommandBuffer> convertCommandBufferFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::CommandEncoder> convertCommandEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual WeakPtr<WebCore::WebGPUCompositorIntegration> convertCompositorIntegrationFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::ComputePassEncoder> convertComputePassEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::ComputePipeline> convertComputePipelineFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::Device> convertDeviceFromBacking(WebGPUIdentifier) = 0;
    virtual ThreadSafeWeakPtr<::WebGPU::ExternalTexture> convertExternalTextureFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::PipelineLayout> convertPipelineLayoutFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::QuerySet> convertQuerySetFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::Queue> convertQueueFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::RenderBundleEncoder> convertRenderBundleEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::RenderBundle> convertRenderBundleFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::RenderPassEncoder> convertRenderPassEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::RenderPipeline> convertRenderPipelineFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::Sampler> convertSamplerFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::ShaderModule> convertShaderModuleFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::PresentationContext> convertPresentationContextFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::Texture> convertTextureFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::TextureView> convertTextureViewFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::XRBinding> convertXRBindingFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::XRProjectionLayer> convertXRProjectionLayerFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::XRSubImage> convertXRSubImageFromBacking(WebGPUIdentifier) = 0;
    virtual RefPtr<::WebGPU::XRView> createXRViewFromBacking(WebGPUIdentifier) = 0;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
