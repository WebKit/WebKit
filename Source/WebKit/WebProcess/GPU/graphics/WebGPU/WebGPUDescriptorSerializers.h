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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "WebGPUIdentifier.h"
#include "WebGPURenderPassTimestampWrites.h"
#include <WebCore/WebGPUCppAPI.h>
#include <optional>

namespace WebCore {
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
struct BufferBinding;
struct CanvasConfiguration;
struct ComputePassDescriptor;
struct ComputePipelineDescriptor;
struct DeviceDescriptor;
struct ExternalTextureDescriptor;
struct FragmentState;
struct ImageCopyBuffer;
struct ImageCopyExternalImage;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
struct ImageCopyExternalImageVideoSource;
#endif
struct ImageCopyTexture;
struct ImageCopyTextureTagged;
struct ObjectDescriptorBase;
struct PipelineLayoutDescriptor;
struct PresentationContextDescriptor;
struct ProgrammableStage;
struct RenderBundleEncoderDescriptor;
struct RenderPassColorAttachment;
struct RenderPassDepthStencilAttachment;
struct RenderPassDescriptor;
struct RenderPipelineDescriptor;
struct ShaderModuleDescriptor;
struct TextureDescriptor;
struct VertexBufferLayout;
struct VertexState;

// Serializes the Web Process descriptors, replacing each object with its identifier in the GPU Process.
// Returns std::nullopt if the descriptor cannot be serialized.
std::optional<BindGroupDescriptor> serialize(const ::WebGPU::BindGroupDescriptor&);
std::optional<BindGroupEntry> serialize(const ::WebGPU::BindGroupEntry&);
std::optional<BindGroupLayoutDescriptor> serialize(const ::WebGPU::BindGroupLayoutDescriptor&);
std::optional<BufferBinding> serialize(const ::WebGPU::BufferBinding&);
std::optional<CanvasConfiguration> serialize(const ::WebGPU::CanvasConfiguration&);
std::optional<ComputePassDescriptor> serialize(const ::WebGPU::ComputePassDescriptor&);
std::optional<ComputePipelineDescriptor> serialize(const ::WebGPU::ComputePipelineDescriptor&);
std::optional<DeviceDescriptor> serialize(const ::WebGPU::DeviceDescriptor&);
std::optional<ExternalTextureDescriptor> serialize(const WebCore::WebGPUExternalTextureDescriptor&);
std::optional<FragmentState> serialize(const ::WebGPU::FragmentState&);
std::optional<ImageCopyBuffer> serialize(const ::WebGPU::TexelCopyBufferInfo&);
std::optional<ImageCopyExternalImage> serialize(const WebCore::WebGPUExternalImageSource&);
#if PLATFORM(COCOA) && ENABLE(VIDEO)
std::optional<ImageCopyExternalImageVideoSource> serializeVideoSource(const WebCore::WebGPUExternalImageSource&);
#endif
std::optional<ImageCopyTexture> serialize(const ::WebGPU::TexelCopyTextureInfo&);
std::optional<ImageCopyTextureTagged> serialize(const WebCore::WebGPUImageCopyTextureTagged&);
NODELETE std::optional<ObjectDescriptorBase> serialize(const WebCore::WebGPUObjectDescriptorBase&);
std::optional<PipelineLayoutDescriptor> serialize(const ::WebGPU::PipelineLayoutDescriptor&);
std::optional<PresentationContextDescriptor> serialize(const WebCore::WebGPUPresentationContextDescriptor&);
std::optional<ProgrammableStage> serialize(const ::WebGPU::ProgrammableStage&);
std::optional<RenderBundleEncoderDescriptor> serialize(const ::WebGPU::RenderBundleEncoderDescriptor&);
std::optional<RenderPassColorAttachment> serialize(const ::WebGPU::RenderPassColorAttachment&);
std::optional<RenderPassDepthStencilAttachment> serialize(const ::WebGPU::RenderPassDepthStencilAttachment&);
std::optional<RenderPassDescriptor> serialize(const ::WebGPU::RenderPassDescriptor&);
std::optional<RenderPassTimestampWrites> serialize(const ::WebGPU::PassTimestampWrites&);
std::optional<RenderPipelineDescriptor> serialize(const ::WebGPU::RenderPipelineDescriptor&);
std::optional<ShaderModuleDescriptor> serialize(const ::WebGPU::ShaderModuleDescriptor&);
std::optional<TextureDescriptor> serialize(const ::WebGPU::TextureDescriptor&);
std::optional<VertexBufferLayout> serialize(const ::WebGPU::VertexBufferLayout&);
std::optional<VertexState> serialize(const ::WebGPU::VertexState&);

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
