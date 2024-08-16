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
#include "WebGPUComputePassTimestampWrites.h"
#include "WebGPUError.h"
#include "WebGPUExtent3D.h"
#include "WebGPUIdentifier.h"
#include "WebGPUOrigin2D.h"
#include "WebGPUOrigin3D.h"
#include "WebGPURenderPassTimestampWrites.h"
#include <WebCore/WebGPUColor.h>
#include <WebCore/WebGPUComputePassTimestampWrites.h>
#include <WebCore/WebGPUError.h>
#include <WebCore/WebGPUExtent3D.h>
#include <WebCore/WebGPUOrigin2D.h>
#include <WebCore/WebGPUOrigin3D.h>
#include <WebCore/WebGPURenderPassTimestampWrites.h>
#include <optional>
#include <wtf/RefCounted.h>

#if ENABLE(VIDEO) && PLATFORM(COCOA)
typedef struct __CVBuffer* CVPixelBufferRef;
#endif

namespace WebCore::WebGPU {

class Adapter;
class BindGroup;
struct BindGroupDescriptor;
struct BindGroupEntry;
class BindGroupLayout;
struct BindGroupLayoutDescriptor;
struct BindGroupLayoutEntry;
struct BlendComponent;
struct BlendState;
class Buffer;
struct BufferBinding;
struct BufferBindingLayout;
struct BufferDescriptor;
struct CanvasConfiguration;
struct ColorTargetState;
class CommandBuffer;
struct CommandBufferDescriptor;
class CommandEncoder;
struct CommandEncoderDescriptor;
class CompilationMessage;
class CompositorIntegration;
struct ComputePassDescriptor;
class ComputePassEncoder;
class ComputePipeline;
struct ComputePipelineDescriptor;
struct DepthStencilState;
class Device;
struct DeviceDescriptor;
struct ExternalTextureBindingLayout;
struct ExternalTextureDescriptor;
class ExternalTexture;
struct FragmentState;
class GPU;
struct Identifier;
struct ImageCopyBuffer;
struct ImageCopyExternalImage;
struct ImageCopyTexture;
struct ImageCopyTextureTagged;
struct ImageDataLayout;
class InternalError;
struct MultisampleState;
struct ObjectDescriptorBase;
class OutOfMemoryError;
struct PipelineDescriptorBase;
class PipelineLayout;
struct PipelineLayoutDescriptor;
struct CanvasConfiguration;
class PresentationContext;
struct PresentationContextDescriptor;
struct PrimitiveState;
struct ProgrammableStage;
class QuerySet;
struct QuerySetDescriptor;
class Queue;
class RenderBundle;
struct RenderBundleDescriptor;
class RenderBundleEncoder;
struct RenderBundleEncoderDescriptor;
struct RenderPassColorAttachment;
struct RenderPassDepthStencilAttachment;
struct RenderPassDescriptor;
class RenderPassEncoder;
struct RenderPassLayout;
class RenderPipeline;
struct RenderPipelineDescriptor;
struct RequestAdapterOptions;
class Sampler;
struct SamplerBindingLayout;
struct SamplerDescriptor;
class ShaderModule;
struct ShaderModuleCompilationHint;
struct ShaderModuleDescriptor;
struct StencilFaceState;
struct StorageTextureBindingLayout;
class SupportedFeatures;
class SupportedLimits;
class Texture;
struct TextureBindingLayout;
struct TextureDescriptor;
class TextureView;
struct TextureViewDescriptor;
class ValidationError;
struct VertexAttribute;
struct VertexBufferLayout;
struct VertexState;
class XRBinding;
class XRProjectionLayer;
class XRSubImage;
class XRView;

} // namespace WebCore::WebGPU

namespace WebKit::WebGPU {

struct BindGroupDescriptor;
struct BindGroupEntry;
struct BindGroupLayoutDescriptor;
struct BindGroupLayoutEntry;
struct BlendComponent;
struct BlendState;
struct BufferBinding;
struct BufferBindingLayout;
struct BufferDescriptor;
struct CanvasConfiguration;
struct ColorTargetState;
struct CommandBufferDescriptor;
struct CommandEncoderDescriptor;
struct CompilationMessage;
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
struct QuerySetDescriptor;
struct RenderBundleDescriptor;
struct RenderBundleEncoderDescriptor;
struct RenderPassColorAttachment;
struct RenderPassDepthStencilAttachment;
struct RenderPassDescriptor;
struct RenderPassLayout;
struct RenderPipelineDescriptor;
struct RequestAdapterOptions;
struct SamplerBindingLayout;
struct SamplerDescriptor;
struct ShaderModuleCompilationHint;
struct ShaderModuleDescriptor;
struct StencilFaceState;
struct StorageTextureBindingLayout;
struct SupportedFeatures;
struct SupportedLimits;
struct TextureBindingLayout;
struct TextureDescriptor;
struct TextureViewDescriptor;
struct ValidationError;
struct VertexAttribute;
struct VertexBufferLayout;
struct VertexState;

class ConvertFromBackingContext {
public:
    virtual ~ConvertFromBackingContext() = default;

