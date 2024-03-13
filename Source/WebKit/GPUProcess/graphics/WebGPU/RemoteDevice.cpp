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
#include "RemoteGPU.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemotePipelineLayout.h"
#include "RemoteQuerySet.h"
#include "RemoteQueue.h"
#include "RemoteRenderBundleEncoder.h"
#include "RemoteRenderPipeline.h"
#include "RemoteRenderingBackend.h"
#include "RemoteSampler.h"
#include "RemoteShaderModule.h"
#include "RemoteTexture.h"
#include "RemoteVideoFrameIdentifier.h"
#include "StreamServerConnection.h"
#include "WebGPUCommandEncoderDescriptor.h"
#include "WebGPUObjectHeap.h"
#include "WebGPUOutOfMemoryError.h"
#include "WebGPUValidationError.h"
#include <WebCore/VideoFrame.h>
#include <WebCore/WebGPUBindGroup.h>
#include <WebCore/WebGPUBindGroupDescriptor.h>
#include <WebCore/WebGPUBindGroupLayout.h>
#include <WebCore/WebGPUBindGroupLayoutDescriptor.h>
#include <WebCore/WebGPUBuffer.h>
#include <WebCore/WebGPUBufferDescriptor.h>
#include <WebCore/WebGPUCommandEncoder.h>
#include <WebCore/WebGPUCommandEncoderDescriptor.h>
#include <WebCore/WebGPUComputePipeline.h>
#include <WebCore/WebGPUComputePipelineDescriptor.h>
#include <WebCore/WebGPUDevice.h>
#include <WebCore/WebGPUExternalTexture.h>
#include <WebCore/WebGPUExternalTextureDescriptor.h>
#include <WebCore/WebGPUPipelineLayout.h>
#include <WebCore/WebGPUPipelineLayoutDescriptor.h>
#include <WebCore/WebGPUQuerySet.h>
#include <WebCore/WebGPUQuerySetDescriptor.h>
#include <WebCore/WebGPUQueue.h>
#include <WebCore/WebGPURenderBundleEncoder.h>
#include <WebCore/WebGPURenderBundleEncoderDescriptor.h>
#include <WebCore/WebGPURenderPipeline.h>
#include <WebCore/WebGPURenderPipelineDescriptor.h>
#include <WebCore/WebGPUSampler.h>
#include <WebCore/WebGPUSamplerDescriptor.h>
#include <WebCore/WebGPUShaderModule.h>
#include <WebCore/WebGPUShaderModuleDescriptor.h>
#include <WebCore/WebGPUTexture.h>
#include <WebCore/WebGPUTextureDescriptor.h>

namespace WebKit {

RemoteDevice::RemoteDevice(GPUConnectionToWebProcess& gpuConnectionToWebProcess, WebCore::WebGPU::Device& device, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier)
    : m_backing(device)
    , m_objectHeap(objectHeap)
    , m_streamConnection(streamConnection.copyRef())
    , m_identifier(identifier)
    , m_queue(RemoteQueue::create(device.queue(), objectHeap, WTFMove(streamConnection), queueIdentifier))
#if ENABLE(VIDEO)
    , m_videoFrameObjectHeap(gpuConnectionToWebProcess.videoFrameObjectHeap())
#if PLATFORM(COCOA)
    , m_sharedVideoFrameReader(m_videoFrameObjectHeap.ptr(), gpuConnectionToWebProcess.webProcessIdentity())
#endif
#endif
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
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

void RemoteDevice::destruct()
{
    m_objectHeap->removeObject(m_identifier);
}

void RemoteDevice::createBuffer(const WebGPU::BufferDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    RELEASE_ASSERT(convertedDescriptor);

    auto buffer = m_backing->createBuffer(*convertedDescriptor);
    RELEASE_ASSERT(buffer);
    auto remoteBuffer = RemoteBuffer::create(*buffer, m_objectHeap, m_streamConnection.copyRef(), descriptor.mappedAtCreation, identifier);
    m_objectHeap->addObject(identifier, remoteBuffer);
}

void RemoteDevice::createTexture(const WebGPU::TextureDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    RELEASE_ASSERT(convertedDescriptor);

    auto texture = m_backing->createTexture(*convertedDescriptor);
    RELEASE_ASSERT(texture);
    auto remoteTexture = RemoteTexture::create(texture.releaseNonNull(), m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remoteTexture);
}

void RemoteDevice::createSampler(const WebGPU::SamplerDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    RELEASE_ASSERT(convertedDescriptor);

    auto sampler = m_backing->createSampler(*convertedDescriptor);
    RELEASE_ASSERT(sampler);
    auto remoteSampler = RemoteSampler::create(*sampler, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remoteSampler);
}

#if ENABLE(VIDEO) && PLATFORM(COCOA)
void RemoteDevice::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTFMove(semaphore));
}

