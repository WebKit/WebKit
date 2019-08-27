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

#include "config.h"
#include "WebGPUDevice.h"

#if ENABLE(WEBGPU)

#include "Exception.h"
#include "GPUBindGroup.h"
#include "GPUBindGroupBinding.h"
#include "GPUBindGroupDescriptor.h"
#include "GPUBindGroupLayoutDescriptor.h"
#include "GPUBufferBinding.h"
#include "GPUBufferDescriptor.h"
#include "GPUCommandBuffer.h"
#include "GPUComputePipelineDescriptor.h"
#include "GPUPipelineStageDescriptor.h"
#include "GPURenderPipelineDescriptor.h"
#include "GPUSampler.h"
#include "GPUSamplerDescriptor.h"
#include "GPUShaderModuleDescriptor.h"
#include "GPUTextureDescriptor.h"
#include "JSDOMConvertBufferSource.h"
#include "JSGPUOutOfMemoryError.h"
#include "JSGPUValidationError.h"
#include "JSWebGPUBuffer.h"
#include "Logging.h"
#include "WebGPUBindGroup.h"
#include "WebGPUBindGroupBinding.h"
#include "WebGPUBindGroupDescriptor.h"
#include "WebGPUBindGroupLayout.h"
#include "WebGPUBufferBinding.h"
#include "WebGPUCommandEncoder.h"
#include "WebGPUComputePipeline.h"
#include "WebGPUComputePipelineDescriptor.h"
#include "WebGPUPipelineLayout.h"
#include "WebGPUPipelineLayoutDescriptor.h"
#include "WebGPUPipelineStageDescriptor.h"
#include "WebGPUQueue.h"
#include "WebGPURenderPipeline.h"
#include "WebGPURenderPipelineDescriptor.h"
#include "WebGPUSampler.h"
#include "WebGPUShaderModule.h"
#include "WebGPUShaderModuleDescriptor.h"
#include "WebGPUSwapChain.h"
#include "WebGPUTexture.h"
#include <wtf/Optional.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

RefPtr<WebGPUDevice> WebGPUDevice::tryCreate(Ref<const WebGPUAdapter>&& adapter)
{
    if (auto device = GPUDevice::tryCreate(adapter->options()))
        return adoptRef(new WebGPUDevice(WTFMove(adapter), device.releaseNonNull()));
    return nullptr;
}

WebGPUDevice::WebGPUDevice(Ref<const WebGPUAdapter>&& adapter, Ref<GPUDevice>&& device)
    : m_adapter(WTFMove(adapter))
    , m_device(WTFMove(device))
    , m_errorScopes(GPUErrorScopes::create())
{
}

Ref<WebGPUBuffer> WebGPUDevice::createBuffer(const GPUBufferDescriptor& descriptor) const
{
    m_errorScopes->setErrorPrefix("GPUDevice.createBuffer(): ");

    auto buffer = m_device->tryCreateBuffer(descriptor, GPUBufferMappedOption::NotMapped, m_errorScopes);
    return WebGPUBuffer::create(WTFMove(buffer), m_errorScopes);
}

Vector<JSC::JSValue> WebGPUDevice::createBufferMapped(JSC::ExecState& state, const GPUBufferDescriptor& descriptor) const
{
    m_errorScopes->setErrorPrefix("GPUDevice.createBufferMapped(): ");

    JSC::JSValue wrappedArrayBuffer = JSC::jsNull();

    auto buffer = m_device->tryCreateBuffer(descriptor, GPUBufferMappedOption::IsMapped, m_errorScopes);
    if (buffer) {
        auto arrayBuffer = buffer->mapOnCreation();
        wrappedArrayBuffer = toJS(&state, JSC::jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject()), arrayBuffer);
    }

    auto webBuffer = WebGPUBuffer::create(WTFMove(buffer), m_errorScopes);
    auto wrappedWebBuffer = toJS(&state, JSC::jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject()), webBuffer);

    return { wrappedWebBuffer, wrappedArrayBuffer };
}

Ref<WebGPUTexture> WebGPUDevice::createTexture(const GPUTextureDescriptor& descriptor) const
{
    auto texture = m_device->tryCreateTexture(descriptor);
    return WebGPUTexture::create(WTFMove(texture));
}

