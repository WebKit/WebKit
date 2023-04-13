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

#include "config.h"
#include "WebGPUObjectHeap.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapter.h"
#include "RemoteBindGroup.h"
#include "RemoteBindGroupLayout.h"
#include "RemoteBuffer.h"
#include "RemoteCommandBuffer.h"
#include "RemoteCommandEncoder.h"
#include "RemoteCompositorIntegration.h"
#include "RemoteComputePassEncoder.h"
#include "RemoteComputePipeline.h"
#include "RemoteDevice.h"
#include "RemoteExternalTexture.h"
#include "RemoteGPU.h"
#include "RemotePipelineLayout.h"
#include "RemotePresentationContext.h"
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

ObjectHeap::ObjectHeap() = default;

ObjectHeap::~ObjectHeap() = default;

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteAdapter& adapter)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteAdapter> { Ref { adapter } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBindGroup& bindGroup)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroup> { Ref { bindGroup } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBindGroupLayout& bindGroupLayout)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroupLayout> { Ref { bindGroupLayout } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBuffer& buffer)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteBuffer> { Ref { buffer } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteCommandBuffer& commandBuffer)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteCommandBuffer> { Ref { commandBuffer } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteCommandEncoder& commandEncoder)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteCommandEncoder> { Ref { commandEncoder } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteCompositorIntegration& querySet)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteCompositorIntegration> { Ref { querySet } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteComputePassEncoder& computePassEncoder)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder> { Ref { computePassEncoder } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteComputePipeline& computePipeline)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline> { Ref { computePipeline } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteDevice& device)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteDevice> { Ref { device } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteExternalTexture& externalTexture)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture> { Ref { externalTexture } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemotePipelineLayout& pipelineLayout)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout> { Ref { pipelineLayout } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemotePresentationContext& presentationContext)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemotePresentationContext> { Ref { presentationContext } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteQuerySet& querySet)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet> { Ref { querySet } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteQueue& queue)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteQueue> { Ref { queue } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderBundleEncoder& renderBundleEncoder)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder> { Ref { renderBundleEncoder } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderBundle& renderBundle)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle> { Ref { renderBundle } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderPassEncoder& renderPassEncoder)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder> { Ref { renderPassEncoder } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderPipeline& renderPipeline)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline> { Ref { renderPipeline } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteSampler& sampler)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteSampler> { Ref { sampler } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteShaderModule& shaderModule)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule> { Ref { shaderModule } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteTexture& texture)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteTexture> { Ref { texture } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteTextureView& textureView)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView> { Ref { textureView } } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::removeObject(WebGPUIdentifier identifier)
{
    auto result = m_objects.remove(identifier);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::clear()
{
    m_objects.clear();
}

PAL::WebGPU::Adapter* ObjectHeap::convertAdapterFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteAdapter>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteAdapter>>(iterator->value)->backing();
}

PAL::WebGPU::BindGroup* ObjectHeap::convertBindGroupFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroup>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroup>>(iterator->value)->backing();
}

PAL::WebGPU::BindGroupLayout* ObjectHeap::convertBindGroupLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroupLayout>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroupLayout>>(iterator->value)->backing();
}

PAL::WebGPU::Buffer* ObjectHeap::convertBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteBuffer>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteBuffer>>(iterator->value)->backing();
}

PAL::WebGPU::CommandBuffer* ObjectHeap::convertCommandBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandBuffer>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandBuffer>>(iterator->value)->backing();
}

PAL::WebGPU::CommandEncoder* ObjectHeap::convertCommandEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandEncoder>>(iterator->value)->backing();
}

PAL::WebGPU::CompositorIntegration* ObjectHeap::convertCompositorIntegrationFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteCompositorIntegration>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteCompositorIntegration>>(iterator->value)->backing();
}

PAL::WebGPU::ComputePassEncoder* ObjectHeap::convertComputePassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder>>(iterator->value)->backing();
}

PAL::WebGPU::ComputePipeline* ObjectHeap::convertComputePipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline>>(iterator->value)->backing();
}

PAL::WebGPU::Device* ObjectHeap::convertDeviceFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteDevice>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteDevice>>(iterator->value)->backing();
}

PAL::WebGPU::ExternalTexture* ObjectHeap::convertExternalTextureFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture>>(iterator->value)->backing();
}

PAL::WebGPU::PipelineLayout* ObjectHeap::convertPipelineLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout>>(iterator->value)->backing();
}

PAL::WebGPU::PresentationContext* ObjectHeap::convertPresentationContextFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemotePresentationContext>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemotePresentationContext>>(iterator->value)->backing();
}

PAL::WebGPU::QuerySet* ObjectHeap::convertQuerySetFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet>>(iterator->value)->backing();
}

PAL::WebGPU::Queue* ObjectHeap::convertQueueFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteQueue>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteQueue>>(iterator->value)->backing();
}

PAL::WebGPU::RenderBundleEncoder* ObjectHeap::convertRenderBundleEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder>>(iterator->value)->backing();
}

PAL::WebGPU::RenderBundle* ObjectHeap::convertRenderBundleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle>>(iterator->value)->backing();
}

PAL::WebGPU::RenderPassEncoder* ObjectHeap::convertRenderPassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder>>(iterator->value)->backing();
}

PAL::WebGPU::RenderPipeline* ObjectHeap::convertRenderPipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline>>(iterator->value)->backing();
}

PAL::WebGPU::Sampler* ObjectHeap::convertSamplerFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteSampler>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteSampler>>(iterator->value)->backing();
}

PAL::WebGPU::ShaderModule* ObjectHeap::convertShaderModuleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule>>(iterator->value)->backing();
}

PAL::WebGPU::Texture* ObjectHeap::convertTextureFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteTexture>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteTexture>>(iterator->value)->backing();
}

PAL::WebGPU::TextureView* ObjectHeap::convertTextureViewFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView>>(iterator->value)->backing();
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
