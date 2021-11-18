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
#include "RemoteDevice.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUCommandEncoderDescriptor.h"
#include "WebGPUObjectHeap.h"
#include <pal/graphics/WebGPU/WebGPUDevice.h>

namespace WebKit {

RemoteDevice::RemoteDevice(PAL::WebGPU::Device& device, WebGPU::ObjectHeap& objectHeap)
    : m_backing(device)
    , m_objectHeap(objectHeap)
{
}

RemoteDevice::~RemoteDevice()
{
}

void RemoteDevice::destroy()
{
}

void RemoteDevice::createBuffer(const WebGPU::BufferDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::createTexture(const WebGPU::TextureDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::createSampler(const WebGPU::SamplerDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::importExternalTexture(const WebGPU::ExternalTextureDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::createBindGroupLayout(const WebGPU::BindGroupLayoutDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::createPipelineLayout(const WebGPU::PipelineLayoutDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::createBindGroup(const WebGPU::BindGroupDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::createShaderModule(const WebGPU::ShaderModuleDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::createComputePipeline(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::createRenderPipeline(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::createComputePipelineAsync(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUIdentifier identifier, WTF::CompletionHandler<void()>&& callback)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(callback);
}

void RemoteDevice::createRenderPipelineAsync(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPUIdentifier identifier, WTF::CompletionHandler<void()>&& callback)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(callback);
}

void RemoteDevice::createCommandEncoder(const std::optional<WebGPU::CommandEncoderDescriptor>& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::createRenderBundleEncoder(const WebGPU::RenderBundleEncoderDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::createQuerySet(const WebGPU::QuerySetDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteDevice::pushErrorScope(PAL::WebGPU::ErrorFilter errorFilter)
{
    UNUSED_PARAM(errorFilter);
}

void RemoteDevice::popErrorScope(WTF::CompletionHandler<void(std::optional<WebGPU::Error>&&)>&& callback)
{
    UNUSED_PARAM(callback);
}

void RemoteDevice::setLabel(String&& label)
{
    UNUSED_PARAM(label);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