Ref<WebGPUSampler> WebGPUDevice::createSampler(const GPUSamplerDescriptor& descriptor) const
{
    auto sampler = m_device->tryCreateSampler(descriptor);
    return WebGPUSampler::create(WTFMove(sampler));
}

Ref<WebGPUBindGroupLayout> WebGPUDevice::createBindGroupLayout(const GPUBindGroupLayoutDescriptor& descriptor) const
{
    auto layout = m_device->tryCreateBindGroupLayout(descriptor);
    return WebGPUBindGroupLayout::create(WTFMove(layout));
}

Ref<WebGPUPipelineLayout> WebGPUDevice::createPipelineLayout(const WebGPUPipelineLayoutDescriptor& descriptor) const
{
    auto gpuDescriptor = descriptor.tryCreateGPUPipelineLayoutDescriptor();
    if (!gpuDescriptor)
        return WebGPUPipelineLayout::create(nullptr);
    
    auto layout = m_device->createPipelineLayout(WTFMove(*gpuDescriptor));
    return WebGPUPipelineLayout::create(WTFMove(layout));
}

Ref<WebGPUBindGroup> WebGPUDevice::createBindGroup(const WebGPUBindGroupDescriptor& descriptor) const
{
    auto gpuDescriptor = descriptor.tryCreateGPUBindGroupDescriptor();
    if (!gpuDescriptor)
        return WebGPUBindGroup::create(nullptr);

    auto bindGroup = m_device->tryCreateBindGroup(*gpuDescriptor, m_errorScopes);
    return WebGPUBindGroup::create(WTFMove(bindGroup));
}

Ref<WebGPUShaderModule> WebGPUDevice::createShaderModule(const WebGPUShaderModuleDescriptor& descriptor) const
{
    // FIXME: What can be validated here?
    auto module = m_device->tryCreateShaderModule(GPUShaderModuleDescriptor { descriptor.code });
    return WebGPUShaderModule::create(WTFMove(module));
}

Ref<WebGPURenderPipeline> WebGPUDevice::createRenderPipeline(const WebGPURenderPipelineDescriptor& descriptor) const
{
    m_errorScopes->setErrorPrefix("GPUDevice.createRenderPipeline(): ");

    auto gpuDescriptor = descriptor.tryCreateGPURenderPipelineDescriptor(m_errorScopes);
    if (!gpuDescriptor)
        return WebGPURenderPipeline::create(nullptr);

    auto pipeline = m_device->tryCreateRenderPipeline(*gpuDescriptor, m_errorScopes);
    return WebGPURenderPipeline::create(WTFMove(pipeline));
}

Ref<WebGPUComputePipeline> WebGPUDevice::createComputePipeline(const WebGPUComputePipelineDescriptor& descriptor) const
{
    m_errorScopes->setErrorPrefix("GPUDevice.createComputePipeline(): ");

    auto gpuDescriptor = descriptor.tryCreateGPUComputePipelineDescriptor(m_errorScopes);
    if (!gpuDescriptor)
        return WebGPUComputePipeline::create(nullptr);

    auto pipeline = m_device->tryCreateComputePipeline(*gpuDescriptor, m_errorScopes);
    return WebGPUComputePipeline::create(WTFMove(pipeline));
}

Ref<WebGPUCommandEncoder> WebGPUDevice::createCommandEncoder() const
{
    auto commandBuffer = m_device->tryCreateCommandBuffer();
    return WebGPUCommandEncoder::create(WTFMove(commandBuffer));
}

Ref<WebGPUQueue> WebGPUDevice::getQueue() const
{
    if (!m_queue)
        m_queue = WebGPUQueue::create(m_device->tryGetQueue());

    return makeRef(*m_queue.get());
}

void WebGPUDevice::popErrorScope(ErrorPromise&& promise)
{
    String failMessage;
    Optional<GPUError> error = m_errorScopes->popErrorScope(failMessage);
    if (failMessage.isEmpty())
        promise.resolve(error);
    else
        promise.reject(Exception { OperationError, "GPUDevice::popErrorScope(): " + failMessage });
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
