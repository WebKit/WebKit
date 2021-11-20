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
#include "WebGPUObjectRegistry.h"

#if ENABLE(GPU_PROCESS)

#include <pal/graphics/WebGPU/WebGPU.h>
#include <pal/graphics/WebGPU/WebGPUAdapter.h>
#include <pal/graphics/WebGPU/WebGPUBindGroup.h>
#include <pal/graphics/WebGPU/WebGPUBindGroupLayout.h>
#include <pal/graphics/WebGPU/WebGPUBuffer.h>
#include <pal/graphics/WebGPU/WebGPUCommandBuffer.h>
#include <pal/graphics/WebGPU/WebGPUCommandEncoder.h>
#include <pal/graphics/WebGPU/WebGPUComputePassEncoder.h>
#include <pal/graphics/WebGPU/WebGPUComputePipeline.h>
#include <pal/graphics/WebGPU/WebGPUDevice.h>
#include <pal/graphics/WebGPU/WebGPUExternalTexture.h>
#include <pal/graphics/WebGPU/WebGPUPipelineLayout.h>
#include <pal/graphics/WebGPU/WebGPUQuerySet.h>
#include <pal/graphics/WebGPU/WebGPUQueue.h>
#include <pal/graphics/WebGPU/WebGPURenderBundle.h>
#include <pal/graphics/WebGPU/WebGPURenderBundleEncoder.h>
#include <pal/graphics/WebGPU/WebGPURenderPassEncoder.h>
#include <pal/graphics/WebGPU/WebGPURenderPipeline.h>
#include <pal/graphics/WebGPU/WebGPUSampler.h>
#include <pal/graphics/WebGPU/WebGPUShaderModule.h>
#include <pal/graphics/WebGPU/WebGPUTexture.h>
#include <pal/graphics/WebGPU/WebGPUTextureView.h>

namespace WebKit::WebGPU {

ObjectRegistry::ObjectRegistry()
{
}

ObjectRegistry::~ObjectRegistry()
{
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::Adapter& adapter)
{
    auto result = m_objects.add(identifier, adapter);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::BindGroup& bindGroup)
{
    auto result = m_objects.add(identifier, bindGroup);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::BindGroupLayout& bindGroupLayout)
{
    auto result = m_objects.add(identifier, bindGroupLayout);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::Buffer& buffer)
{
    auto result = m_objects.add(identifier, buffer);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::CommandBuffer& commandBuffer)
{
    auto result = m_objects.add(identifier, commandBuffer);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::CommandEncoder& commandEncoder)
{
    auto result = m_objects.add(identifier, commandEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::ComputePassEncoder& computePassEncoder)
{
    auto result = m_objects.add(identifier, computePassEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::ComputePipeline& computePipeline)
{
    auto result = m_objects.add(identifier, computePipeline);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::Device& device)
{
    auto result = m_objects.add(identifier, device);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::ExternalTexture& externalTexture)
{
    auto result = m_objects.add(identifier, externalTexture);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::GPU& gpu)
{
    auto result = m_objects.add(identifier, gpu);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::PipelineLayout& pipelineLayout)
{
    auto result = m_objects.add(identifier, pipelineLayout);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::QuerySet& querySet)
{
    auto result = m_objects.add(identifier, querySet);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::Queue& queue)
{
    auto result = m_objects.add(identifier, queue);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::RenderBundleEncoder& renderBundleEncoder)
{
    auto result = m_objects.add(identifier, renderBundleEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::RenderBundle& renderBundle)
{
    auto result = m_objects.add(identifier, renderBundle);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::RenderPassEncoder& renderPassEncoder)
{
    auto result = m_objects.add(identifier, renderPassEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::RenderPipeline& renderPipeline)
{
    auto result = m_objects.add(identifier, renderPipeline);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::Sampler& sampler)
{
    auto result = m_objects.add(identifier, sampler);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::ShaderModule& shaderModule)
{
    auto result = m_objects.add(identifier, shaderModule);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::Texture& texture)
{
    auto result = m_objects.add(identifier, texture);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::addObject(WebGPUIdentifier identifier, PAL::WebGPU::TextureView& textureView)
{
    auto result = m_objects.add(identifier, textureView);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectRegistry::removeObject(WebGPUIdentifier identifier)
{
    auto result = m_objects.remove(identifier);
    ASSERT_UNUSED(result, result);
}

PAL::WebGPU::Adapter* ObjectRegistry::convertAdapterFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::Adapter>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::Adapter>>(iterator->value).get();
}

PAL::WebGPU::BindGroup* ObjectRegistry::convertBindGroupFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::BindGroup>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::BindGroup>>(iterator->value).get();
}

PAL::WebGPU::BindGroupLayout* ObjectRegistry::convertBindGroupLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::BindGroupLayout>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::BindGroupLayout>>(iterator->value).get();
}

PAL::WebGPU::Buffer* ObjectRegistry::convertBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::Buffer>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::Buffer>>(iterator->value).get();
}

PAL::WebGPU::CommandBuffer* ObjectRegistry::convertCommandBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::CommandBuffer>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::CommandBuffer>>(iterator->value).get();
}

PAL::WebGPU::CommandEncoder* ObjectRegistry::convertCommandEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::CommandEncoder>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::CommandEncoder>>(iterator->value).get();
}

PAL::WebGPU::ComputePassEncoder* ObjectRegistry::convertComputePassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::ComputePassEncoder>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::ComputePassEncoder>>(iterator->value).get();
}

