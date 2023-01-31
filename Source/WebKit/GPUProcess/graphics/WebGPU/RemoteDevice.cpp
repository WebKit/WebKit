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
#include "RemoteDevice.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteBindGroup.h"
#include "RemoteBindGroupLayout.h"
#include "RemoteBuffer.h"
#include "RemoteCommandEncoder.h"
#include "RemoteComputePipeline.h"
#include "RemoteDeviceMessages.h"
#include "RemoteExternalTexture.h"
#include "RemotePipelineLayout.h"
#include "RemoteQuerySet.h"
#include "RemoteQueue.h"
#include "RemoteRenderBundleEncoder.h"
#include "RemoteRenderPipeline.h"
#include "RemoteSampler.h"
#include "RemoteShaderModule.h"
#include "RemoteTexture.h"
#include "StreamServerConnection.h"
#include "WebGPUCommandEncoderDescriptor.h"
#include "WebGPUObjectHeap.h"
#include "WebGPUOutOfMemoryError.h"
#include "WebGPUValidationError.h"
#include <pal/graphics/WebGPU/WebGPUBindGroup.h>
#include <pal/graphics/WebGPU/WebGPUBindGroupDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUBindGroupLayout.h>
#include <pal/graphics/WebGPU/WebGPUBindGroupLayoutDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUBuffer.h>
#include <pal/graphics/WebGPU/WebGPUBufferDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUCommandEncoder.h>
#include <pal/graphics/WebGPU/WebGPUCommandEncoderDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUComputePipeline.h>
#include <pal/graphics/WebGPU/WebGPUComputePipelineDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUDevice.h>
#include <pal/graphics/WebGPU/WebGPUExternalTexture.h>
#include <pal/graphics/WebGPU/WebGPUExternalTextureDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUPipelineLayout.h>
#include <pal/graphics/WebGPU/WebGPUPipelineLayoutDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUQuerySet.h>
#include <pal/graphics/WebGPU/WebGPUQuerySetDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUQueue.h>
#include <pal/graphics/WebGPU/WebGPURenderBundleEncoder.h>
#include <pal/graphics/WebGPU/WebGPURenderBundleEncoderDescriptor.h>
#include <pal/graphics/WebGPU/WebGPURenderPipeline.h>
#include <pal/graphics/WebGPU/WebGPURenderPipelineDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUSampler.h>
#include <pal/graphics/WebGPU/WebGPUSamplerDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUShaderModule.h>
#include <pal/graphics/WebGPU/WebGPUShaderModuleDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUTexture.h>
#include <pal/graphics/WebGPU/WebGPUTextureDescriptor.h>

namespace WebKit {

RemoteDevice::RemoteDevice(PAL::WebGPU::Device& device, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier)
    : m_backing(device)
    , m_objectHeap(objectHeap)
    , m_streamConnection(streamConnection.copyRef())
    , m_identifier(identifier)
    , m_queue(RemoteQueue::create(device.queue(), objectHeap, WTFMove(streamConnection), queueIdentifier))
{
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteDevice::messageReceiverName(), m_identifier.toUInt64());
}

RemoteDevice::~RemoteDevice() = default;

void RemoteDevice::stopListeningForIPC()
{
    m_streamConnection->stopReceivingMessages(Messages::RemoteDevice::messageReceiverName(), m_identifier.toUInt64());
}

Ref<RemoteQueue> RemoteDevice::queue()
{
    return m_queue;
}

void RemoteDevice::destroy()
{
    m_backing->destroy();
}

void RemoteDevice::createBuffer(const WebGPU::BufferDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto buffer = m_backing->createBuffer(*convertedDescriptor);
    auto remoteBuffer = RemoteBuffer::create(buffer, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteBuffer);
}

void RemoteDevice::createTexture(const WebGPU::TextureDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto texture = m_backing->createTexture(*convertedDescriptor);
    auto remoteTexture = RemoteTexture::create(texture, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteTexture);
}

void RemoteDevice::createSurfaceTexture(WebGPUIdentifier presentationContextIdentifier, const WebGPU::TextureDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto presentationContext = m_objectHeap.convertPresentationContextFromBacking(presentationContextIdentifier);
    ASSERT(presentationContext);
    auto texture = m_backing->createSurfaceTexture(*convertedDescriptor, *presentationContext);
    auto remoteTexture = RemoteTexture::create(texture, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteTexture);
}

void RemoteDevice::createSampler(const WebGPU::SamplerDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto sampler = m_backing->createSampler(*convertedDescriptor);
    auto remoteSampler = RemoteSampler::create(sampler, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteSampler);
}

void RemoteDevice::importExternalTexture(const WebGPU::ExternalTextureDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto externalTexture = m_backing->importExternalTexture(*convertedDescriptor);
    auto remoteExternalTexture = RemoteExternalTexture::create(externalTexture, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteExternalTexture);
}

void RemoteDevice::createBindGroupLayout(const WebGPU::BindGroupLayoutDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto bindGroupLayout = m_backing->createBindGroupLayout(*convertedDescriptor);
    auto remoteBindGroupLayout = RemoteBindGroupLayout::create(bindGroupLayout, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteBindGroupLayout);
}