    std::optional<WebCore::WebGPU::BindGroupDescriptor> convertFromBacking(const BindGroupDescriptor&);
    std::optional<WebCore::WebGPU::BindGroupEntry> convertFromBacking(const BindGroupEntry&);
    std::optional<WebCore::WebGPU::BindGroupLayoutDescriptor> convertFromBacking(const BindGroupLayoutDescriptor&);
    std::optional<WebCore::WebGPU::BindGroupLayoutEntry> convertFromBacking(const BindGroupLayoutEntry&);
    std::optional<WebCore::WebGPU::BlendComponent> convertFromBacking(const BlendComponent&);
    std::optional<WebCore::WebGPU::BlendState> convertFromBacking(const BlendState&);
    std::optional<WebCore::WebGPU::BufferBinding> convertFromBacking(const BufferBinding&);
    std::optional<WebCore::WebGPU::BufferBindingLayout> convertFromBacking(const BufferBindingLayout&);
    std::optional<WebCore::WebGPU::BufferDescriptor> convertFromBacking(const BufferDescriptor&);
    std::optional<WebCore::WebGPU::CanvasConfiguration> convertFromBacking(const CanvasConfiguration&);
    std::optional<WebCore::WebGPU::Color> convertFromBacking(const Color&);
    std::optional<WebCore::WebGPU::ColorDict> convertFromBacking(const ColorDict&);
    std::optional<WebCore::WebGPU::ColorTargetState> convertFromBacking(const ColorTargetState&);
    std::optional<WebCore::WebGPU::CommandBufferDescriptor> convertFromBacking(const CommandBufferDescriptor&);
    std::optional<WebCore::WebGPU::CommandEncoderDescriptor> convertFromBacking(const CommandEncoderDescriptor&);
    RefPtr<WebCore::WebGPU::CompilationMessage> convertFromBacking(const CompilationMessage&);
    std::optional<WebCore::WebGPU::ComputePassDescriptor> convertFromBacking(const ComputePassDescriptor&);
    std::optional<WebCore::WebGPU::ComputePassTimestampWrites> convertFromBacking(const ComputePassTimestampWrites&);
    std::optional<WebCore::WebGPU::ComputePipelineDescriptor> convertFromBacking(const ComputePipelineDescriptor&);
    std::optional<WebCore::WebGPU::DepthStencilState> convertFromBacking(const DepthStencilState&);
    std::optional<WebCore::WebGPU::DeviceDescriptor> convertFromBacking(const DeviceDescriptor&);
    std::optional<WebCore::WebGPU::Error> convertFromBacking(const Error&);
    std::optional<WebCore::WebGPU::Extent3D> convertFromBacking(const Extent3D&);
    std::optional<WebCore::WebGPU::Extent3DDict> convertFromBacking(const Extent3DDict&);
    std::optional<WebCore::WebGPU::ExternalTextureBindingLayout> convertFromBacking(const ExternalTextureBindingLayout&);
#if ENABLE(VIDEO) && PLATFORM(COCOA)
    using PixelBufferType = RetainPtr<CVPixelBufferRef>;
#else
    using PixelBufferType = void*;
#endif
    std::optional<WebCore::WebGPU::ExternalTextureDescriptor> convertFromBacking(const ExternalTextureDescriptor&, PixelBufferType);
    std::optional<WebCore::WebGPU::FragmentState> convertFromBacking(const FragmentState&);
    std::optional<WebCore::WebGPU::Identifier> convertFromBacking(const Identifier&);
    std::optional<WebCore::WebGPU::ImageCopyBuffer> convertFromBacking(const ImageCopyBuffer&);
    std::optional<WebCore::WebGPU::ImageCopyExternalImage> convertFromBacking(const ImageCopyExternalImage&);
    std::optional<WebCore::WebGPU::ImageCopyTexture> convertFromBacking(const ImageCopyTexture&);
    std::optional<WebCore::WebGPU::ImageCopyTextureTagged> convertFromBacking(const ImageCopyTextureTagged&);
    std::optional<WebCore::WebGPU::ImageDataLayout> convertFromBacking(const ImageDataLayout&);
    RefPtr<WebCore::WebGPU::InternalError> convertFromBacking(const InternalError&);
    std::optional<WebCore::WebGPU::MultisampleState> convertFromBacking(const MultisampleState&);
    std::optional<WebCore::WebGPU::ObjectDescriptorBase> convertFromBacking(const ObjectDescriptorBase&);
    std::optional<WebCore::WebGPU::Origin2D> convertFromBacking(const Origin2D&);
    std::optional<WebCore::WebGPU::Origin2DDict> convertFromBacking(const Origin2DDict&);
    std::optional<WebCore::WebGPU::Origin3D> convertFromBacking(const Origin3D&);
    std::optional<WebCore::WebGPU::Origin3DDict> convertFromBacking(const Origin3DDict&);
    RefPtr<WebCore::WebGPU::OutOfMemoryError> convertFromBacking(const OutOfMemoryError&);
    std::optional<WebCore::WebGPU::PipelineDescriptorBase> convertFromBacking(const PipelineDescriptorBase&);
    std::optional<WebCore::WebGPU::PipelineLayoutDescriptor> convertFromBacking(const PipelineLayoutDescriptor&);
    std::optional<WebCore::WebGPU::PresentationContextDescriptor> convertFromBacking(const PresentationContextDescriptor&);
    std::optional<WebCore::WebGPU::PrimitiveState> convertFromBacking(const PrimitiveState&);
    std::optional<WebCore::WebGPU::ProgrammableStage> convertFromBacking(const ProgrammableStage&);
    std::optional<WebCore::WebGPU::QuerySetDescriptor> convertFromBacking(const QuerySetDescriptor&);
    std::optional<WebCore::WebGPU::RenderBundleDescriptor> convertFromBacking(const RenderBundleDescriptor&);
    std::optional<WebCore::WebGPU::RenderBundleEncoderDescriptor> convertFromBacking(const RenderBundleEncoderDescriptor&);
    std::optional<WebCore::WebGPU::RenderPassColorAttachment> convertFromBacking(const RenderPassColorAttachment&);
    std::optional<WebCore::WebGPU::RenderPassDepthStencilAttachment> convertFromBacking(const RenderPassDepthStencilAttachment&);
    std::optional<WebCore::WebGPU::RenderPassDescriptor> convertFromBacking(const RenderPassDescriptor&);
    std::optional<WebCore::WebGPU::RenderPassLayout> convertFromBacking(const RenderPassLayout&);
    std::optional<WebCore::WebGPU::RenderPassTimestampWrites> convertFromBacking(const RenderPassTimestampWrites&);
    std::optional<WebCore::WebGPU::RenderPipelineDescriptor> convertFromBacking(const RenderPipelineDescriptor&);
    std::optional<WebCore::WebGPU::RequestAdapterOptions> convertFromBacking(const RequestAdapterOptions&);
    std::optional<WebCore::WebGPU::SamplerBindingLayout> convertFromBacking(const SamplerBindingLayout&);
    std::optional<WebCore::WebGPU::SamplerDescriptor> convertFromBacking(const SamplerDescriptor&);
    std::optional<WebCore::WebGPU::ShaderModuleCompilationHint> convertFromBacking(const ShaderModuleCompilationHint&);
    std::optional<WebCore::WebGPU::ShaderModuleDescriptor> convertFromBacking(const ShaderModuleDescriptor&);
    std::optional<WebCore::WebGPU::StencilFaceState> convertFromBacking(const StencilFaceState&);
    std::optional<WebCore::WebGPU::StorageTextureBindingLayout> convertFromBacking(const StorageTextureBindingLayout&);
    RefPtr<WebCore::WebGPU::SupportedFeatures> convertFromBacking(const SupportedFeatures&);
    RefPtr<WebCore::WebGPU::SupportedLimits> convertFromBacking(const SupportedLimits&);
    std::optional<WebCore::WebGPU::TextureBindingLayout> convertFromBacking(const TextureBindingLayout&);
    std::optional<WebCore::WebGPU::TextureDescriptor> convertFromBacking(const TextureDescriptor&);
    std::optional<WebCore::WebGPU::TextureViewDescriptor> convertFromBacking(const TextureViewDescriptor&);
    RefPtr<WebCore::WebGPU::ValidationError> convertFromBacking(const ValidationError&);
    std::optional<WebCore::WebGPU::VertexAttribute> convertFromBacking(const VertexAttribute&);
    std::optional<WebCore::WebGPU::VertexBufferLayout> convertFromBacking(const VertexBufferLayout&);
    std::optional<WebCore::WebGPU::VertexState> convertFromBacking(const VertexState&);