void RemoteDevice::setSharedVideoFrameMemory(WebCore::SharedMemory::Handle&& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(WTFMove(handle));
}
#endif

#if PLATFORM(COCOA) && ENABLE(VIDEO)
void RemoteDevice::importExternalTextureFromVideoFrame(const WebGPU::ExternalTextureDescriptor& descriptor, WebGPUIdentifier identifier)
{
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    std::optional<WebKit::SharedVideoFrame> sharedVideoFrame = descriptor.sharedFrame;
    RetainPtr<CVPixelBufferRef> pixelBuffer { nullptr };
    if (sharedVideoFrame) {
        if (auto videoFrame = m_sharedVideoFrameReader.read(WTFMove(*sharedVideoFrame)))
            pixelBuffer = videoFrame->pixelBuffer();
    } else if (descriptor.mediaIdentifier) {
        m_gpuConnectionToWebProcess->performWithMediaPlayerOnMainThread(*descriptor.mediaIdentifier, [&] (auto& player) mutable {
            auto videoFrame = player.videoFrameForCurrentTime();
            pixelBuffer = videoFrame ? videoFrame->pixelBuffer() : nullptr;
        });
    }

    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor, pixelBuffer);
    RELEASE_ASSERT(convertedDescriptor);

    auto externalTexture = m_backing->importExternalTexture(*convertedDescriptor);
    RELEASE_ASSERT(externalTexture);
    auto remoteExternalTexture = RemoteExternalTexture::create(*externalTexture, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remoteExternalTexture);
#else
    UNUSED_PARAM(identifier);
#endif
}
#endif // PLATFORM(COCOA) && ENABLE(VIDEO)

void RemoteDevice::createBindGroupLayout(const WebGPU::BindGroupLayoutDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    RELEASE_ASSERT(convertedDescriptor);

    auto bindGroupLayout = m_backing->createBindGroupLayout(*convertedDescriptor);
    RELEASE_ASSERT(bindGroupLayout);
    auto remoteBindGroupLayout = RemoteBindGroupLayout::create(*bindGroupLayout, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remoteBindGroupLayout);
}

void RemoteDevice::createPipelineLayout(const WebGPU::PipelineLayoutDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    RELEASE_ASSERT(convertedDescriptor);

    auto pipelineLayout = m_backing->createPipelineLayout(*convertedDescriptor);
    RELEASE_ASSERT(pipelineLayout);
    auto remotePipelineLayout = RemotePipelineLayout::create(*pipelineLayout, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remotePipelineLayout);
}

void RemoteDevice::createBindGroup(const WebGPU::BindGroupDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    RELEASE_ASSERT(convertedDescriptor);

    auto bindGroup = m_backing->createBindGroup(*convertedDescriptor);
    RELEASE_ASSERT(bindGroup);
    auto remoteBindGroup = RemoteBindGroup::create(*bindGroup, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remoteBindGroup);
}

void RemoteDevice::createShaderModule(const WebGPU::ShaderModuleDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    RELEASE_ASSERT(convertedDescriptor);

    auto shaderModule = m_backing->createShaderModule(*convertedDescriptor);
    RELEASE_ASSERT(shaderModule);
    auto remoteShaderModule = RemoteShaderModule::create(*shaderModule, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remoteShaderModule);

}

void RemoteDevice::createComputePipeline(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    RELEASE_ASSERT(convertedDescriptor);

    auto computePipeline = m_backing->createComputePipeline(*convertedDescriptor);
    RELEASE_ASSERT(computePipeline);
    auto remoteComputePipeline = RemoteComputePipeline::create(*computePipeline, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remoteComputePipeline);
}

void RemoteDevice::createRenderPipeline(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    RELEASE_ASSERT(convertedDescriptor);

    auto renderPipeline = m_backing->createRenderPipeline(*convertedDescriptor);
    RELEASE_ASSERT(renderPipeline);
    auto remoteRenderPipeline = RemoteRenderPipeline::create(*renderPipeline, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remoteRenderPipeline);
}

void RemoteDevice::createComputePipelineAsync(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUIdentifier identifier, CompletionHandler<void(bool)>&& callback)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        callback(false);
        return;
    }

    m_backing->createComputePipelineAsync(*convertedDescriptor, [callback = WTFMove(callback), objectHeap = Ref { m_objectHeap.get() }, streamConnection = m_streamConnection.copyRef(), identifier] (RefPtr<WebCore::WebGPU::ComputePipeline>&& computePipeline) mutable {
        bool result = computePipeline.get();
        if (result) {
            auto remoteComputePipeline = RemoteComputePipeline::create(computePipeline.releaseNonNull(), objectHeap, WTFMove(streamConnection), identifier);
            objectHeap->addObject(identifier, remoteComputePipeline);
        }
        callback(result);
    });
}

