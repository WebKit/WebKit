/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WebGPUObjectHeap.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapter.h"
#include "RemoteBindGroup.h"
#include "RemoteBindGroupLayout.h"
#include "RemoteBuffer.h"
#include "RemoteCommandBuffer.h"
#include "RemoteCommandEncoder.h"
#include "RemoteComputePassEncoder.h"
#include "RemoteComputePipeline.h"
#include "RemoteDevice.h"
#include "RemoteExternalTexture.h"
#include "RemoteGPU.h"
#include "RemotePipelineLayout.h"
#include "RemoteQuerySet.h"
#include "RemoteQueue.h"
#include "RemoteRenderBundle.h"
#include "RemoteRenderBundleEncoder.h"
#include "RemoteRenderPassEncoder.h"
#include "RemoteRenderPipeline.h"
#include "RemoteSampler.h"
#include "RemoteShaderModule.h"
#include "RemoteTexture.h"
#include "RemoteTextureView.h"

namespace WebKit::WebGPU {

ObjectHeap::ObjectHeap()
{
}

ObjectHeap::~ObjectHeap()
{
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteAdapter& adapter)
{
    auto result = m_objects.add(identifier, adapter);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBindGroup& bindGroup)
{
    auto result = m_objects.add(identifier, bindGroup);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBindGroupLayout& bindGroupLayout)
{
    auto result = m_objects.add(identifier, bindGroupLayout);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBuffer& buffer)
{
    auto result = m_objects.add(identifier, buffer);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteCommandBuffer& commandBuffer)
{
    auto result = m_objects.add(identifier, commandBuffer);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteCommandEncoder& commandEncoder)
{
    auto result = m_objects.add(identifier, commandEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteComputePassEncoder& computePassEncoder)
{
    auto result = m_objects.add(identifier, computePassEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteComputePipeline& computePipeline)
{
    auto result = m_objects.add(identifier, computePipeline);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteDevice& device)
{
    auto result = m_objects.add(identifier, device);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteExternalTexture& externalTexture)
{
    auto result = m_objects.add(identifier, externalTexture);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemotePipelineLayout& pipelineLayout)
{
    auto result = m_objects.add(identifier, pipelineLayout);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteQuerySet& querySet)
{
    auto result = m_objects.add(identifier, querySet);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteQueue& queue)
{
    auto result = m_objects.add(identifier, queue);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderBundleEncoder& renderBundleEncoder)
{
    auto result = m_objects.add(identifier, renderBundleEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderBundle& renderBundle)
{
    auto result = m_objects.add(identifier, renderBundle);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderPassEncoder& renderPassEncoder)
{
    auto result = m_objects.add(identifier, renderPassEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderPipeline& renderPipeline)
{
    auto result = m_objects.add(identifier, renderPipeline);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteSampler& sampler)
{
    auto result = m_objects.add(identifier, sampler);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteShaderModule& shaderModule)
{
    auto result = m_objects.add(identifier, shaderModule);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteTexture& texture)
{
    auto result = m_objects.add(identifier, texture);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteTextureView& textureView)
{
    auto result = m_objects.add(identifier, textureView);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::removeObject(WebGPUIdentifier identifier)
{
    auto result = m_objects.remove(identifier);
    ASSERT_UNUSED(result, result);
}

PAL::WebGPU::Adapter* ObjectHeap::convertAdapterFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteAdapter>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteAdapter>>(iterator->value)->backing();
}

PAL::WebGPU::BindGroup* ObjectHeap::convertBindGroupFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteBindGroup>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteBindGroup>>(iterator->value)->backing();
}

PAL::WebGPU::BindGroupLayout* ObjectHeap::convertBindGroupLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteBindGroupLayout>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteBindGroupLayout>>(iterator->value)->backing();
}

PAL::WebGPU::Buffer* ObjectHeap::convertBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteBuffer>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteBuffer>>(iterator->value)->backing();
}

PAL::WebGPU::CommandBuffer* ObjectHeap::convertCommandBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteCommandBuffer>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteCommandBuffer>>(iterator->value)->backing();
}

PAL::WebGPU::CommandEncoder* ObjectHeap::convertCommandEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteCommandEncoder>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteCommandEncoder>>(iterator->value)->backing();
}

PAL::WebGPU::ComputePassEncoder* ObjectHeap::convertComputePassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteComputePassEncoder>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteComputePassEncoder>>(iterator->value)->backing();
}

PAL::WebGPU::ComputePipeline* ObjectHeap::convertComputePipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteComputePipeline>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteComputePipeline>>(iterator->value)->backing();
}

PAL::WebGPU::Device* ObjectHeap::convertDeviceFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteDevice>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteDevice>>(iterator->value)->backing();
}

PAL::WebGPU::ExternalTexture* ObjectHeap::convertExternalTextureFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteExternalTexture>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteExternalTexture>>(iterator->value)->backing();
}

PAL::WebGPU::PipelineLayout* ObjectHeap::convertPipelineLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemotePipelineLayout>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemotePipelineLayout>>(iterator->value)->backing();
}

PAL::WebGPU::QuerySet* ObjectHeap::convertQuerySetFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteQuerySet>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteQuerySet>>(iterator->value)->backing();
}

PAL::WebGPU::Queue* ObjectHeap::convertQueueFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteQueue>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteQueue>>(iterator->value)->backing();
}

PAL::WebGPU::RenderBundleEncoder* ObjectHeap::convertRenderBundleEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteRenderBundleEncoder>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteRenderBundleEncoder>>(iterator->value)->backing();
}

PAL::WebGPU::RenderBundle* ObjectHeap::convertRenderBundleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteRenderBundle>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteRenderBundle>>(iterator->value)->backing();
}

PAL::WebGPU::RenderPassEncoder* ObjectHeap::convertRenderPassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteRenderPassEncoder>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteRenderPassEncoder>>(iterator->value)->backing();
}

PAL::WebGPU::RenderPipeline* ObjectHeap::convertRenderPipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteRenderPipeline>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteRenderPipeline>>(iterator->value)->backing();
}

PAL::WebGPU::Sampler* ObjectHeap::convertSamplerFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteSampler>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteSampler>>(iterator->value)->backing();
}

PAL::WebGPU::ShaderModule* ObjectHeap::convertShaderModuleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteShaderModule>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteShaderModule>>(iterator->value)->backing();
}

PAL::WebGPU::Texture* ObjectHeap::convertTextureFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteTexture>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteTexture>>(iterator->value)->backing();
}

PAL::WebGPU::TextureView* ObjectHeap::convertTextureViewFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<RemoteTextureView>>(iterator->value))
        return nullptr;
    return &std::get<Ref<RemoteTextureView>>(iterator->value)->backing();
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
