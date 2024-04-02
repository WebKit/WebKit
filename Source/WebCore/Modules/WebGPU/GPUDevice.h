/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "EventTarget.h"
#include "GPUComputePipeline.h"
#include "GPUDeviceLostInfo.h"
#include "GPUError.h"
#include "GPUErrorFilter.h"
#include "GPURenderPipeline.h"
#include "GPUQueue.h"
#include "HTMLVideoElement.h"
#include "JSDOMPromiseDeferredForward.h"
#include "ScriptExecutionContext.h"
#include "WebGPUDevice.h"
#include <optional>
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakPtr.h>
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
class GPUPresentationContext;
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
class GPUTexture;
struct GPUTextureDescriptor;

class GPUDevice : public RefCounted<GPUDevice>, public ActiveDOMObject, public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(GPUDevice);
public:
    static Ref<GPUDevice> create(ScriptExecutionContext* scriptExecutionContext, Ref<WebGPU::Device>&& backing)
    {
        return adoptRef(*new GPUDevice(scriptExecutionContext, WTFMove(backing)));
    }

    virtual ~GPUDevice();

    String label() const;
    void setLabel(String&&);

    Ref<GPUSupportedFeatures> features() const;
    Ref<GPUSupportedLimits> limits() const;

    Ref<GPUQueue> queue() const;

    void destroy(ScriptExecutionContext&);

    ExceptionOr<Ref<GPUBuffer>> createBuffer(const GPUBufferDescriptor&);
    ExceptionOr<Ref<GPUTexture>> createTexture(const GPUTextureDescriptor&);
    bool isSupportedFormat(GPUTextureFormat) const;
    Ref<GPUSampler> createSampler(const std::optional<GPUSamplerDescriptor>&);
    Ref<GPUExternalTexture> importExternalTexture(const GPUExternalTextureDescriptor&);

    ExceptionOr<Ref<GPUBindGroupLayout>> createBindGroupLayout(const GPUBindGroupLayoutDescriptor&);
    Ref<GPUPipelineLayout> createPipelineLayout(const GPUPipelineLayoutDescriptor&);
    Ref<GPUBindGroup> createBindGroup(const GPUBindGroupDescriptor&);

    Ref<GPUShaderModule> createShaderModule(const GPUShaderModuleDescriptor&);
    Ref<GPUComputePipeline> createComputePipeline(const GPUComputePipelineDescriptor&);
    ExceptionOr<Ref<GPURenderPipeline>> createRenderPipeline(const GPURenderPipelineDescriptor&);
    using CreateComputePipelineAsyncPromise = DOMPromiseDeferred<IDLInterface<GPUComputePipeline>>;
    void createComputePipelineAsync(const GPUComputePipelineDescriptor&, CreateComputePipelineAsyncPromise&&);
    using CreateRenderPipelineAsyncPromise = DOMPromiseDeferred<IDLInterface<GPURenderPipeline>>;
    ExceptionOr<void> createRenderPipelineAsync(const GPURenderPipelineDescriptor&, CreateRenderPipelineAsyncPromise&&);

    Ref<GPUCommandEncoder> createCommandEncoder(const std::optional<GPUCommandEncoderDescriptor>&);
    ExceptionOr<Ref<GPURenderBundleEncoder>> createRenderBundleEncoder(const GPURenderBundleEncoderDescriptor&);

    ExceptionOr<Ref<GPUQuerySet>> createQuerySet(const GPUQuerySetDescriptor&);

    void pushErrorScope(GPUErrorFilter);
    using ErrorScopePromise = DOMPromiseDeferred<IDLNullable<IDLUnion<IDLInterface<GPUOutOfMemoryError>, IDLInterface<GPUValidationError>, IDLInterface<GPUInternalError>>>>;
    void popErrorScope(ErrorScopePromise&&);

    bool addEventListener(const AtomString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&) override;

    using LostPromise = DOMPromiseProxy<IDLInterface<GPUDeviceLostInfo>>;
    LostPromise& lost();

    WebGPU::Device& backing() { return m_backing; }
    const WebGPU::Device& backing() const { return m_backing; }
    void removeBufferToUnmap(GPUBuffer&);
    void addBufferToUnmap(GPUBuffer&);

    using RefCounted::ref;
    using RefCounted::deref;

private:
    GPUDevice(ScriptExecutionContext*, Ref<WebGPU::Device>&&);

    // ActiveDOMObject.
    // FIXME: We probably need to override more methods to make this work properly.
    const char* activeDOMObjectName() const final { return "GPUDevice"; }
    Ref<GPUPipelineLayout> createAutoPipelineLayout();

    // EventTarget.
    EventTargetInterface eventTargetInterface() const final { return GPUDeviceEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    UniqueRef<LostPromise> m_lostPromise;
    Ref<WebGPU::Device> m_backing;
    Ref<GPUQueue> m_queue;
    Ref<GPUPipelineLayout> m_autoPipelineLayout;
    HashSet<GPUBuffer*> m_buffersToUnmap;

#if ENABLE(VIDEO)
    GPUExternalTexture* externalTextureForDescriptor(const GPUExternalTextureDescriptor&);
#endif

    WeakHashMap<HTMLVideoElement, WeakPtr<GPUExternalTexture>, WeakPtrImplWithEventTargetData> m_videoElementToExternalTextureMap;
    bool m_waitingForDeviceLostPromise { false };
};

}