PAL::WebGPU::ComputePipeline* ObjectRegistry::convertComputePipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::ComputePipeline>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::ComputePipeline>>(iterator->value).get();
}

PAL::WebGPU::Device* ObjectRegistry::convertDeviceFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::Device>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::Device>>(iterator->value).get();
}

PAL::WebGPU::ExternalTexture* ObjectRegistry::convertExternalTextureFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::ExternalTexture>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::ExternalTexture>>(iterator->value).get();
}

PAL::WebGPU::GPU* ObjectRegistry::convertGPUFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::GPU>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::GPU>>(iterator->value).get();
}

PAL::WebGPU::PipelineLayout* ObjectRegistry::convertPipelineLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::PipelineLayout>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::PipelineLayout>>(iterator->value).get();
}

PAL::WebGPU::QuerySet* ObjectRegistry::convertQuerySetFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::QuerySet>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::QuerySet>>(iterator->value).get();
}

PAL::WebGPU::Queue* ObjectRegistry::convertQueueFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::Queue>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::Queue>>(iterator->value).get();
}

PAL::WebGPU::RenderBundleEncoder* ObjectRegistry::convertRenderBundleEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::RenderBundleEncoder>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::RenderBundleEncoder>>(iterator->value).get();
}

PAL::WebGPU::RenderBundle* ObjectRegistry::convertRenderBundleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::RenderBundle>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::RenderBundle>>(iterator->value).get();
}

PAL::WebGPU::RenderPassEncoder* ObjectRegistry::convertRenderPassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::RenderPassEncoder>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::RenderPassEncoder>>(iterator->value).get();
}

PAL::WebGPU::RenderPipeline* ObjectRegistry::convertRenderPipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::RenderPipeline>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::RenderPipeline>>(iterator->value).get();
}

PAL::WebGPU::Sampler* ObjectRegistry::convertSamplerFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::Sampler>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::Sampler>>(iterator->value).get();
}

PAL::WebGPU::ShaderModule* ObjectRegistry::convertShaderModuleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::ShaderModule>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::ShaderModule>>(iterator->value).get();
}

PAL::WebGPU::Texture* ObjectRegistry::convertTextureFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::Texture>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::Texture>>(iterator->value).get();
}

PAL::WebGPU::TextureView* ObjectRegistry::convertTextureViewFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<std::reference_wrapper<PAL::WebGPU::TextureView>>(iterator->value))
        return nullptr;
    return &std::get<std::reference_wrapper<PAL::WebGPU::TextureView>>(iterator->value).get();
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
