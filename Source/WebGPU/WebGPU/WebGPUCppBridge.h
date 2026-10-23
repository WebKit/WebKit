/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import <WebGPU/WebGPU.h>
#import <WebGPU/WebGPUCpp.h>

// The handles of the WebGPU C API are the objects of the WebGPU C++ API, so the conversions of a
// C API shim do not depend on the implementation that created the objects.

namespace WebGPU {

inline Adapter& fromAPI(WGPUAdapter handle)
{
    return *reinterpret_cast<Adapter*>(handle);
}

inline WGPUAdapter toAPI(Adapter& object)
{
    return reinterpret_cast<WGPUAdapter>(&object);
}

inline BindGroup& fromAPI(WGPUBindGroup handle)
{
    return *reinterpret_cast<BindGroup*>(handle);
}

inline WGPUBindGroup toAPI(BindGroup& object)
{
    return reinterpret_cast<WGPUBindGroup>(&object);
}

inline BindGroupLayout& fromAPI(WGPUBindGroupLayout handle)
{
    return *reinterpret_cast<BindGroupLayout*>(handle);
}

inline WGPUBindGroupLayout toAPI(BindGroupLayout& object)
{
    return reinterpret_cast<WGPUBindGroupLayout>(&object);
}

inline Buffer& fromAPI(WGPUBuffer handle)
{
    return *reinterpret_cast<Buffer*>(handle);
}

inline WGPUBuffer toAPI(Buffer& object)
{
    return reinterpret_cast<WGPUBuffer>(&object);
}

inline CommandBuffer& fromAPI(WGPUCommandBuffer handle)
{
    return *reinterpret_cast<CommandBuffer*>(handle);
}

inline WGPUCommandBuffer toAPI(CommandBuffer& object)
{
    return reinterpret_cast<WGPUCommandBuffer>(&object);
}

inline CommandEncoder& fromAPI(WGPUCommandEncoder handle)
{
    return *reinterpret_cast<CommandEncoder*>(handle);
}

inline WGPUCommandEncoder toAPI(CommandEncoder& object)
{
    return reinterpret_cast<WGPUCommandEncoder>(&object);
}

inline ComputePassEncoder& fromAPI(WGPUComputePassEncoder handle)
{
    return *reinterpret_cast<ComputePassEncoder*>(handle);
}

inline WGPUComputePassEncoder toAPI(ComputePassEncoder& object)
{
    return reinterpret_cast<WGPUComputePassEncoder>(&object);
}

inline ComputePipeline& fromAPI(WGPUComputePipeline handle)
{
    return *reinterpret_cast<ComputePipeline*>(handle);
}

inline WGPUComputePipeline toAPI(ComputePipeline& object)
{
    return reinterpret_cast<WGPUComputePipeline>(&object);
}

inline Device& fromAPI(WGPUDevice handle)
{
    return *reinterpret_cast<Device*>(handle);
}

inline WGPUDevice toAPI(Device& object)
{
    return reinterpret_cast<WGPUDevice>(&object);
}

inline ExternalTexture& fromAPI(WGPUExternalTexture handle)
{
    return *reinterpret_cast<ExternalTexture*>(handle);
}

inline WGPUExternalTexture toAPI(ExternalTexture& object)
{
    return reinterpret_cast<WGPUExternalTexture>(&object);
}

inline Instance& fromAPI(WGPUInstance handle)
{
    return *reinterpret_cast<Instance*>(handle);
}

inline WGPUInstance toAPI(Instance& object)
{
    return reinterpret_cast<WGPUInstance>(&object);
}

inline PipelineLayout& fromAPI(WGPUPipelineLayout handle)
{
    return *reinterpret_cast<PipelineLayout*>(handle);
}

inline WGPUPipelineLayout toAPI(PipelineLayout& object)
{
    return reinterpret_cast<WGPUPipelineLayout>(&object);
}

inline PresentationContext& fromAPI(WGPUSurface handle)
{
    return *reinterpret_cast<PresentationContext*>(handle);
}

inline WGPUSurface toAPI(PresentationContext& object)
{
    return reinterpret_cast<WGPUSurface>(&object);
}

inline QuerySet& fromAPI(WGPUQuerySet handle)
{
    return *reinterpret_cast<QuerySet*>(handle);
}

inline WGPUQuerySet toAPI(QuerySet& object)
{
    return reinterpret_cast<WGPUQuerySet>(&object);
}

inline Queue& fromAPI(WGPUQueue handle)
{
    return *reinterpret_cast<Queue*>(handle);
}

inline WGPUQueue toAPI(Queue& object)
{
    return reinterpret_cast<WGPUQueue>(&object);
}

inline RenderBundle& fromAPI(WGPURenderBundle handle)
{
    return *reinterpret_cast<RenderBundle*>(handle);
}

inline WGPURenderBundle toAPI(RenderBundle& object)
{
    return reinterpret_cast<WGPURenderBundle>(&object);
}

inline RenderBundleEncoder& fromAPI(WGPURenderBundleEncoder handle)
{
    return *reinterpret_cast<RenderBundleEncoder*>(handle);
}

inline WGPURenderBundleEncoder toAPI(RenderBundleEncoder& object)
{
    return reinterpret_cast<WGPURenderBundleEncoder>(&object);
}

inline RenderPassEncoder& fromAPI(WGPURenderPassEncoder handle)
{
    return *reinterpret_cast<RenderPassEncoder*>(handle);
}

inline WGPURenderPassEncoder toAPI(RenderPassEncoder& object)
{
    return reinterpret_cast<WGPURenderPassEncoder>(&object);
}

inline RenderPipeline& fromAPI(WGPURenderPipeline handle)
{
    return *reinterpret_cast<RenderPipeline*>(handle);
}

inline WGPURenderPipeline toAPI(RenderPipeline& object)
{
    return reinterpret_cast<WGPURenderPipeline>(&object);
}

inline Sampler& fromAPI(WGPUSampler handle)
{
    return *reinterpret_cast<Sampler*>(handle);
}

inline WGPUSampler toAPI(Sampler& object)
{
    return reinterpret_cast<WGPUSampler>(&object);
}

inline ShaderModule& fromAPI(WGPUShaderModule handle)
{
    return *reinterpret_cast<ShaderModule*>(handle);
}

inline WGPUShaderModule toAPI(ShaderModule& object)
{
    return reinterpret_cast<WGPUShaderModule>(&object);
}

inline Texture& fromAPI(WGPUTexture handle)
{
    return *reinterpret_cast<Texture*>(handle);
}

inline WGPUTexture toAPI(Texture& object)
{
    return reinterpret_cast<WGPUTexture>(&object);
}

inline TextureView& fromAPI(WGPUTextureView handle)
{
    return *reinterpret_cast<TextureView*>(handle);
}

inline WGPUTextureView toAPI(TextureView& object)
{
    return reinterpret_cast<WGPUTextureView>(&object);
}

} // namespace WebGPU
