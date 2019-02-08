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
#include <wtf/Optional.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

OBJC_PROTOCOL(MTLDevice);

namespace WebCore {

using PlatformDevice = MTLDevice;
using PlatformDeviceSmartPtr = RetainPtr<MTLDevice>;

class GPUBindGroupLayout;
class GPUBuffer;
class GPUCommandBuffer;
class GPUPipelineLayout;
class GPURenderPipeline;
class GPUShaderModule;
class GPUTexture;

struct GPUBindGroupLayoutDescriptor;
struct GPUBufferDescriptor;
struct GPUPipelineLayoutDescriptor;
struct GPURenderPipelineDescriptor;
struct GPURequestAdapterOptions;
struct GPUShaderModuleDescriptor;
struct GPUTextureDescriptor;

class GPUDevice : public RefCounted<GPUDevice> {
public:
    static RefPtr<GPUDevice> create(Optional<GPURequestAdapterOptions>&&);

    RefPtr<GPUBuffer> createBuffer(GPUBufferDescriptor&&) const;
    RefPtr<GPUTexture> tryCreateTexture(GPUTextureDescriptor&&) const;

    RefPtr<GPUBindGroupLayout> tryCreateBindGroupLayout(GPUBindGroupLayoutDescriptor&&) const;
    Ref<GPUPipelineLayout> createPipelineLayout(GPUPipelineLayoutDescriptor&&) const;

    RefPtr<GPUShaderModule> createShaderModule(GPUShaderModuleDescriptor&&) const;
    RefPtr<GPURenderPipeline> createRenderPipeline(GPURenderPipelineDescriptor&&) const;

    RefPtr<GPUCommandBuffer> createCommandBuffer();

    RefPtr<GPUQueue> getQueue();
    PlatformDevice* platformDevice() const { return m_platformDevice.get(); }

private:
    GPUDevice(PlatformDeviceSmartPtr&&);

    PlatformDeviceSmartPtr m_platformDevice;
    RefPtr<GPUQueue> m_queue;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
