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

#include "GPUDevice.h"
#include "GPUErrorScopes.h"
#include "JSDOMPromiseDeferred.h"
#include "WebGPUAdapter.h"
#include "WebGPUQueue.h"
#include "WebGPUSwapChainDescriptor.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace JSC {
class ArrayBuffer;
class JSValue;
}

namespace WebCore {

class GPUOutOfMemoryError;
class GPUValidationError;
class ScriptExecutionContext;
class WebGPUBindGroup;
class WebGPUBindGroupLayout;
class WebGPUBuffer;
class WebGPUCommandEncoder;
class WebGPUComputePipeline;
class WebGPUPipelineLayout;
class WebGPURenderPipeline;
class WebGPUSampler;
class WebGPUShaderModule;
class WebGPUSwapChain;
class WebGPUTexture;

struct GPUBindGroupLayoutDescriptor;
struct GPUBufferDescriptor;
struct GPUSamplerDescriptor;
struct GPUTextureDescriptor;
struct WebGPUBindGroupDescriptor;
struct WebGPUComputePipelineDescriptor;
struct WebGPUPipelineLayoutDescriptor;
struct WebGPURenderPipelineDescriptor;
struct WebGPUShaderModuleDescriptor;

enum class GPUErrorFilter;

using ErrorIDLUnion = IDLUnion<IDLInterface<GPUOutOfMemoryError>, IDLInterface<GPUValidationError>>;
using ErrorPromise = DOMPromiseDeferred<IDLNullable<ErrorIDLUnion>>;

class WebGPUDevice : public RefCounted<WebGPUDevice> {
public:
    static RefPtr<WebGPUDevice> tryCreate(Ref<const WebGPUAdapter>&&);

    const WebGPUAdapter& adapter() const { return m_adapter.get(); }
    GPUDevice& device() { return m_device.get(); }
    const GPUDevice& device() const { return m_device.get(); }

    Ref<WebGPUBuffer> createBuffer(const GPUBufferDescriptor&) const;
    Vector<JSC::JSValue> createBufferMapped(JSC::ExecState&, const GPUBufferDescriptor&) const;
    Ref<WebGPUTexture> createTexture(const GPUTextureDescriptor&) const;
    Ref<WebGPUSampler> createSampler(const GPUSamplerDescriptor&) const;

    Ref<WebGPUBindGroupLayout> createBindGroupLayout(const GPUBindGroupLayoutDescriptor&) const;
    Ref<WebGPUPipelineLayout> createPipelineLayout(const WebGPUPipelineLayoutDescriptor&) const;
    Ref<WebGPUBindGroup> createBindGroup(const WebGPUBindGroupDescriptor&) const;

    Ref<WebGPUShaderModule> createShaderModule(const WebGPUShaderModuleDescriptor&) const;
    Ref<WebGPURenderPipeline> createRenderPipeline(const WebGPURenderPipelineDescriptor&) const;
    Ref<WebGPUComputePipeline> createComputePipeline(const WebGPUComputePipelineDescriptor&) const;

    Ref<WebGPUCommandEncoder> createCommandEncoder() const;

    Ref<WebGPUQueue> getQueue() const;

    void pushErrorScope(GPUErrorFilter filter) { m_errorScopes->pushErrorScope(filter); }
    void popErrorScope(ErrorPromise&&);

private:
    WebGPUDevice(Ref<const WebGPUAdapter>&&, Ref<GPUDevice>&&);

    Ref<const WebGPUAdapter> m_adapter;
    Ref<GPUDevice> m_device;
    mutable RefPtr<WebGPUQueue> m_queue;

    Ref<GPUErrorScopes> m_errorScopes;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