void RemoteDevice::createPipelineLayout(const WebGPU::PipelineLayoutDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto pipelineLayout = m_backing->createPipelineLayout(*convertedDescriptor);
    auto remotePipelineLayout = RemotePipelineLayout::create(pipelineLayout, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remotePipelineLayout);
}

void RemoteDevice::createBindGroup(const WebGPU::BindGroupDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto bindGroup = m_backing->createBindGroup(*convertedDescriptor);
    auto remoteBindGroup = RemoteBindGroup::create(bindGroup, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteBindGroup);
}

void RemoteDevice::createShaderModule(const WebGPU::ShaderModuleDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto shaderModule = m_backing->createShaderModule(*convertedDescriptor);
    auto remoteShaderModule = RemoteShaderModule::create(shaderModule, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteShaderModule);
}

void RemoteDevice::createComputePipeline(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto computePipeline = m_backing->createComputePipeline(*convertedDescriptor);
    auto remoteComputePipeline = RemoteComputePipeline::create(computePipeline, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteComputePipeline);
}

void RemoteDevice::createRenderPipeline(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto renderPipeline = m_backing->createRenderPipeline(*convertedDescriptor);
    auto remoteRenderPipeline = RemoteRenderPipeline::create(renderPipeline, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteRenderPipeline);
}

void RemoteDevice::createComputePipelineAsync(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUIdentifier identifier, CompletionHandler<void()>&& callback)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        callback();
        return;
    }

    m_backing->createComputePipelineAsync(*convertedDescriptor, [callback = WTFMove(callback), objectHeap = Ref { m_objectHeap }, streamConnection = m_streamConnection.copyRef(), identifier] (Ref<PAL::WebGPU::ComputePipeline>&& computePipeline) mutable {
        auto remoteComputePipeline = RemoteComputePipeline::create(computePipeline, objectHeap, WTFMove(streamConnection), identifier);
        objectHeap->addObject(identifier, remoteComputePipeline);
        callback();
    });
}

void RemoteDevice::createRenderPipelineAsync(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPUIdentifier identifier, CompletionHandler<void()>&& callback)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        callback();
        return;
    }

    m_backing->createRenderPipelineAsync(*convertedDescriptor, [callback = WTFMove(callback), objectHeap = Ref { m_objectHeap }, streamConnection = m_streamConnection.copyRef(), identifier] (Ref<PAL::WebGPU::RenderPipeline>&& renderPipeline) mutable {
        auto remoteRenderPipeline = RemoteRenderPipeline::create(renderPipeline, objectHeap, WTFMove(streamConnection), identifier);
        objectHeap->addObject(identifier, remoteRenderPipeline);
        callback();
    });
}

void RemoteDevice::createCommandEncoder(const std::optional<WebGPU::CommandEncoderDescriptor>& descriptor, WebGPUIdentifier identifier)
{
    std::optional<PAL::WebGPU::CommandEncoderDescriptor> convertedDescriptor;
    if (descriptor) {
        auto resultDescriptor = m_objectHeap.convertFromBacking(*descriptor);
        ASSERT(resultDescriptor);
        convertedDescriptor = WTFMove(resultDescriptor);
        if (!convertedDescriptor)
            return;
    }
    auto commandEncoder = m_backing->createCommandEncoder(*convertedDescriptor);
    auto remoteCommandEncoder = RemoteCommandEncoder::create(commandEncoder, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteCommandEncoder);
}

void RemoteDevice::createRenderBundleEncoder(const WebGPU::RenderBundleEncoderDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto renderBundleEncoder = m_backing->createRenderBundleEncoder(*convertedDescriptor);
    auto remoteRenderBundleEncoder = RemoteRenderBundleEncoder::create(renderBundleEncoder, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteRenderBundleEncoder);
}

void RemoteDevice::createQuerySet(const WebGPU::QuerySetDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto querySet = m_backing->createQuerySet(*convertedDescriptor);
    auto remoteQuerySet = RemoteQuerySet::create(querySet, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteQuerySet);
}

void RemoteDevice::pushErrorScope(PAL::WebGPU::ErrorFilter errorFilter)
{
    m_backing->pushErrorScope(errorFilter);
}

void RemoteDevice::popErrorScope(CompletionHandler<void(std::optional<WebGPU::Error>&&)>&& callback)
{
    m_backing->popErrorScope([callback = WTFMove(callback)] (std::optional<PAL::WebGPU::Error>&& error) mutable {
        if (!error) {
            callback(std::nullopt);
            return;
        }

        WTF::switchOn(*error, [&] (Ref<PAL::WebGPU::OutOfMemoryError> outOfMemoryError) {
            callback({ WebGPU::OutOfMemoryError { } });
        }, [&] (Ref<PAL::WebGPU::ValidationError> validationError) {
            callback({ WebGPU::ValidationError { validationError->message() } });
        });
    });
}

void RemoteDevice::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
