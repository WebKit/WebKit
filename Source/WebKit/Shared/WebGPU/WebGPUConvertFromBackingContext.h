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
#include <optional>
#include <pal/graphics/WebGPU/WebGPUColor.h>
#include <pal/graphics/WebGPU/WebGPUComputePassTimestampWrites.h>
#include <pal/graphics/WebGPU/WebGPUError.h>
#include <pal/graphics/WebGPU/WebGPUExtent3D.h>
#include <pal/graphics/WebGPU/WebGPUOrigin2D.h>
#include <pal/graphics/WebGPU/WebGPUOrigin3D.h>
#include <pal/graphics/WebGPU/WebGPURenderPassTimestampWrites.h>
#include <wtf/RefCounted.h>

namespace PAL::WebGPU {

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
struct MultisampleState;
struct ObjectDescriptorBase;
class OutOfMemoryError;
struct PipelineDescriptorBase;
class PipelineLayout;
struct PipelineLayoutDescriptor;
struct PresentationConfiguration;
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

} // namespace PAL::WebGPU

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
struct MultisampleState;
struct ObjectDescriptorBase;
struct OutOfMemoryError;
struct PipelineDescriptorBase;
struct PipelineLayoutDescriptor;
struct PresentationConfiguration;
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

    std::optional<PAL::WebGPU::BindGroupDescriptor> convertFromBacking(const BindGroupDescriptor&);
    std::optional<PAL::WebGPU::BindGroupEntry> convertFromBacking(const BindGroupEntry&);
    std::optional<PAL::WebGPU::BindGroupLayoutDescriptor> convertFromBacking(const BindGroupLayoutDescriptor&);
    std::optional<PAL::WebGPU::BindGroupLayoutEntry> convertFromBacking(const BindGroupLayoutEntry&);
    std::optional<PAL::WebGPU::BlendComponent> convertFromBacking(const BlendComponent&);
    std::optional<PAL::WebGPU::BlendState> convertFromBacking(const BlendState&);
    std::optional<PAL::WebGPU::BufferBinding> convertFromBacking(const BufferBinding&);
    std::optional<PAL::WebGPU::BufferBindingLayout> convertFromBacking(const BufferBindingLayout&);
    std::optional<PAL::WebGPU::BufferDescriptor> convertFromBacking(const BufferDescriptor&);
    std::optional<PAL::WebGPU::CanvasConfiguration> convertFromBacking(const CanvasConfiguration&);
    std::optional<PAL::WebGPU::Color> convertFromBacking(const Color&);
    std::optional<PAL::WebGPU::ColorDict> convertFromBacking(const ColorDict&);
    std::optional<PAL::WebGPU::ColorTargetState> convertFromBacking(const ColorTargetState&);
    std::optional<PAL::WebGPU::CommandBufferDescriptor> convertFromBacking(const CommandBufferDescriptor&);
    std::optional<PAL::WebGPU::CommandEncoderDescriptor> convertFromBacking(const CommandEncoderDescriptor&);
    RefPtr<PAL::WebGPU::CompilationMessage> convertFromBacking(const CompilationMessage&);
    std::optional<PAL::WebGPU::ComputePassDescriptor> convertFromBacking(const ComputePassDescriptor&);
    std::optional<PAL::WebGPU::ComputePassTimestampWrite> convertFromBacking(const ComputePassTimestampWrite&);
    std::optional<PAL::WebGPU::ComputePassTimestampWrites> convertFromBacking(const ComputePassTimestampWrites&);
    std::optional<PAL::WebGPU::ComputePipelineDescriptor> convertFromBacking(const ComputePipelineDescriptor&);
    std::optional<PAL::WebGPU::DepthStencilState> convertFromBacking(const DepthStencilState&);
    std::optional<PAL::WebGPU::DeviceDescriptor> convertFromBacking(const DeviceDescriptor&);
    std::optional<PAL::WebGPU::Error> convertFromBacking(const Error&);
    std::optional<PAL::WebGPU::Extent3D> convertFromBacking(const Extent3D&);
    std::optional<PAL::WebGPU::Extent3DDict> convertFromBacking(const Extent3DDict&);
    std::optional<PAL::WebGPU::ExternalTextureBindingLayout> convertFromBacking(const ExternalTextureBindingLayout&);
    std::optional<PAL::WebGPU::ExternalTextureDescriptor> convertFromBacking(const ExternalTextureDescriptor&);
    std::optional<PAL::WebGPU::FragmentState> convertFromBacking(const FragmentState&);
    std::optional<PAL::WebGPU::Identifier> convertFromBacking(const Identifier&);
    std::optional<PAL::WebGPU::ImageCopyBuffer> convertFromBacking(const ImageCopyBuffer&);
    std::optional<PAL::WebGPU::ImageCopyExternalImage> convertFromBacking(const ImageCopyExternalImage&);
    std::optional<PAL::WebGPU::ImageCopyTexture> convertFromBacking(const ImageCopyTexture&);
    std::optional<PAL::WebGPU::ImageCopyTextureTagged> convertFromBacking(const ImageCopyTextureTagged&);
    std::optional<PAL::WebGPU::ImageDataLayout> convertFromBacking(const ImageDataLayout&);
    std::optional<PAL::WebGPU::MultisampleState> convertFromBacking(const MultisampleState&);
    std::optional<PAL::WebGPU::ObjectDescriptorBase> convertFromBacking(const ObjectDescriptorBase&);
    std::optional<PAL::WebGPU::Origin2D> convertFromBacking(const Origin2D&);
    std::optional<PAL::WebGPU::Origin2DDict> convertFromBacking(const Origin2DDict&);
    std::optional<PAL::WebGPU::Origin3D> convertFromBacking(const Origin3D&);
    std::optional<PAL::WebGPU::Origin3DDict> convertFromBacking(const Origin3DDict&);
    RefPtr<PAL::WebGPU::OutOfMemoryError> convertFromBacking(const OutOfMemoryError&);
    std::optional<PAL::WebGPU::PipelineDescriptorBase> convertFromBacking(const PipelineDescriptorBase&);
    std::optional<PAL::WebGPU::PipelineLayoutDescriptor> convertFromBacking(const PipelineLayoutDescriptor&);
    std::optional<PAL::WebGPU::PresentationConfiguration> convertFromBacking(const PresentationConfiguration&);
    std::optional<PAL::WebGPU::PresentationContextDescriptor> convertFromBacking(const PresentationContextDescriptor&);
    std::optional<PAL::WebGPU::PrimitiveState> convertFromBacking(const PrimitiveState&);
    std::optional<PAL::WebGPU::ProgrammableStage> convertFromBacking(const ProgrammableStage&);
    std::optional<PAL::WebGPU::QuerySetDescriptor> convertFromBacking(const QuerySetDescriptor&);
    std::optional<PAL::WebGPU::RenderBundleDescriptor> convertFromBacking(const RenderBundleDescriptor&);
    std::optional<PAL::WebGPU::RenderBundleEncoderDescriptor> convertFromBacking(const RenderBundleEncoderDescriptor&);
    std::optional<PAL::WebGPU::RenderPassColorAttachment> convertFromBacking(const RenderPassColorAttachment&);
    std::optional<PAL::WebGPU::RenderPassDepthStencilAttachment> convertFromBacking(const RenderPassDepthStencilAttachment&);
    std::optional<PAL::WebGPU::RenderPassDescriptor> convertFromBacking(const RenderPassDescriptor&);
    std::optional<PAL::WebGPU::RenderPassLayout> convertFromBacking(const RenderPassLayout&);
    std::optional<PAL::WebGPU::RenderPassTimestampWrite> convertFromBacking(const RenderPassTimestampWrite&);
    std::optional<PAL::WebGPU::RenderPassTimestampWrites> convertFromBacking(const RenderPassTimestampWrites&);
    std::optional<PAL::WebGPU::RenderPipelineDescriptor> convertFromBacking(const RenderPipelineDescriptor&);
    std::optional<PAL::WebGPU::RequestAdapterOptions> convertFromBacking(const RequestAdapterOptions&);
    std::optional<PAL::WebGPU::SamplerBindingLayout> convertFromBacking(const SamplerBindingLayout&);
    std::optional<PAL::WebGPU::SamplerDescriptor> convertFromBacking(const SamplerDescriptor&);
    std::optional<PAL::WebGPU::ShaderModuleCompilationHint> convertFromBacking(const ShaderModuleCompilationHint&);
    std::optional<PAL::WebGPU::ShaderModuleDescriptor> convertFromBacking(const ShaderModuleDescriptor&);
    std::optional<PAL::WebGPU::StencilFaceState> convertFromBacking(const StencilFaceState&);
    std::optional<PAL::WebGPU::StorageTextureBindingLayout> convertFromBacking(const StorageTextureBindingLayout&);
    RefPtr<PAL::WebGPU::SupportedFeatures> convertFromBacking(const SupportedFeatures&);
    RefPtr<PAL::WebGPU::SupportedLimits> convertFromBacking(const SupportedLimits&);
    std::optional<PAL::WebGPU::TextureBindingLayout> convertFromBacking(const TextureBindingLayout&);
    std::optional<PAL::WebGPU::TextureDescriptor> convertFromBacking(const TextureDescriptor&);
    std::optional<PAL::WebGPU::TextureViewDescriptor> convertFromBacking(const TextureViewDescriptor&);
    RefPtr<PAL::WebGPU::ValidationError> convertFromBacking(const ValidationError&);
    std::optional<PAL::WebGPU::VertexAttribute> convertFromBacking(const VertexAttribute&);
    std::optional<PAL::WebGPU::VertexBufferLayout> convertFromBacking(const VertexBufferLayout&);
    std::optional<PAL::WebGPU::VertexState> convertFromBacking(const VertexState&);

    virtual PAL::WebGPU::Adapter* convertAdapterFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::BindGroup* convertBindGroupFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::BindGroupLayout* convertBindGroupLayoutFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::Buffer* convertBufferFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::CommandBuffer* convertCommandBufferFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::CommandEncoder* convertCommandEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::CompositorIntegration* convertCompositorIntegrationFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::ComputePassEncoder* convertComputePassEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::ComputePipeline* convertComputePipelineFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::Device* convertDeviceFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::ExternalTexture* convertExternalTextureFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::PipelineLayout* convertPipelineLayoutFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::QuerySet* convertQuerySetFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::Queue* convertQueueFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::RenderBundleEncoder* convertRenderBundleEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::RenderBundle* convertRenderBundleFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::RenderPassEncoder* convertRenderPassEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::RenderPipeline* convertRenderPipelineFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::Sampler* convertSamplerFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::ShaderModule* convertShaderModuleFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::PresentationContext* convertPresentationContextFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::Texture* convertTextureFromBacking(WebGPUIdentifier) = 0;
    virtual PAL::WebGPU::TextureView* convertTextureViewFromBacking(WebGPUIdentifier) = 0;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
