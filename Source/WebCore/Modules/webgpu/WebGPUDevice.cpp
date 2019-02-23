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

#include "GPUBindGroup.h"
#include "GPUBindGroupBinding.h"
#include "GPUBindGroupDescriptor.h"
#include "GPUBufferBinding.h"
#include "GPUBufferDescriptor.h"
#include "GPUCommandBuffer.h"
#include "GPUPipelineStageDescriptor.h"
#include "GPURenderPipelineDescriptor.h"
#include "GPUShaderModuleDescriptor.h"
#include "GPUTextureDescriptor.h"
#include "Logging.h"
#include "WebGPUBindGroup.h"
#include "WebGPUBindGroupBinding.h"
#include "WebGPUBindGroupDescriptor.h"
#include "WebGPUBindGroupLayout.h"
#include "WebGPUBuffer.h"
#include "WebGPUBufferBinding.h"
#include "WebGPUCommandBuffer.h"
#include "WebGPUPipelineLayout.h"
#include "WebGPUPipelineLayoutDescriptor.h"
#include "WebGPUPipelineStageDescriptor.h"
#include "WebGPUQueue.h"
#include "WebGPURenderPipeline.h"
#include "WebGPURenderPipelineDescriptor.h"
#include "WebGPUShaderModule.h"
#include "WebGPUShaderModuleDescriptor.h"
#include "WebGPUTexture.h"

namespace WebCore {

RefPtr<WebGPUDevice> WebGPUDevice::create(Ref<WebGPUAdapter>&& adapter)
{
    if (auto device = GPUDevice::create(adapter->options()))
        return adoptRef(new WebGPUDevice(WTFMove(adapter), device.releaseNonNull()));
    return nullptr;
}

WebGPUDevice::WebGPUDevice(Ref<WebGPUAdapter>&& adapter, Ref<GPUDevice>&& device)
    : m_adapter(WTFMove(adapter))
    , m_device(WTFMove(device))
{
    UNUSED_PARAM(m_adapter);
}

Ref<WebGPUBuffer> WebGPUDevice::createBuffer(GPUBufferDescriptor&& descriptor) const
{
    auto buffer = m_device->tryCreateBuffer(WTFMove(descriptor));
    return WebGPUBuffer::create(WTFMove(buffer));
}

Ref<WebGPUTexture> WebGPUDevice::createTexture(GPUTextureDescriptor&& descriptor) const
{
    auto texture = m_device->tryCreateTexture(WTFMove(descriptor));
    return WebGPUTexture::create(WTFMove(texture));
}

Ref<WebGPUBindGroupLayout> WebGPUDevice::createBindGroupLayout(WebGPUBindGroupLayoutDescriptor&& descriptor) const
{
    auto layout = m_device->tryCreateBindGroupLayout(GPUBindGroupLayoutDescriptor { descriptor.bindings });
    return WebGPUBindGroupLayout::create(WTFMove(layout));
}

Ref<WebGPUPipelineLayout> WebGPUDevice::createPipelineLayout(WebGPUPipelineLayoutDescriptor&& descriptor) const
{
    // FIXME: Is an empty pipelineLayout an error?
    auto bindGroupLayouts = descriptor.bindGroupLayouts.map([] (const auto& layout) -> RefPtr<const GPUBindGroupLayout> {
        return layout->bindGroupLayout();
    });
    auto layout = m_device->createPipelineLayout(GPUPipelineLayoutDescriptor { WTFMove(bindGroupLayouts) });
    return WebGPUPipelineLayout::create(WTFMove(layout));
}

Ref<WebGPUBindGroup> WebGPUDevice::createBindGroup(WebGPUBindGroupDescriptor&& descriptor) const
{
    auto gpuDescriptor = descriptor.asGPUBindGroupDescriptor();
    if (!gpuDescriptor)
        return WebGPUBindGroup::create(nullptr);

    auto bindGroup = GPUBindGroup::create(WTFMove(*gpuDescriptor));
    return WebGPUBindGroup::create(WTFMove(bindGroup));
}

RefPtr<WebGPUShaderModule> WebGPUDevice::createShaderModule(WebGPUShaderModuleDescriptor&& descriptor) const
{
    // FIXME: What can be validated here?
    if (auto module = m_device->createShaderModule(GPUShaderModuleDescriptor { descriptor.code }))
        return WebGPUShaderModule::create(module.releaseNonNull());
    return nullptr;
}

static Optional<GPUPipelineStageDescriptor> validateAndConvertPipelineStage(const WebGPUPipelineStageDescriptor& descriptor)
{
    if (!descriptor.module || !descriptor.module->module() || descriptor.entryPoint.isEmpty())
        return WTF::nullopt;

    return GPUPipelineStageDescriptor { descriptor.module->module(), descriptor.entryPoint };
}

RefPtr<WebGPURenderPipeline> WebGPUDevice::createRenderPipeline(WebGPURenderPipelineDescriptor&& descriptor) const
{
    auto pipelineLayout = descriptor.layout ? descriptor.layout->pipelineLayout() : nullptr;

    auto vertexStage = validateAndConvertPipelineStage(descriptor.vertexStage);
    auto fragmentStage = validateAndConvertPipelineStage(descriptor.fragmentStage);

    if (!vertexStage || !fragmentStage) {
        LOG(WebGPU, "WebGPUDevice::createRenderPipeline(): Invalid WebGPUPipelineStageDescriptor!");
        return nullptr;
    }

    if (auto pipeline = m_device->createRenderPipeline(GPURenderPipelineDescriptor { WTFMove(pipelineLayout), WTFMove(*vertexStage), WTFMove(*fragmentStage), descriptor.primitiveTopology, WTFMove(descriptor.depthStencilState), WTFMove(descriptor.inputState) }))
        return WebGPURenderPipeline::create(pipeline.releaseNonNull());
    return nullptr;
}

RefPtr<WebGPUCommandBuffer> WebGPUDevice::createCommandBuffer() const
{
    if (auto commandBuffer = m_device->createCommandBuffer())
        return WebGPUCommandBuffer::create(commandBuffer.releaseNonNull());
    return nullptr;
}

RefPtr<WebGPUQueue> WebGPUDevice::getQueue()
{
    if (!m_queue)
        m_queue = WebGPUQueue::create(m_device->getQueue());

    return m_queue;
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
