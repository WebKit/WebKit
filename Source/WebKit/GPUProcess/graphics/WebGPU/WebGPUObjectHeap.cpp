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

ObjectHeap::ObjectHeap()
{
}

ObjectHeap::~ObjectHeap()
{
}

const PAL::WebGPU::Adapter* ObjectHeap::convertAdapterFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::Adapter>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::Adapter>>(iterator->value).ptr();
}

const PAL::WebGPU::BindGroup* ObjectHeap::convertBindGroupFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::BindGroup>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::BindGroup>>(iterator->value).ptr();
}

const PAL::WebGPU::BindGroupLayout* ObjectHeap::convertBindGroupLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::BindGroupLayout>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::BindGroupLayout>>(iterator->value).ptr();
}

const PAL::WebGPU::Buffer* ObjectHeap::convertBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::Buffer>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::Buffer>>(iterator->value).ptr();
}

const PAL::WebGPU::CommandBuffer* ObjectHeap::convertCommandBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::CommandBuffer>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::CommandBuffer>>(iterator->value).ptr();
}

const PAL::WebGPU::CommandEncoder* ObjectHeap::convertCommandEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::CommandEncoder>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::CommandEncoder>>(iterator->value).ptr();
}

const PAL::WebGPU::ComputePassEncoder* ObjectHeap::convertComputePassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::ComputePassEncoder>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::ComputePassEncoder>>(iterator->value).ptr();
}

const PAL::WebGPU::ComputePipeline* ObjectHeap::convertComputePipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::ComputePipeline>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::ComputePipeline>>(iterator->value).ptr();
}

const PAL::WebGPU::Device* ObjectHeap::convertDeviceFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::Device>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::Device>>(iterator->value).ptr();
}

const PAL::WebGPU::GPU* ObjectHeap::convertGPUFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::GPU>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::GPU>>(iterator->value).ptr();
}

const PAL::WebGPU::PipelineLayout* ObjectHeap::convertPipelineLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::PipelineLayout>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::PipelineLayout>>(iterator->value).ptr();
}

const PAL::WebGPU::QuerySet* ObjectHeap::convertQuerySetFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::QuerySet>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::QuerySet>>(iterator->value).ptr();
}

const PAL::WebGPU::Queue* ObjectHeap::convertQueueFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::Queue>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::Queue>>(iterator->value).ptr();
}

const PAL::WebGPU::RenderBundleEncoder* ObjectHeap::convertRenderBundleEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::RenderBundleEncoder>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::RenderBundleEncoder>>(iterator->value).ptr();
}

const PAL::WebGPU::RenderBundle* ObjectHeap::convertRenderBundleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::RenderBundle>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::RenderBundle>>(iterator->value).ptr();
}

const PAL::WebGPU::RenderPassEncoder* ObjectHeap::convertRenderPassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::RenderPassEncoder>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::RenderPassEncoder>>(iterator->value).ptr();
}

const PAL::WebGPU::RenderPipeline* ObjectHeap::convertRenderPipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::RenderPipeline>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::RenderPipeline>>(iterator->value).ptr();
}

const PAL::WebGPU::Sampler* ObjectHeap::convertSamplerFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::Sampler>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::Sampler>>(iterator->value).ptr();
}

const PAL::WebGPU::ShaderModule* ObjectHeap::convertShaderModuleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::ShaderModule>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::ShaderModule>>(iterator->value).ptr();
}

const PAL::WebGPU::Texture* ObjectHeap::convertTextureFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::Texture>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::Texture>>(iterator->value).ptr();
}

const PAL::WebGPU::TextureView* ObjectHeap::convertTextureViewFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<Ref<PAL::WebGPU::TextureView>>(iterator->value))
        return nullptr;
    return std::get<Ref<PAL::WebGPU::TextureView>>(iterator->value).ptr();
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
