/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "DOMPromiseProxy.h"
#include "EventTarget.h"
#include "GPUComputePipeline.h"
#include "GPUDeviceLostInfo.h"
#include "GPUError.h"
#include "GPUErrorFilter.h"
#include "GPURenderPipeline.h"
#include "GPUQueue.h"
#include "JSDOMPromiseDeferred.h"
#include "ScriptExecutionContext.h"
#include <optional>
#include <pal/graphics/WebGPU/WebGPUDevice.h>
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPUBindGroup;
struct GPUBindGroupDescriptor;
class GPUBindGroupLayout;
struct GPUBindGroupLayoutDescriptor;
class GPUBuffer;
struct GPUBufferDescriptor;
class GPUCommandEncoder;
struct GPUCommandEncoderDescriptor;
class GPUComputePipeline;
struct GPUComputePipelineDescriptor;
class GPUExternalTexture;
struct GPUExternalTextureDescriptor;
class GPURenderPipeline;
struct GPURenderPipelineDescriptor;
class GPUPipelineLayout;
struct GPUPipelineLayoutDescriptor;
class GPUQuerySet;
struct GPUQuerySetDescriptor;
class GPURenderBundleEncoder;
struct GPURenderBundleEncoderDescriptor;
class GPURenderPipeline;
struct GPURenderPipelineDescriptor;
class GPUSampler;
struct GPUSamplerDescriptor;
class GPUShaderModule;
struct GPUShaderModuleDescriptor;
class GPUSupportedFeatures;
class GPUSupportedLimits;
class GPUSurface;
struct GPUSurfaceDescriptor;
class GPUSwapChain;
struct GPUSwapChainDescriptor;
class GPUTexture;
struct GPUTextureDescriptor;

class GPUDevice : public RefCounted<GPUDevice>, public ActiveDOMObject, public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(GPUDevice);
public:
    static Ref<GPUDevice> create(ScriptExecutionContext* scriptExecutionContext, Ref<PAL::WebGPU::Device>&& backing)
    {
        return adoptRef(*new GPUDevice(scriptExecutionContext, WTFMove(backing)));
    }

    virtual ~GPUDevice();

    String label() const;
    void setLabel(String&&);

    Ref<GPUSupportedFeatures> features() const;
    Ref<GPUSupportedLimits> limits() const;

    GPUQueue& queue() const;

    void destroy();

    Ref<GPUBuffer> createBuffer(const GPUBufferDescriptor&);
    Ref<GPUTexture> createTexture(const GPUTextureDescriptor&);
    Ref<GPUTexture> createSurfaceTexture(const GPUTextureDescriptor&, const GPUSurface&);
    Ref<GPUSurface> createSurface(const GPUSurfaceDescriptor&);
    Ref<GPUSwapChain> createSwapChain(const GPUSurface&, const GPUSwapChainDescriptor&);
    Ref<GPUSampler> createSampler(const std::optional<GPUSamplerDescriptor>&);
    Ref<GPUExternalTexture> importExternalTexture(const GPUExternalTextureDescriptor&);

    Ref<GPUBindGroupLayout> createBindGroupLayout(const GPUBindGroupLayoutDescriptor&);
    Ref<GPUPipelineLayout> createPipelineLayout(const GPUPipelineLayoutDescriptor&);
    Ref<GPUBindGroup> createBindGroup(const GPUBindGroupDescriptor&);

    Ref<GPUShaderModule> createShaderModule(const GPUShaderModuleDescriptor&);
    Ref<GPUComputePipeline> createComputePipeline(const GPUComputePipelineDescriptor&);
    Ref<GPURenderPipeline> createRenderPipeline(const GPURenderPipelineDescriptor&);
    using CreateComputePipelineAsyncPromise = DOMPromiseDeferred<IDLInterface<GPUComputePipeline>>;
    void createComputePipelineAsync(const GPUComputePipelineDescriptor&, CreateComputePipelineAsyncPromise&&);
    using CreateRenderPipelineAsyncPromise = DOMPromiseDeferred<IDLInterface<GPURenderPipeline>>;
    void createRenderPipelineAsync(const GPURenderPipelineDescriptor&, CreateRenderPipelineAsyncPromise&&);

    Ref<GPUCommandEncoder> createCommandEncoder(const std::optional<GPUCommandEncoderDescriptor>&);
    Ref<GPURenderBundleEncoder> createRenderBundleEncoder(const GPURenderBundleEncoderDescriptor&);

    Ref<GPUQuerySet> createQuerySet(const GPUQuerySetDescriptor&);

    void pushErrorScope(GPUErrorFilter);
    using ErrorScopePromise = DOMPromiseDeferred<IDLNullable<IDLUnion<IDLInterface<GPUOutOfMemoryError>, IDLInterface<GPUValidationError>>>>;
    void popErrorScope(ErrorScopePromise&&);

    using LostPromise = DOMPromiseProxy<IDLInterface<GPUDeviceLostInfo>>;
    LostPromise& lost() { return m_lostPromise; }

    PAL::WebGPU::Device& backing() { return m_backing; }
    const PAL::WebGPU::Device& backing() const { return m_backing; }

    using RefCounted::ref;
    using RefCounted::deref;

private:
    GPUDevice(ScriptExecutionContext*, Ref<PAL::WebGPU::Device>&&);

    // ActiveDOMObject.
    // FIXME: We probably need to override more methods to make this work properly.
    const char* activeDOMObjectName() const final { return "GPUDevice"; }

    // EventTarget.
    EventTargetInterface eventTargetInterface() const final { return GPUDeviceEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    LostPromise m_lostPromise;
    Ref<PAL::WebGPU::Device> m_backing;
    Ref<GPUQueue> m_queue;
    Ref<GPUPipelineLayout> m_autoPipelineLayout;
};

}
