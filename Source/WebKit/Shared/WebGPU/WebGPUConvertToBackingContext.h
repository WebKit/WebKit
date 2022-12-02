/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
struct ComputePassDescriptor;
class ComputePassEncoder;
class ComputePipeline;
struct ComputePipelineDescriptor;
struct DepthStencilState;
class Device;
struct DeviceDescriptor;
class ExternalTexture;
struct ExternalTextureBindingLayout;
struct ExternalTextureDescriptor;
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
class Surface;
struct SurfaceDescriptor;
class SwapChain;
struct SwapChainDescriptor;
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
struct SurfaceDescriptor;
struct SwapChainDescriptor;
struct TextureBindingLayout;
struct TextureDescriptor;
struct TextureViewDescriptor;
struct ValidationError;
struct VertexAttribute;
struct VertexBufferLayout;
struct VertexState;

class ConvertToBackingContext : public RefCounted<ConvertToBackingContext> {
public:
    virtual ~ConvertToBackingContext() = default;

    std::optional<BindGroupDescriptor> convertToBacking(const PAL::WebGPU::BindGroupDescriptor&);
    std::optional<BindGroupEntry> convertToBacking(const PAL::WebGPU::BindGroupEntry&);
    std::optional<BindGroupLayoutDescriptor> convertToBacking(const PAL::WebGPU::BindGroupLayoutDescriptor&);
    std::optional<BindGroupLayoutEntry> convertToBacking(const PAL::WebGPU::BindGroupLayoutEntry&);
    std::optional<BlendComponent> convertToBacking(const PAL::WebGPU::BlendComponent&);
    std::optional<BlendState> convertToBacking(const PAL::WebGPU::BlendState&);
    std::optional<BufferBinding> convertToBacking(const PAL::WebGPU::BufferBinding&);
    std::optional<BufferBindingLayout> convertToBacking(const PAL::WebGPU::BufferBindingLayout&);
    std::optional<BufferDescriptor> convertToBacking(const PAL::WebGPU::BufferDescriptor&);
    std::optional<CanvasConfiguration> convertToBacking(const PAL::WebGPU::CanvasConfiguration&);
    std::optional<Color> convertToBacking(const PAL::WebGPU::Color&);
    std::optional<ColorDict> convertToBacking(const PAL::WebGPU::ColorDict&);
    std::optional<ColorTargetState> convertToBacking(const PAL::WebGPU::ColorTargetState&);
    std::optional<CommandBufferDescriptor> convertToBacking(const PAL::WebGPU::CommandBufferDescriptor&);
    std::optional<CommandEncoderDescriptor> convertToBacking(const PAL::WebGPU::CommandEncoderDescriptor&);
    std::optional<CompilationMessage> convertToBacking(const PAL::WebGPU::CompilationMessage&);
    std::optional<ComputePassDescriptor> convertToBacking(const PAL::WebGPU::ComputePassDescriptor&);
    std::optional<ComputePassTimestampWrite> convertToBacking(const PAL::WebGPU::ComputePassTimestampWrite&);
    std::optional<ComputePassTimestampWrites> convertToBacking(const PAL::WebGPU::ComputePassTimestampWrites&);
    std::optional<ComputePipelineDescriptor> convertToBacking(const PAL::WebGPU::ComputePipelineDescriptor&);
    std::optional<DepthStencilState> convertToBacking(const PAL::WebGPU::DepthStencilState&);
    std::optional<DeviceDescriptor> convertToBacking(const PAL::WebGPU::DeviceDescriptor&);
    std::optional<Error> convertToBacking(const PAL::WebGPU::Error&);
    std::optional<Extent3D> convertToBacking(const PAL::WebGPU::Extent3D&);
    std::optional<Extent3DDict> convertToBacking(const PAL::WebGPU::Extent3DDict&);
    std::optional<ExternalTextureBindingLayout> convertToBacking(const PAL::WebGPU::ExternalTextureBindingLayout&);
    std::optional<ExternalTextureDescriptor> convertToBacking(const PAL::WebGPU::ExternalTextureDescriptor&);
    std::optional<FragmentState> convertToBacking(const PAL::WebGPU::FragmentState&);
    std::optional<Identifier> convertToBacking(const PAL::WebGPU::Identifier&);
    std::optional<ImageCopyBuffer> convertToBacking(const PAL::WebGPU::ImageCopyBuffer&);
    std::optional<ImageCopyExternalImage> convertToBacking(const PAL::WebGPU::ImageCopyExternalImage&);
    std::optional<ImageCopyTexture> convertToBacking(const PAL::WebGPU::ImageCopyTexture&);
    std::optional<ImageCopyTextureTagged> convertToBacking(const PAL::WebGPU::ImageCopyTextureTagged&);
    std::optional<ImageDataLayout> convertToBacking(const PAL::WebGPU::ImageDataLayout&);
    std::optional<MultisampleState> convertToBacking(const PAL::WebGPU::MultisampleState&);
    std::optional<ObjectDescriptorBase> convertToBacking(const PAL::WebGPU::ObjectDescriptorBase&);
    std::optional<Origin2D> convertToBacking(const PAL::WebGPU::Origin2D&);
    std::optional<Origin2DDict> convertToBacking(const PAL::WebGPU::Origin2DDict&);
    std::optional<Origin3D> convertToBacking(const PAL::WebGPU::Origin3D&);
    std::optional<Origin3DDict> convertToBacking(const PAL::WebGPU::Origin3DDict&);
    std::optional<OutOfMemoryError> convertToBacking(const PAL::WebGPU::OutOfMemoryError&);
    std::optional<PipelineDescriptorBase> convertToBacking(const PAL::WebGPU::PipelineDescriptorBase&);
    std::optional<PipelineLayoutDescriptor> convertToBacking(const PAL::WebGPU::PipelineLayoutDescriptor&);
    std::optional<PrimitiveState> convertToBacking(const PAL::WebGPU::PrimitiveState&);
    std::optional<ProgrammableStage> convertToBacking(const PAL::WebGPU::ProgrammableStage&);
    std::optional<QuerySetDescriptor> convertToBacking(const PAL::WebGPU::QuerySetDescriptor&);
    std::optional<RenderBundleDescriptor> convertToBacking(const PAL::WebGPU::RenderBundleDescriptor&);
    std::optional<RenderBundleEncoderDescriptor> convertToBacking(const PAL::WebGPU::RenderBundleEncoderDescriptor&);
    std::optional<RenderPassColorAttachment> convertToBacking(const PAL::WebGPU::RenderPassColorAttachment&);
    std::optional<RenderPassDepthStencilAttachment> convertToBacking(const PAL::WebGPU::RenderPassDepthStencilAttachment&);
    std::optional<RenderPassDescriptor> convertToBacking(const PAL::WebGPU::RenderPassDescriptor&);
    std::optional<RenderPassLayout> convertToBacking(const PAL::WebGPU::RenderPassLayout&);
    std::optional<RenderPassTimestampWrite> convertToBacking(const PAL::WebGPU::RenderPassTimestampWrite&);
    std::optional<RenderPassTimestampWrites> convertToBacking(const PAL::WebGPU::RenderPassTimestampWrites&);
    std::optional<RenderPipelineDescriptor> convertToBacking(const PAL::WebGPU::RenderPipelineDescriptor&);
    std::optional<RequestAdapterOptions> convertToBacking(const PAL::WebGPU::RequestAdapterOptions&);
    std::optional<SamplerBindingLayout> convertToBacking(const PAL::WebGPU::SamplerBindingLayout&);
    std::optional<SamplerDescriptor> convertToBacking(const PAL::WebGPU::SamplerDescriptor&);
    std::optional<ShaderModuleCompilationHint> convertToBacking(const PAL::WebGPU::ShaderModuleCompilationHint&);
    std::optional<ShaderModuleDescriptor> convertToBacking(const PAL::WebGPU::ShaderModuleDescriptor&);
    std::optional<StencilFaceState> convertToBacking(const PAL::WebGPU::StencilFaceState&);
    std::optional<StorageTextureBindingLayout> convertToBacking(const PAL::WebGPU::StorageTextureBindingLayout&);
    std::optional<SupportedFeatures> convertToBacking(const PAL::WebGPU::SupportedFeatures&);
    std::optional<SupportedLimits> convertToBacking(const PAL::WebGPU::SupportedLimits&);
    std::optional<SurfaceDescriptor> convertToBacking(const PAL::WebGPU::SurfaceDescriptor&);
    std::optional<SwapChainDescriptor> convertToBacking(const PAL::WebGPU::SwapChainDescriptor&);
    std::optional<TextureBindingLayout> convertToBacking(const PAL::WebGPU::TextureBindingLayout&);
    std::optional<TextureDescriptor> convertToBacking(const PAL::WebGPU::TextureDescriptor&);
    std::optional<TextureViewDescriptor> convertToBacking(const PAL::WebGPU::TextureViewDescriptor&);
    std::optional<ValidationError> convertToBacking(const PAL::WebGPU::ValidationError&);
    std::optional<VertexAttribute> convertToBacking(const PAL::WebGPU::VertexAttribute&);
    std::optional<VertexBufferLayout> convertToBacking(const PAL::WebGPU::VertexBufferLayout&);
    std::optional<VertexState> convertToBacking(const PAL::WebGPU::VertexState&);

    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Adapter&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::BindGroup&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::BindGroupLayout&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Buffer&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::CommandBuffer&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::CommandEncoder&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::ComputePassEncoder&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::ComputePipeline&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Device&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::ExternalTexture&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::GPU&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::PipelineLayout&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::QuerySet&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Queue&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderBundleEncoder&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderBundle&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderPassEncoder&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderPipeline&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Sampler&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::ShaderModule&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Surface&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::SwapChain&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Texture&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::TextureView&) = 0;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