    virtual WebCore::WebGPU::Adapter* convertAdapterFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::BindGroup* convertBindGroupFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::BindGroupLayout* convertBindGroupLayoutFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::Buffer* convertBufferFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::CommandBuffer* convertCommandBufferFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::CommandEncoder* convertCommandEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::CompositorIntegration* convertCompositorIntegrationFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::ComputePassEncoder* convertComputePassEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::ComputePipeline* convertComputePipelineFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::Device* convertDeviceFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::ExternalTexture* convertExternalTextureFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::PipelineLayout* convertPipelineLayoutFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::QuerySet* convertQuerySetFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::Queue* convertQueueFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::RenderBundleEncoder* convertRenderBundleEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::RenderBundle* convertRenderBundleFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::RenderPassEncoder* convertRenderPassEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::RenderPipeline* convertRenderPipelineFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::Sampler* convertSamplerFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::ShaderModule* convertShaderModuleFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::PresentationContext* convertPresentationContextFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::Texture* convertTextureFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::TextureView* convertTextureViewFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::XRBinding* convertXRBindingFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::XRProjectionLayer* convertXRProjectionLayerFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::XRSubImage* convertXRSubImageFromBacking(WebGPUIdentifier) = 0;
    virtual WebCore::WebGPU::XRView* createXRViewFromBacking(WebGPUIdentifier) = 0;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
