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
#include "RemoteDeviceProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteBindGroupLayoutProxy.h"
#include "RemoteBindGroupProxy.h"
#include "RemoteBufferProxy.h"
#include "RemoteCommandEncoderProxy.h"
#include "RemoteComputePipelineProxy.h"
#include "RemoteExternalTextureProxy.h"
#include "RemotePipelineLayoutProxy.h"
#include "RemoteQuerySetProxy.h"
#include "RemoteRenderBundleEncoderProxy.h"
#include "RemoteRenderPipelineProxy.h"
#include "RemoteSamplerProxy.h"
#include "RemoteShaderModuleProxy.h"
#include "RemoteTextureProxy.h"
#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

RemoteDeviceProxy::RemoteDeviceProxy(Ref<PAL::WebGPU::SupportedFeatures>&& features, Ref<PAL::WebGPU::SupportedLimits>&& limits, ConvertToBackingContext& convertToBackingContext)
    : Device(WTFMove(features), WTFMove(limits))
    , m_convertToBackingContext(convertToBackingContext)
{
}

RemoteDeviceProxy::~RemoteDeviceProxy()
{
}

void RemoteDeviceProxy::destroy()
{
}

Ref<PAL::WebGPU::Buffer> RemoteDeviceProxy::createBuffer(const PAL::WebGPU::BufferDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteBufferProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::Texture> RemoteDeviceProxy::createTexture(const PAL::WebGPU::TextureDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteTextureProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::Sampler> RemoteDeviceProxy::createSampler(const PAL::WebGPU::SamplerDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteSamplerProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::ExternalTexture> RemoteDeviceProxy::importExternalTexture(const PAL::WebGPU::ExternalTextureDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteExternalTextureProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::BindGroupLayout> RemoteDeviceProxy::createBindGroupLayout(const PAL::WebGPU::BindGroupLayoutDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteBindGroupLayoutProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::PipelineLayout> RemoteDeviceProxy::createPipelineLayout(const PAL::WebGPU::PipelineLayoutDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemotePipelineLayoutProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::BindGroup> RemoteDeviceProxy::createBindGroup(const PAL::WebGPU::BindGroupDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteBindGroupProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::ShaderModule> RemoteDeviceProxy::createShaderModule(const PAL::WebGPU::ShaderModuleDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteShaderModuleProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::ComputePipeline> RemoteDeviceProxy::createComputePipeline(const PAL::WebGPU::ComputePipelineDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteComputePipelineProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::RenderPipeline> RemoteDeviceProxy::createRenderPipeline(const PAL::WebGPU::RenderPipelineDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteRenderPipelineProxy::create(m_convertToBackingContext);
}

void RemoteDeviceProxy::createComputePipelineAsync(const PAL::WebGPU::ComputePipelineDescriptor& descriptor, WTF::Function<void(Ref<PAL::WebGPU::ComputePipeline>&&)>&& callback)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(callback);
}

void RemoteDeviceProxy::createRenderPipelineAsync(const PAL::WebGPU::RenderPipelineDescriptor& descriptor, WTF::Function<void(Ref<PAL::WebGPU::RenderPipeline>&&)>&& callback)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(callback);
}

Ref<PAL::WebGPU::CommandEncoder> RemoteDeviceProxy::createCommandEncoder(const std::optional<PAL::WebGPU::CommandEncoderDescriptor>& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteCommandEncoderProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::RenderBundleEncoder> RemoteDeviceProxy::createRenderBundleEncoder(const PAL::WebGPU::RenderBundleEncoderDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteRenderBundleEncoderProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::QuerySet> RemoteDeviceProxy::createQuerySet(const PAL::WebGPU::QuerySetDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteQuerySetProxy::create(m_convertToBackingContext);
}

void RemoteDeviceProxy::pushErrorScope(PAL::WebGPU::ErrorFilter errorFilter)
{
    UNUSED_PARAM(errorFilter);
}

void RemoteDeviceProxy::popErrorScope(WTF::Function<void(std::optional<PAL::WebGPU::Error>&&)>&& callback)
{
    UNUSED_PARAM(callback);
}

void RemoteDeviceProxy::setLabelInternal(const String& label)
{
    UNUSED_PARAM(label);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
