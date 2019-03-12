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

#include "GPUQueue.h"
#include "GPUSwapChainDescriptor.h"
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

OBJC_PROTOCOL(MTLDevice);

namespace WebCore {

using PlatformDevice = MTLDevice;
using PlatformDeviceSmartPtr = RetainPtr<MTLDevice>;

class GPUBindGroupLayout;
class GPUBuffer;
class GPUCommandBuffer;
class GPUPipelineLayout;
class GPURenderPipeline;
class GPUSampler;
class GPUShaderModule;
class GPUSwapChain;
class GPUTexture;

struct GPUBindGroupLayoutDescriptor;
struct GPUBufferDescriptor;
struct GPUPipelineLayoutDescriptor;
struct GPURenderPipelineDescriptor;
struct GPURequestAdapterOptions;
struct GPUSamplerDescriptor;
struct GPUShaderModuleDescriptor;
struct GPUTextureDescriptor;

class GPUDevice : public RefCounted<GPUDevice>, public CanMakeWeakPtr<GPUDevice> {
public:
    static RefPtr<GPUDevice> create(Optional<GPURequestAdapterOptions>&&);

    RefPtr<GPUBuffer> tryCreateBuffer(GPUBufferDescriptor&&);
    RefPtr<GPUTexture> tryCreateTexture(GPUTextureDescriptor&&) const;
    RefPtr<GPUSampler> tryCreateSampler(const GPUSamplerDescriptor&) const;

    RefPtr<GPUBindGroupLayout> tryCreateBindGroupLayout(const GPUBindGroupLayoutDescriptor&) const;
    Ref<GPUPipelineLayout> createPipelineLayout(GPUPipelineLayoutDescriptor&&) const;

    RefPtr<GPUShaderModule> createShaderModule(GPUShaderModuleDescriptor&&) const;
    RefPtr<GPURenderPipeline> createRenderPipeline(GPURenderPipelineDescriptor&&) const;

    RefPtr<GPUCommandBuffer> createCommandBuffer();

    RefPtr<GPUSwapChain> tryCreateSwapChain(const GPUSwapChainDescriptor&, int width, int height) const;

    RefPtr<GPUQueue> getQueue() const;
    PlatformDevice* platformDevice() const { return m_platformDevice.get(); }
    GPUSwapChain* swapChain() const { return m_swapChain.get(); }

private:
    GPUDevice(PlatformDeviceSmartPtr&&);

    PlatformDeviceSmartPtr m_platformDevice;
    mutable RefPtr<GPUQueue> m_queue;
    mutable RefPtr<GPUSwapChain> m_swapChain;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
