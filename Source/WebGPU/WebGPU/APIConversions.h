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

inline SWIFT_RETURNS_UNRETAINED Adapter& fromAPI(WGPUAdapter adapter)
{
    return static_cast<Adapter&>(*adapter);
}

inline SWIFT_RETURNS_UNRETAINED BindGroup& fromAPI(WGPUBindGroup bindGroup)
{
    return static_cast<BindGroup&>(*bindGroup);
}

inline SWIFT_RETURNS_UNRETAINED BindGroupLayout& fromAPI(WGPUBindGroupLayout bindGroupLayout)
{
    return static_cast<BindGroupLayout&>(*bindGroupLayout);
}

inline SWIFT_RETURNS_UNRETAINED Buffer& fromAPI(WGPUBuffer buffer)
{
    return static_cast<Buffer&>(*buffer);
}

inline SWIFT_RETURNS_UNRETAINED CommandBuffer& fromAPI(WGPUCommandBuffer commandBuffer)
{
    return static_cast<CommandBuffer&>(*commandBuffer);
}

inline SWIFT_RETURNS_UNRETAINED CommandEncoder& fromAPI(WGPUCommandEncoder commandEncoder)
{
    return static_cast<CommandEncoder&>(*commandEncoder);
}

inline SWIFT_RETURNS_UNRETAINED ComputePassEncoder& fromAPI(WGPUComputePassEncoder computePassEncoder)
{
    return static_cast<ComputePassEncoder&>(*computePassEncoder);
}

inline SWIFT_RETURNS_UNRETAINED ComputePipeline& fromAPI(WGPUComputePipeline computePipeline)
{
    return static_cast<ComputePipeline&>(*computePipeline);
}

inline SWIFT_RETURNS_UNRETAINED Device& fromAPI(WGPUDevice device)
{
    return static_cast<Device&>(*device);
}

inline SWIFT_RETURNS_UNRETAINED ExternalTexture& fromAPI(WGPUExternalTexture texture)
{
    return static_cast<ExternalTexture&>(*texture);
}

inline SWIFT_RETURNS_UNRETAINED Instance& fromAPI(WGPUInstance instance)
{
    return static_cast<Instance&>(*instance);
}

inline SWIFT_RETURNS_UNRETAINED PipelineLayout& fromAPI(WGPUPipelineLayout pipelineLayout)
{
    return static_cast<PipelineLayout&>(*pipelineLayout);
}

inline SWIFT_RETURNS_UNRETAINED QuerySet& fromAPI(WGPUQuerySet querySet)
{
    return static_cast<QuerySet&>(*querySet);
}

inline SWIFT_RETURNS_UNRETAINED Queue& fromAPI(WGPUQueue queue)
{
    return static_cast<Queue&>(*queue);
}

inline SWIFT_RETURNS_UNRETAINED RenderBundle& fromAPI(WGPURenderBundle renderBundle)
{
    return static_cast<RenderBundle&>(*renderBundle);
}

inline SWIFT_RETURNS_UNRETAINED RenderBundleEncoder& fromAPI(WGPURenderBundleEncoder renderBundleEncoder)
{
    return static_cast<RenderBundleEncoder&>(*renderBundleEncoder);
}

inline SWIFT_RETURNS_UNRETAINED RenderPassEncoder& fromAPI(WGPURenderPassEncoder renderPassEncoder)
{
    return static_cast<RenderPassEncoder&>(*renderPassEncoder);
}

inline SWIFT_RETURNS_UNRETAINED RenderPipeline& fromAPI(WGPURenderPipeline renderPipeline)
{
    return static_cast<RenderPipeline&>(*renderPipeline);
}

inline SWIFT_RETURNS_UNRETAINED Sampler& fromAPI(WGPUSampler sampler)
{
    return static_cast<Sampler&>(*sampler);
}

inline SWIFT_RETURNS_UNRETAINED ShaderModule& fromAPI(WGPUShaderModule shaderModule)
{
    return static_cast<ShaderModule&>(*shaderModule);
}

inline SWIFT_RETURNS_UNRETAINED PresentationContext& fromAPI(WGPUSurface surface)
{
    return static_cast<PresentationContext&>(*surface);
}

inline SWIFT_RETURNS_UNRETAINED PresentationContext& fromAPI(WGPUSwapChain swapChain)
{
    return static_cast<PresentationContext&>(*swapChain);
}

inline SWIFT_RETURNS_UNRETAINED Texture& fromAPI(WGPUTexture texture)
{
    return static_cast<Texture&>(*texture);
}

inline SWIFT_RETURNS_UNRETAINED TextureView& fromAPI(WGPUTextureView textureView)
{
    return static_cast<TextureView&>(*textureView);
}

inline SWIFT_RETURNS_UNRETAINED XRBinding& fromAPI(WGPUXRBinding binding)
{
    return static_cast<XRBinding&>(*binding);
}

inline SWIFT_RETURNS_UNRETAINED XRSubImage& fromAPI(WGPUXRSubImage subImage)
{
    return static_cast<XRSubImage&>(*subImage);
}

inline SWIFT_RETURNS_UNRETAINED XRProjectionLayer& fromAPI(WGPUXRProjectionLayer layer)
{
    return static_cast<XRProjectionLayer&>(*layer);
}

inline SWIFT_RETURNS_UNRETAINED XRView& fromAPI(WGPUXRView view)
{
    return static_cast<XRView&>(*view);
}

inline SWIFT_RETURNS_UNRETAINED String fromAPI(const char* string)
{
    return String::fromUTF8(string);
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
