/*
 * Copyright (c) 2022-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import "Adapter.h"
#import "BindGroup.h"
#import "BindGroupLayout.h"
#import "Buffer.h"
#import "CommandBuffer.h"
#import "CommandEncoder.h"
#import "ComputePassEncoder.h"
#import "ComputePipeline.h"
#import "Device.h"
#import "ExternalTexture.h"
#import "Instance.h"
#import "PipelineLayout.h"
#import "PresentationContext.h"
#import "QuerySet.h"
#import "Queue.h"
#import "RenderBundle.h"
#import "RenderBundleEncoder.h"
#import "RenderPassEncoder.h"
#import "RenderPipeline.h"
#import "Sampler.h"
#import "ShaderModule.h"
#import "Texture.h"
#import "TextureView.h"
#import "XRBinding.h"
#import "XRProjectionLayer.h"
#import "XRSubImage.h"
#import "XRView.h"
#import <wtf/BlockPtr.h>
#import <wtf/SwiftBridging.h>
#import <wtf/text/WTFString.h>

namespace WebGPU {

// FIXME: It would be cool if we didn't have to list all these overloads, but instead could do something like bridge_cast() in WTF.

inline Adapter& fromAPI(WGPUAdapter adapter)
{
    return static_cast<Adapter&>(*adapter);
}

inline BindGroup& fromAPI(WGPUBindGroup bindGroup)
{
    return static_cast<BindGroup&>(*bindGroup);
}

inline BindGroupLayout& fromAPI(WGPUBindGroupLayout bindGroupLayout)
{
    return static_cast<BindGroupLayout&>(*bindGroupLayout);
}

inline Buffer& fromAPI(WGPUBuffer buffer)
{
    return static_cast<Buffer&>(*buffer);
}

inline CommandBuffer& fromAPI(WGPUCommandBuffer commandBuffer)
{
    return static_cast<CommandBuffer&>(*commandBuffer);
}

inline CommandEncoder& fromAPI(WGPUCommandEncoder commandEncoder)
{
    return static_cast<CommandEncoder&>(*commandEncoder);
}

inline ComputePassEncoder& fromAPI(WGPUComputePassEncoder computePassEncoder)
{
    return static_cast<ComputePassEncoder&>(*computePassEncoder);
}

inline ComputePipeline& fromAPI(WGPUComputePipeline computePipeline)
{
    return static_cast<ComputePipeline&>(*computePipeline);
}

inline Device& fromAPI(WGPUDevice device)
{
    return static_cast<Device&>(*device);
}

inline ExternalTexture& fromAPI(WGPUExternalTexture texture)
{
    return static_cast<ExternalTexture&>(*texture);
}

inline Instance& fromAPI(WGPUInstance instance)
{
    return static_cast<Instance&>(*instance);
}

inline PipelineLayout& fromAPI(WGPUPipelineLayout pipelineLayout)
{
    return static_cast<PipelineLayout&>(*pipelineLayout);
}

inline QuerySet& fromAPI(WGPUQuerySet querySet)
{
    return static_cast<QuerySet&>(*querySet);
}

inline Queue& fromAPI(WGPUQueue queue)
{
    return static_cast<Queue&>(*queue);
}

inline RenderBundle& fromAPI(WGPURenderBundle renderBundle)
{
    return static_cast<RenderBundle&>(*renderBundle);
}

inline RenderBundleEncoder& fromAPI(WGPURenderBundleEncoder renderBundleEncoder)
{
    return static_cast<RenderBundleEncoder&>(*renderBundleEncoder);
}

inline RenderPassEncoder& fromAPI(WGPURenderPassEncoder renderPassEncoder)
{
    return static_cast<RenderPassEncoder&>(*renderPassEncoder);
}

inline RenderPipeline& fromAPI(WGPURenderPipeline renderPipeline)
{
    return static_cast<RenderPipeline&>(*renderPipeline);
}

inline Sampler& fromAPI(WGPUSampler sampler)
{
    return static_cast<Sampler&>(*sampler);
}

inline ShaderModule& fromAPI(WGPUShaderModule shaderModule)
{
    return static_cast<ShaderModule&>(*shaderModule);
}

inline PresentationContext& fromAPI(WGPUSurface surface)
{
    return static_cast<PresentationContext&>(*surface);
}

inline PresentationContext& fromAPI(WGPUSwapChain swapChain)
{
    return static_cast<PresentationContext&>(*swapChain);
}

inline Texture& fromAPI(WGPUTexture texture)
{
    return static_cast<Texture&>(*texture);
}

inline TextureView& fromAPI(WGPUTextureView textureView)
{
    return static_cast<TextureView&>(*textureView);
}

inline XRBinding& fromAPI(WGPUXRBinding binding)
{
    return static_cast<XRBinding&>(*binding);
}

inline XRSubImage& fromAPI(WGPUXRSubImage subImage)
{
    return static_cast<XRSubImage&>(*subImage);
}

inline XRProjectionLayer& fromAPI(WGPUXRProjectionLayer layer)
{
    return static_cast<XRProjectionLayer&>(*layer);
}

inline XRView& fromAPI(WGPUXRView view)
{
    return static_cast<XRView&>(*view);
}

// Associates a chainable extension struct with its sType tag. Specialize for each struct
// that findChainedStruct() is used with.
template<typename T> struct ChainedStructSType;

template<> struct ChainedStructSType<WGPUShaderSourceWGSL> {
    static constexpr WGPUSType value = WGPUSType_ShaderSourceWGSL;
};

template<> struct ChainedStructSType<WGPUInstanceCocoaDescriptor> {
    static constexpr WGPUSType value = static_cast<WGPUSType>(WGPUSTypeExtended_InstanceCocoaDescriptor);
};

template<> struct ChainedStructSType<WGPUSurfaceDescriptorCocoaCustomSurface> {
    static constexpr WGPUSType value = static_cast<WGPUSType>(WGPUSTypeExtended_SurfaceDescriptorCocoaSurfaceBacking);
};

// Walks a descriptor's nextInChain looking for one particular extension struct. Every
// chainable struct starts with its WGPUChainedStruct, so the match can be cast to it.
template<typename T>
inline const T* findChainedStruct(const WGPUChainedStruct* chain)
{
    static_assert(std::is_same_v<decltype(T::chain), WGPUChainedStruct>);
    for (; chain; chain = chain->next) {
        if (chain->sType == ChainedStructSType<T>::value)
            return reinterpret_cast<const T*>(chain);
    }
    return nullptr;
}

inline String fromAPI(const char* string)
{
    return String::fromUTF8(string);
}

inline String fromAPI(WGPUStringView string)
{
    if (!string.data)
        return { };
    if (string.length == WGPU_STRLEN)
        return String::fromUTF8(string.data);
    return String::fromUTF8(unsafeMakeSpan(string.data, string.length));
}

// Literals have static storage, so the view can borrow them freely.
inline WGPUStringView toAPI(ASCIILiteral literal)
{
    return { literal.characters(), literal.length() };
}

// Borrows already-encoded UTF-8 bytes, so the result only lives as long as the argument.
inline WGPUStringView toAPI(const UTF8CString& string LIFETIME_BOUND)
{
    auto bytes = byteCast<char>(string.span());
    return { bytes.data(), bytes.size() };
}

inline std::span<const WGPUBindGroupLayout> bindGroupLayoutsSpan(const WGPUPipelineLayoutDescriptor& descriptor)
{
    return unsafeMakeSpan(descriptor.bindGroupLayouts, descriptor.bindGroupLayoutCount);
}

inline std::span<const WGPUTextureFormat> colorFormatsSpan(const WGPURenderBundleEncoderDescriptor& descriptor)
{
    return unsafeMakeSpan(descriptor.colorFormats, descriptor.colorFormatCount);
}

inline std::span<const WGPUBindGroupEntry> entriesSpan(const WGPUBindGroupDescriptor& descriptor)
{
    return unsafeMakeSpan(descriptor.entries, descriptor.entryCount);
}

inline std::span<const WGPUBindGroupLayoutEntry> entriesSpan(const WGPUBindGroupLayoutDescriptor& descriptor)
{
    return unsafeMakeSpan(descriptor.entries, descriptor.entryCount);
}

inline std::span<const WGPUConstantEntry> constantsSpan(const WGPUComputeState& state)
{
    return unsafeMakeSpan(state.constants, state.constantCount);
}

inline std::span<const WGPUConstantEntry> constantsSpan(const WGPUVertexState& state)
{
    return unsafeMakeSpan(state.constants, state.constantCount);
}

inline std::span<const WGPUConstantEntry> constantsSpan(const WGPUFragmentState& state)
{
    return unsafeMakeSpan(state.constants, state.constantCount);
}

inline std::span<const WGPUShaderModuleCompilationHint> hintsSpan(const WGPUShaderModuleDescriptor& descriptor)
{
    return unsafeMakeSpan(descriptor.hints, descriptor.hintCount);
}

inline std::span<const WGPUTextureFormat> viewFormatsSpan(const WGPUTextureDescriptor& descriptor)
{
    return unsafeMakeSpan(descriptor.viewFormats, descriptor.viewFormatCount);
}

inline std::span<const WGPUVertexAttribute> attributesSpan(const WGPUVertexBufferLayout& layout)
{
    return unsafeMakeSpan(layout.attributes, layout.attributeCount);
}

inline std::span<const WGPUFeatureName> requiredFeaturesSpan(const WGPUDeviceDescriptor& descriptor)
{
    return unsafeMakeSpan(descriptor.requiredFeatures, descriptor.requiredFeatureCount);
}

inline std::span<const WGPURenderPassColorAttachment> colorAttachmentsSpan(const WGPURenderPassDescriptor& descriptor)
{
    return unsafeMakeSpan(descriptor.colorAttachments, descriptor.colorAttachmentCount);
}

inline std::span<const WGPUVertexBufferLayout> buffersSpan(const WGPUVertexState& state)
{
    return unsafeMakeSpan(state.buffers, state.bufferCount);
}

inline std::span<const WGPUColorTargetState> targetsSpan(const WGPUFragmentState& state)
{
    return unsafeMakeSpan(state.targets, state.targetCount);
}

template<typename R, typename... Args>
inline BlockPtr<R (Args...)> fromAPI(R (^ __strong &&block)(Args...))
{
    return makeBlockPtr(WTF::move(block));
}

template <typename T>
inline T* releaseToAPI(Ref<T>&& pointer)
{
    return &pointer.leakRef();
}

template <typename T>
inline T* releaseToAPI(RefPtr<T>&& pointer)
{
    // FIXME: We shouldn't need this, because invalid objects should be created instead of returning nullptr.
    if (pointer)
        return pointer.leakRef();
    return nullptr;
}

} // namespace WebGPU
