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

inline String fromAPI(const char* string)
{
    return String::fromUTF8(string);
}

#define WGPU_PROTECTED_API_UNAVAILABLE NS_SWIFT_UNAVAILABLE("use fromAPI, reference couting is implicit")

inline Ref<Adapter> protectedFromAPI(WGPUAdapter adapter) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<Adapter&>(*adapter);
}

inline Ref<BindGroup> protectedFromAPI(WGPUBindGroup bindGroup) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<BindGroup&>(*bindGroup);
}

inline Ref<BindGroupLayout> protectedFromAPI(WGPUBindGroupLayout bindGroupLayout) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<BindGroupLayout&>(*bindGroupLayout);
}

inline Ref<Buffer> protectedFromAPI(WGPUBuffer buffer) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<Buffer&>(*buffer);
}

inline Ref<CommandBuffer> protectedFromAPI(WGPUCommandBuffer commandBuffer) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<CommandBuffer&>(*commandBuffer);
}

inline Ref<CommandEncoder> protectedFromAPI(WGPUCommandEncoder commandEncoder) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<CommandEncoder&>(*commandEncoder);
}

inline Ref<ComputePassEncoder> protectedFromAPI(WGPUComputePassEncoder computePassEncoder) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<ComputePassEncoder&>(*computePassEncoder);
}

inline Ref<ComputePipeline> protectedFromAPI(WGPUComputePipeline computePipeline) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<ComputePipeline&>(*computePipeline);
}

inline Ref<Device> protectedFromAPI(WGPUDevice device) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<Device&>(*device);
}

inline Ref<ExternalTexture> protectedFromAPI(WGPUExternalTexture texture) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<ExternalTexture&>(*texture);
}

inline Ref<Instance> protectedFromAPI(WGPUInstance instance) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<Instance&>(*instance);
}

inline Ref<PipelineLayout> protectedFromAPI(WGPUPipelineLayout pipelineLayout) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<PipelineLayout&>(*pipelineLayout);
}

inline Ref<QuerySet> protectedFromAPI(WGPUQuerySet querySet) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<QuerySet&>(*querySet);
}

inline Ref<Queue> protectedFromAPI(WGPUQueue queue) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<Queue&>(*queue);
}

inline Ref<RenderBundle> protectedFromAPI(WGPURenderBundle renderBundle) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<RenderBundle&>(*renderBundle);
}

inline Ref<RenderBundleEncoder> protectedFromAPI(WGPURenderBundleEncoder renderBundleEncoder) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<RenderBundleEncoder&>(*renderBundleEncoder);
}

inline Ref<RenderPassEncoder> protectedFromAPI(WGPURenderPassEncoder renderPassEncoder) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<RenderPassEncoder&>(*renderPassEncoder);
}

inline Ref<RenderPipeline> protectedFromAPI(WGPURenderPipeline renderPipeline) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<RenderPipeline&>(*renderPipeline);
}

inline Ref<Sampler> protectedFromAPI(WGPUSampler sampler) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<Sampler&>(*sampler);
}

inline Ref<ShaderModule> protectedFromAPI(WGPUShaderModule shaderModule) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<ShaderModule&>(*shaderModule);
}

inline Ref<PresentationContext> protectedFromAPI(WGPUSurface surface) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<PresentationContext&>(*surface);
}

inline Ref<PresentationContext> protectedFromAPI(WGPUSwapChain swapChain) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<PresentationContext&>(*swapChain);
}

inline Ref<Texture> protectedFromAPI(WGPUTexture texture) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<Texture&>(*texture);
}

inline Ref<TextureView> protectedFromAPI(WGPUTextureView textureView) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<TextureView&>(*textureView);
}

inline Ref<XRBinding> protectedFromAPI(WGPUXRBinding binding) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<XRBinding&>(*binding);
}

inline Ref<XRSubImage> protectedFromAPI(WGPUXRSubImage subImage) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<XRSubImage&>(*subImage);
}

inline Ref<XRProjectionLayer> protectedFromAPI(WGPUXRProjectionLayer layer) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<XRProjectionLayer&>(*layer);
}

inline Ref<XRView> protectedFromAPI(WGPUXRView view) WGPU_PROTECTED_API_UNAVAILABLE
{
    return static_cast<XRView&>(*view);
}

template<typename R, typename... Args>
inline BlockPtr<R (Args...)> fromAPI(R (^ __strong &&block)(Args...))
{
    return makeBlockPtr(WTFMove(block));
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

#undef WGPU_PROTECTED_API_UNAVAILABLE

} // namespace WebGPU
