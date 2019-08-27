/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGPU)

#include "GPUBindGroupAllocator.h"
#include "GPUQueue.h"
#include "GPUSwapChain.h"
#include <wtf/Function.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

OBJC_PROTOCOL(MTLDevice);

namespace WebCore {

class GPUBindGroup;
class GPUBindGroupLayout;
class GPUBuffer;
class GPUCommandBuffer;
class GPUComputePipeline;
class GPUErrorScopes;
class GPUPipelineLayout;
class GPURenderPipeline;
class GPUSampler;
class GPUShaderModule;
class GPUTexture;

struct GPUBindGroupDescriptor;
struct GPUBindGroupLayoutDescriptor;
struct GPUBufferDescriptor;
struct GPUComputePipelineDescriptor;
struct GPUPipelineLayoutDescriptor;
struct GPURenderPipelineDescriptor;
struct GPURequestAdapterOptions;
struct GPUSamplerDescriptor;
struct GPUShaderModuleDescriptor;
struct GPUSwapChainDescriptor;
struct GPUTextureDescriptor;

enum class GPUBufferMappedOption;

using PlatformDevice = MTLDevice;
using PlatformDeviceSmartPtr = RetainPtr<MTLDevice>;

class GPUDevice : public RefCounted<GPUDevice>, public CanMakeWeakPtr<GPUDevice> {
public:
    static RefPtr<GPUDevice> tryCreate(const Optional<GPURequestAdapterOptions>&);

    RefPtr<GPUBuffer> tryCreateBuffer(const GPUBufferDescriptor&, GPUBufferMappedOption, GPUErrorScopes&);
    RefPtr<GPUTexture> tryCreateTexture(const GPUTextureDescriptor&) const;
    RefPtr<GPUSampler> tryCreateSampler(const GPUSamplerDescriptor&) const;

    RefPtr<GPUBindGroupLayout> tryCreateBindGroupLayout(const GPUBindGroupLayoutDescriptor&) const;
    Ref<GPUPipelineLayout> createPipelineLayout(GPUPipelineLayoutDescriptor&&) const;
    RefPtr<GPUBindGroup> tryCreateBindGroup(const GPUBindGroupDescriptor&, GPUErrorScopes&) const;

    RefPtr<GPUShaderModule> tryCreateShaderModule(const GPUShaderModuleDescriptor&) const;
    RefPtr<GPURenderPipeline> tryCreateRenderPipeline(const GPURenderPipelineDescriptor&, GPUErrorScopes&) const;
    RefPtr<GPUComputePipeline> tryCreateComputePipeline(const GPUComputePipelineDescriptor&, GPUErrorScopes&) const;

    RefPtr<GPUCommandBuffer> tryCreateCommandBuffer() const;

    RefPtr<GPUQueue> tryGetQueue() const;
    
    PlatformDevice* platformDevice() const { return m_platformDevice.get(); }
    GPUSwapChain* swapChain() const { return m_swapChain.get(); }
    void setSwapChain(RefPtr<GPUSwapChain>&&);

    static constexpr bool useWHLSL = true;

private:
    explicit GPUDevice(PlatformDeviceSmartPtr&&);

    PlatformDeviceSmartPtr m_platformDevice;
    mutable RefPtr<GPUQueue> m_queue;
    RefPtr<GPUSwapChain> m_swapChain;
    mutable RefPtr<GPUBindGroupAllocator> m_bindGroupAllocator;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
