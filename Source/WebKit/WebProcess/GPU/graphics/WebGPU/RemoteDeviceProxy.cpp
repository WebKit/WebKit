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
#include "RemoteDeviceProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteBindGroupLayoutProxy.h"
#include "RemoteBindGroupProxy.h"
#include "RemoteBufferProxy.h"
#include "RemoteCommandEncoderProxy.h"
#include "RemoteComputePipelineProxy.h"
#include "RemoteDeviceMessages.h"
#include "RemoteExternalTextureProxy.h"
#include "RemotePipelineLayoutProxy.h"
#include "RemoteQuerySetProxy.h"
#include "RemoteQueueProxy.h"
#include "RemoteRenderBundleEncoderProxy.h"
#include "RemoteRenderPipelineProxy.h"
#include "RemoteSamplerProxy.h"
#include "RemoteShaderModuleProxy.h"
#include "RemoteTextureProxy.h"
#include "WebGPUCommandEncoderDescriptor.h"
#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

RemoteDeviceProxy::RemoteDeviceProxy(Ref<PAL::WebGPU::SupportedFeatures>&& features, Ref<PAL::WebGPU::SupportedLimits>&& limits, RemoteAdapterProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier)
    : Device(WTFMove(features), WTFMove(limits))
    , m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
    , m_queue(RemoteQueueProxy::create(*this, convertToBackingContext, queueIdentifier))
{
}

RemoteDeviceProxy::~RemoteDeviceProxy()
{
}

Ref<PAL::WebGPU::Queue> RemoteDeviceProxy::queue()
{
    return m_queue;
}

void RemoteDeviceProxy::destroy()
{
    auto sendResult = send(Messages::RemoteDevice::Destroy());
    UNUSED_VARIABLE(sendResult);
}

Ref<PAL::WebGPU::Buffer> RemoteDeviceProxy::createBuffer(const PAL::WebGPU::BufferDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteBufferProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBuffer(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteBufferProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::Texture> RemoteDeviceProxy::createTexture(const PAL::WebGPU::TextureDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteTextureProxy::create(root(), m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateTexture(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteTextureProxy::create(root(), m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::Texture> RemoteDeviceProxy::createSurfaceTexture(const PAL::WebGPU::TextureDescriptor& descriptor, const PAL::WebGPU::PresentationContext& presentationContext)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteTextureProxy::create(root(), m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateSurfaceTexture(m_convertToBackingContext->convertToBacking(presentationContext), *convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteTextureProxy::create(root(), m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::Sampler> RemoteDeviceProxy::createSampler(const PAL::WebGPU::SamplerDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteSamplerProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateSampler(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteSamplerProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::ExternalTexture> RemoteDeviceProxy::importExternalTexture(const PAL::WebGPU::ExternalTextureDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteExternalTextureProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::ImportExternalTexture(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteExternalTextureProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::BindGroupLayout> RemoteDeviceProxy::createBindGroupLayout(const PAL::WebGPU::BindGroupLayoutDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteBindGroupLayoutProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBindGroupLayout(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteBindGroupLayoutProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::PipelineLayout> RemoteDeviceProxy::createPipelineLayout(const PAL::WebGPU::PipelineLayoutDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemotePipelineLayoutProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreatePipelineLayout(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemotePipelineLayoutProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::BindGroup> RemoteDeviceProxy::createBindGroup(const PAL::WebGPU::BindGroupDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteBindGroupProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBindGroup(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteBindGroupProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::ShaderModule> RemoteDeviceProxy::createShaderModule(const PAL::WebGPU::ShaderModuleDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteShaderModuleProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateShaderModule(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteShaderModuleProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::ComputePipeline> RemoteDeviceProxy::createComputePipeline(const PAL::WebGPU::ComputePipelineDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteComputePipelineProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateComputePipeline(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteComputePipelineProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::RenderPipeline> RemoteDeviceProxy::createRenderPipeline(const PAL::WebGPU::RenderPipelineDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteRenderPipelineProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateRenderPipeline(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteRenderPipelineProxy::create(*this, m_convertToBackingContext, identifier);
}

void RemoteDeviceProxy::createComputePipelineAsync(const PAL::WebGPU::ComputePipelineDescriptor& descriptor, CompletionHandler<void(Ref<PAL::WebGPU::ComputePipeline>&&)>&& callback)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = sendSync(Messages::RemoteDevice::CreateComputePipelineAsync(*convertedDescriptor, identifier));
    if (!sendResult)
        return;

    callback(RemoteComputePipelineProxy::create(*this, m_convertToBackingContext, identifier));
}

void RemoteDeviceProxy::createRenderPipelineAsync(const PAL::WebGPU::RenderPipelineDescriptor& descriptor, CompletionHandler<void(Ref<PAL::WebGPU::RenderPipeline>&&)>&& callback)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = sendSync(Messages::RemoteDevice::CreateRenderPipelineAsync(*convertedDescriptor, identifier));
    if (!sendResult)
        return;

    callback(RemoteRenderPipelineProxy::create(*this, m_convertToBackingContext, identifier));
}

Ref<PAL::WebGPU::CommandEncoder> RemoteDeviceProxy::createCommandEncoder(const std::optional<PAL::WebGPU::CommandEncoderDescriptor>& descriptor)
{
    std::optional<CommandEncoderDescriptor> convertedDescriptor;
    if (descriptor) {
        convertedDescriptor = m_convertToBackingContext->convertToBacking(*descriptor);
        if (!convertedDescriptor) {
            // FIXME: Implement error handling.
            return RemoteCommandEncoderProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
        }
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateCommandEncoder(WTFMove(convertedDescriptor), identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteCommandEncoderProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::RenderBundleEncoder> RemoteDeviceProxy::createRenderBundleEncoder(const PAL::WebGPU::RenderBundleEncoderDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteRenderBundleEncoderProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateRenderBundleEncoder(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteRenderBundleEncoderProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::QuerySet> RemoteDeviceProxy::createQuerySet(const PAL::WebGPU::QuerySetDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteQuerySetProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateQuerySet(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteQuerySetProxy::create(*this, m_convertToBackingContext, identifier);
}

void RemoteDeviceProxy::pushErrorScope(PAL::WebGPU::ErrorFilter errorFilter)
{
    auto sendResult = send(Messages::RemoteDevice::PushErrorScope(errorFilter));
    UNUSED_VARIABLE(sendResult);
}

void RemoteDeviceProxy::popErrorScope(CompletionHandler<void(std::optional<PAL::WebGPU::Error>&&)>&& callback)
{
    auto sendResult = sendSync(Messages::RemoteDevice::PopErrorScope());
    auto [error] = sendResult.takeReplyOr(std::nullopt);

    if (!error) {
        callback(std::nullopt);
        return;
    }

    WTF::switchOn(WTFMove(*error), [&] (OutOfMemoryError&& outOfMemoryError) {
        callback({ PAL::WebGPU::OutOfMemoryError::create() });
    }, [&] (ValidationError&& validationError) {
        callback({ PAL::WebGPU::ValidationError::create(WTFMove(validationError.message)) });
    });
}

void RemoteDeviceProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteDevice::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
