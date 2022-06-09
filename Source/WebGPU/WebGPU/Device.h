/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#import "WebGPU.h"
#import <wtf/FastMalloc.h>
#import <wtf/Function.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>

namespace WebGPU {

class BindGroup;
class BindGroupLayout;
class Buffer;
class CommandEncoder;
class ComputePipeline;
class PipelineLayout;
class QuerySet;
class RenderBundleEncoder;
class RenderPipeline;
class Sampler;
class ShaderModule;
class Surface;
class SwapChain;
class Texture;
class Queue;

class Device : public RefCounted<Device> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<Device> create()
    {
        return adoptRef(*new Device());
    }

    ~Device();

    Ref<BindGroup> createBindGroup(const WGPUBindGroupDescriptor*);
    Ref<BindGroupLayout> createBindGroupLayout(const WGPUBindGroupLayoutDescriptor*);
    Ref<Buffer> createBuffer(const WGPUBufferDescriptor*);
    Ref<CommandEncoder> createCommandEncoder(const WGPUCommandEncoderDescriptor*);
    Ref<ComputePipeline> createComputePipeline(const WGPUComputePipelineDescriptor*);
    void createComputePipelineAsync(const WGPUComputePipelineDescriptor*, WTF::Function<void(WGPUCreatePipelineAsyncStatus, Ref<ComputePipeline>&&, const char* message)>&& callback);
    Ref<PipelineLayout> createPipelineLayout(const WGPUPipelineLayoutDescriptor*);
    Ref<QuerySet> createQuerySet(const WGPUQuerySetDescriptor*);
    Ref<RenderBundleEncoder> createRenderBundleEncoder(const WGPURenderBundleEncoderDescriptor*);
    Ref<RenderPipeline> createRenderPipeline(const WGPURenderPipelineDescriptor*);
    void createRenderPipelineAsync(const WGPURenderPipelineDescriptor*, WTF::Function<void(WGPUCreatePipelineAsyncStatus, Ref<RenderPipeline>&&, const char* message)>&& callback);
    Ref<Sampler> createSampler(const WGPUSamplerDescriptor*);
    Ref<ShaderModule> createShaderModule(const WGPUShaderModuleDescriptor*);
    Ref<SwapChain> createSwapChain(const Surface&, const WGPUSwapChainDescriptor*);
    Ref<Texture> createTexture(const WGPUTextureDescriptor*);
    void destroy();
    bool getLimits(WGPUSupportedLimits*);
    Ref<Queue> getQueue();
    bool popErrorScope(WTF::Function<void(WGPUErrorType, const char*)>&& callback);
    void pushErrorScope(WGPUErrorFilter);
    void setDeviceLostCallback(WTF::Function<void(WGPUDeviceLostReason, const char*)>&&);
    void setUncapturedErrorCallback(WTF::Function<void(WGPUErrorType, const char*)>&&);
    void setLabel(const char*);

private:
    Device();
};

} // namespace WebGPU

struct WGPUDeviceImpl {
    Ref<WebGPU::Device> device;
};
