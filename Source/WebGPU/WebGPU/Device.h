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
#import <functional>

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

class Device {
public:
    BindGroup createBindGroup(const WGPUBindGroupDescriptor*);
    BindGroupLayout createBindGroupLayout(const WGPUBindGroupLayoutDescriptor*);
    Buffer createBuffer(const WGPUBufferDescriptor*);
    CommandEncoder createCommandEncoder(const WGPUCommandEncoderDescriptor*);
    ComputePipeline createComputePipeline(const WGPUComputePipelineDescriptor*);
    void createComputePipelineAsync(const WGPUComputePipelineDescriptor*, std::function<void(WGPUCreatePipelineAsyncStatus, ComputePipeline&&, const char* message)>&& callback);
    PipelineLayout createPipelineLayout(const WGPUPipelineLayoutDescriptor*);
    QuerySet createQuerySet(const WGPUQuerySetDescriptor*);
    RenderBundleEncoder createRenderBundleEncoder(const WGPURenderBundleEncoderDescriptor*);
    RenderPipeline createRenderPipeline(const WGPURenderPipelineDescriptor*);
    void createRenderPipelineAsync(const WGPURenderPipelineDescriptor*, std::function<void(WGPUCreatePipelineAsyncStatus, RenderPipeline&&, const char* message)>&& callback);
    Sampler createSampler(const WGPUSamplerDescriptor*);
    ShaderModule createShaderModule(const WGPUShaderModuleDescriptor*);
    SwapChain createSwapChain(const Surface&, const WGPUSwapChainDescriptor*);
    Texture createTexture(const WGPUTextureDescriptor*);
    void destroy();
    bool getLimits(WGPUSupportedLimits*);
    Queue getQueue();
    bool popErrorScope(std::function<void(WGPUErrorType, const char*)>&& callback);
    void pushErrorScope(WGPUErrorFilter);
    void setDeviceLostCallback(std::function<void(WGPUDeviceLostReason, const char*)>&&);
    void setUncapturedErrorCallback(std::function<void(WGPUErrorType, const char*)>&&);
    void setLabel(const char*);
};

} // namespace WebGPU

struct WGPUDeviceImpl {
    WebGPU::Device device;
};