void RemoteDevice::createRenderPipelineAsync(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPUIdentifier identifier, CompletionHandler<void(bool)>&& callback)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        callback(false);
        return;
    }

    m_backing->createRenderPipelineAsync(*convertedDescriptor, [callback = WTFMove(callback), objectHeap = Ref { m_objectHeap.get() }, streamConnection = m_streamConnection.copyRef(), identifier] (RefPtr<WebCore::WebGPU::RenderPipeline>&& renderPipeline) mutable {
        bool result = renderPipeline.get();
        if (result) {
            auto remoteRenderPipeline = RemoteRenderPipeline::create(renderPipeline.releaseNonNull(), objectHeap, WTFMove(streamConnection), identifier);
            objectHeap->addObject(identifier, remoteRenderPipeline);
        }
        callback(result);
    });
}

void RemoteDevice::createCommandEncoder(const std::optional<WebGPU::CommandEncoderDescriptor>& descriptor, WebGPUIdentifier identifier)
{
    std::optional<WebCore::WebGPU::CommandEncoderDescriptor> convertedDescriptor;
    if (descriptor) {
        auto resultDescriptor = m_objectHeap->convertFromBacking(*descriptor);
        RELEASE_ASSERT(resultDescriptor);
        convertedDescriptor = WTFMove(resultDescriptor);
    }
    auto commandEncoder = m_backing->createCommandEncoder(convertedDescriptor);
    RELEASE_ASSERT(commandEncoder);
    auto remoteCommandEncoder = RemoteCommandEncoder::create(*commandEncoder, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remoteCommandEncoder);
}

void RemoteDevice::createRenderBundleEncoder(const WebGPU::RenderBundleEncoderDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    RELEASE_ASSERT(convertedDescriptor);

    auto renderBundleEncoder = m_backing->createRenderBundleEncoder(*convertedDescriptor);
    RELEASE_ASSERT(renderBundleEncoder);
    auto remoteRenderBundleEncoder = RemoteRenderBundleEncoder::create(*renderBundleEncoder, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remoteRenderBundleEncoder);
}

void RemoteDevice::createQuerySet(const WebGPU::QuerySetDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    RELEASE_ASSERT(convertedDescriptor);

    auto querySet = m_backing->createQuerySet(*convertedDescriptor);
    RELEASE_ASSERT(querySet);
    auto remoteQuerySet = RemoteQuerySet::create(*querySet, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap->addObject(identifier, remoteQuerySet);
}

void RemoteDevice::pushErrorScope(WebCore::WebGPU::ErrorFilter errorFilter)
{
    m_backing->pushErrorScope(errorFilter);
}

void RemoteDevice::popErrorScope(CompletionHandler<void(bool, std::optional<WebGPU::Error>&&)>&& callback)
{
    m_backing->popErrorScope([callback = WTFMove(callback)] (bool success, std::optional<WebCore::WebGPU::Error>&& error) mutable {
        if (!error) {
            callback(success, std::nullopt);
            return;
        }

        WTF::switchOn(*error, [&] (Ref<WebCore::WebGPU::OutOfMemoryError> outOfMemoryError) {
            callback(success, { WebGPU::OutOfMemoryError { } });
        }, [&] (Ref<WebCore::WebGPU::ValidationError> validationError) {
            callback(success, { WebGPU::ValidationError { validationError->message() } });
        }, [&] (Ref<WebCore::WebGPU::InternalError> internalError) {
            callback(success, { WebGPU::InternalError { internalError->message() } });
        });
    });
}

void RemoteDevice::resolveUncapturedErrorEvent(CompletionHandler<void(bool, std::optional<WebGPU::Error>&&)>&& callback)
{
    m_backing->resolveUncapturedErrorEvent([callback = WTFMove(callback)] (bool hasUncapturedError, std::optional<WebCore::WebGPU::Error>&& error) mutable {
        if (!error) {
            callback(hasUncapturedError, std::nullopt);
            return;
        }

        WTF::switchOn(*error, [&] (Ref<WebCore::WebGPU::OutOfMemoryError> outOfMemoryError) {
            callback(hasUncapturedError, { WebGPU::OutOfMemoryError { } });
        }, [&] (Ref<WebCore::WebGPU::ValidationError> validationError) {
            callback(hasUncapturedError, { WebGPU::ValidationError { validationError->message() } });
        }, [&] (Ref<WebCore::WebGPU::InternalError> internalError) {
            callback(hasUncapturedError, { WebGPU::InternalError { internalError->message() } });
        });
    });
}

void RemoteDevice::resolveDeviceLostPromise(CompletionHandler<void(WebCore::WebGPU::DeviceLostReason)>&& callback)
{
    m_backing->resolveDeviceLostPromise([callback = WTFMove(callback)] (WebCore::WebGPU::DeviceLostReason reason) mutable {
        callback(reason);
    });
}

void RemoteDevice::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
