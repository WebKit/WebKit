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

#include "GPUCommandBuffer.h"
#include "GPUPipelineStageDescriptor.h"
#include "GPURenderPipelineDescriptor.h"
#include "GPUShaderModuleDescriptor.h"
#include "Logging.h"
#include "WebGPUBindGroupLayout.h"
#include "WebGPUBuffer.h"
#include "WebGPUCommandBuffer.h"
#include "WebGPUPipelineStageDescriptor.h"
#include "WebGPUQueue.h"
#include "WebGPURenderPipeline.h"
#include "WebGPURenderPipelineDescriptor.h"
#include "WebGPUShaderModule.h"
#include "WebGPUShaderModuleDescriptor.h"

namespace WebCore {

RefPtr<WebGPUDevice> WebGPUDevice::create(Ref<WebGPUAdapter>&& adapter)
{
    auto device = GPUDevice::create(); // FIXME: Take adapter into account when creating m_device.
    return device ? adoptRef(new WebGPUDevice(WTFMove(adapter), device.releaseNonNull())) : nullptr;
}

WebGPUDevice::WebGPUDevice(Ref<WebGPUAdapter>&& adapter, Ref<GPUDevice>&& device)
    : m_adapter(WTFMove(adapter))
    , m_device(WTFMove(device))
{
    UNUSED_PARAM(m_adapter);
}

RefPtr<WebGPUBuffer> WebGPUDevice::createBuffer(WebGPUBufferDescriptor&& descriptor) const
{
    // FIXME: Validation on descriptor needed?
    auto buffer = m_device->createBuffer(GPUBufferDescriptor { descriptor.size, descriptor.usage });
    return buffer ? WebGPUBuffer::create(buffer.releaseNonNull()) : nullptr;
}

Ref<WebGPUBindGroupLayout> WebGPUDevice::createBindGroupLayout(WebGPUBindGroupLayoutDescriptor&& descriptor) const
{
    auto layout = m_device->createBindGroupLayout(GPUBindGroupLayoutDescriptor { descriptor.bindings });
    return WebGPUBindGroupLayout::create(WTFMove(layout));
}

RefPtr<WebGPUShaderModule> WebGPUDevice::createShaderModule(WebGPUShaderModuleDescriptor&& descriptor) const
{
    // FIXME: What can be validated here?
    auto module = m_device->createShaderModule(GPUShaderModuleDescriptor { descriptor.code });
    return module ? WebGPUShaderModule::create(module.releaseNonNull()) : nullptr;
}

static std::optional<GPUPipelineStageDescriptor> validateAndConvertPipelineStage(const WebGPUPipelineStageDescriptor& descriptor)
{
    if (!descriptor.module || !descriptor.module->module() || descriptor.entryPoint.isEmpty())
        return std::nullopt;

    return GPUPipelineStageDescriptor { descriptor.module->module(), descriptor.entryPoint };
}

RefPtr<WebGPURenderPipeline> WebGPUDevice::createRenderPipeline(WebGPURenderPipelineDescriptor&& descriptor) const
{
    auto vertexStage = validateAndConvertPipelineStage(descriptor.vertexStage);
    auto fragmentStage = validateAndConvertPipelineStage(descriptor.fragmentStage);

    if (!vertexStage || !fragmentStage) {
        LOG(WebGPU, "WebGPUDevice::createRenderPipeline(): Invalid WebGPUPipelineStageDescriptor!");
        return nullptr;
    }

    auto pipeline = m_device->createRenderPipeline(GPURenderPipelineDescriptor { WTFMove(*vertexStage), WTFMove(*fragmentStage), descriptor.primitiveTopology, descriptor.inputState });
    return pipeline ? WebGPURenderPipeline::create(pipeline.releaseNonNull()) : nullptr;
}

RefPtr<WebGPUCommandBuffer> WebGPUDevice::createCommandBuffer() const
{
    auto commandBuffer = m_device->createCommandBuffer();
    return commandBuffer ? WebGPUCommandBuffer::create(commandBuffer.releaseNonNull()) : nullptr;
}

RefPtr<WebGPUQueue> WebGPUDevice::getQueue()
{
    if (!m_queue)
        m_queue = WebGPUQueue::create(m_device->getQueue());

    return m_queue;
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
