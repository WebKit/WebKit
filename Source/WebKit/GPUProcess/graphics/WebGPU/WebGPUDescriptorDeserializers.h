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

#include "WebGPURenderPassTimestampWrites.h"
#include <WebCore/WebGPUCppAPI.h>
#include <optional>
#include <wtf/Vector.h>

#if ENABLE(VIDEO) && PLATFORM(COCOA)
#include <wtf/RetainPtr.h>

typedef struct CF_BRIDGED_TYPE(id) __CVBuffer* CVPixelBufferRef;

namespace WebCore {
enum class VideoFrameRotation : uint16_t;
}
#endif

namespace WebCore {
struct WebGPUExternalImageSource;
struct WebGPUExternalTextureDescriptor;
struct WebGPUImageCopyTextureTagged;
struct WebGPUObjectDescriptorBase;
struct WebGPUPresentationContextDescriptor;
}

namespace WebKit::WebGPU {

class ObjectHeap;
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

// Owns the arrays that a deserialized WebGPU::RenderPipelineDescriptor borrows.
struct RenderPipelineDescriptorStorage {
    Vector<::WebGPU::ConstantEntry> vertexConstants;
    Vector<Vector<::WebGPU::VertexAttribute>> vertexAttributes;
    Vector<std::optional<::WebGPU::VertexBufferLayout>> vertexBuffers;
    Vector<::WebGPU::ConstantEntry> fragmentConstants;
    Vector<std::optional<::WebGPU::ColorTargetState>> fragmentTargets;
};

#if ENABLE(VIDEO) && PLATFORM(COCOA)
using PixelBufferType = RetainPtr<CVPixelBufferRef>;
#else
using PixelBufferType = void*;
#endif

// Deserializes the descriptors sent by the Web Process, looking up each object identifier in the ObjectHeap.
// Returns std::nullopt if the descriptor cannot be deserialized.
std::optional<::WebGPU::BindGroupDescriptor> deserialize(ObjectHeap&, const BindGroupDescriptor&, Vector<::WebGPU::BindGroupEntry>& entriesStorage);
std::optional<::WebGPU::BindGroupEntry> deserialize(ObjectHeap&, const BindGroupEntry&);
std::optional<::WebGPU::BindGroupLayoutDescriptor> deserialize(ObjectHeap&, const BindGroupLayoutDescriptor&, Vector<::WebGPU::BindGroupLayoutEntry>& entriesStorage);
std::optional<::WebGPU::BufferBinding> deserialize(ObjectHeap&, const BufferBinding&);
std::optional<::WebGPU::CanvasConfiguration> deserialize(ObjectHeap&, const CanvasConfiguration&);
std::optional<::WebGPU::ComputePassDescriptor> deserialize(ObjectHeap&, const ComputePassDescriptor&);
std::optional<::WebGPU::ComputePipelineDescriptor> deserialize(ObjectHeap&, const ComputePipelineDescriptor&, Vector<::WebGPU::ConstantEntry>& constantsStorage, bool allowMissingPipelineLayout = false);
std::optional<::WebGPU::DeviceDescriptor> deserialize(ObjectHeap&, const DeviceDescriptor&);
std::optional<WebCore::WebGPUExternalTextureDescriptor> deserialize(ObjectHeap&, const ExternalTextureDescriptor&, PixelBufferType);
std::optional<::WebGPU::FragmentState> deserialize(ObjectHeap&, const FragmentState&, RenderPipelineDescriptorStorage&);
std::optional<::WebGPU::TexelCopyBufferInfo> deserialize(ObjectHeap&, const ImageCopyBuffer&);
std::optional<WebCore::WebGPUExternalImageSource> deserialize(ObjectHeap&, const ImageCopyExternalImage&);
#if PLATFORM(COCOA) && ENABLE(VIDEO)
std::optional<WebCore::WebGPUExternalImageSource> deserialize(ObjectHeap&, const ImageCopyExternalImageVideoSource&, PixelBufferType, WebCore::VideoFrameRotation, bool isMirrored);
#endif
std::optional<::WebGPU::TexelCopyTextureInfo> deserialize(ObjectHeap&, const ImageCopyTexture&);
std::optional<WebCore::WebGPUImageCopyTextureTagged> deserialize(ObjectHeap&, const ImageCopyTextureTagged&);
NODELETE std::optional<WebCore::WebGPUObjectDescriptorBase> deserialize(ObjectHeap&, const ObjectDescriptorBase&);
std::optional<::WebGPU::PipelineLayoutDescriptor> deserialize(ObjectHeap&, const PipelineLayoutDescriptor&, Vector<Ref<::WebGPU::BindGroupLayout>>& bindGroupLayoutsStorage);
std::optional<WebCore::WebGPUPresentationContextDescriptor> deserialize(ObjectHeap&, const PresentationContextDescriptor&);
std::optional<::WebGPU::ProgrammableStage> deserialize(ObjectHeap&, const ProgrammableStage&, Vector<::WebGPU::ConstantEntry>& constantsStorage);
std::optional<::WebGPU::RenderBundleEncoderDescriptor> deserialize(ObjectHeap&, const RenderBundleEncoderDescriptor&);
std::optional<::WebGPU::RenderPassColorAttachment> deserialize(ObjectHeap&, const RenderPassColorAttachment&);
std::optional<::WebGPU::RenderPassDepthStencilAttachment> deserialize(ObjectHeap&, const RenderPassDepthStencilAttachment&);
std::optional<::WebGPU::RenderPassDescriptor> deserialize(ObjectHeap&, const RenderPassDescriptor&, Vector<std::optional<::WebGPU::RenderPassColorAttachment>>& colorAttachmentsStorage);
std::optional<::WebGPU::PassTimestampWrites> deserialize(ObjectHeap&, const RenderPassTimestampWrites&);
std::optional<::WebGPU::RenderPipelineDescriptor> deserialize(ObjectHeap&, const RenderPipelineDescriptor&, RenderPipelineDescriptorStorage&, bool allowMissingPipelineLayout = false);
std::optional<::WebGPU::ShaderModuleDescriptor> deserialize(ObjectHeap&, const ShaderModuleDescriptor&, Vector<::WebGPU::ShaderModuleCompilationHint>& hintsStorage);
std::optional<::WebGPU::TextureDescriptor> deserialize(ObjectHeap&, const TextureDescriptor&);
std::optional<::WebGPU::VertexBufferLayout> deserialize(ObjectHeap&, const VertexBufferLayout&, Vector<::WebGPU::VertexAttribute>& attributesStorage);
std::optional<::WebGPU::VertexState> deserialize(ObjectHeap&, const VertexState&, RenderPipelineDescriptorStorage&);

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
