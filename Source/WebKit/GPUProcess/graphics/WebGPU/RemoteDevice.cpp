/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include "Logging.h"
#include "RemoteBindGroup.h"
#include "RemoteBindGroupLayout.h"
#include "RemoteBuffer.h"
#include "RemoteCommandBuffer.h"
#include "RemoteCommandEncoder.h"
#include "RemoteComputePipeline.h"
#include "RemoteDeviceMessages.h"
#include "RemoteExternalTexture.h"
#include "RemoteGPU.h"
#include "RemoteGPUProxyMessages.h"
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
#include "RemoteXRBinding.h"
#include "StreamServerConnection.h"
#include "WebGPUCommandEncoderDescriptor.h"
#include "WebGPUDescriptorDeserializers.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/VideoFrame.h>
#include <WebCore/WebGPU.h>
#include <WebCore/WebGPUComputePipelineDescriptor.h>
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUDevice.h>
#include <WebCore/WebGPUExternalTextureDescriptor.h>
#include <WebCore/WebGPURenderBundleEncoderDescriptor.h>
#include <WebCore/WebGPURenderPipelineDescriptor.h>
#include <WebCore/WebGPUShaderModuleDescriptor.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_streamConnection)
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, m_streamConnection, completion)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteDevice);

RemoteDevice::RemoteDevice(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, ::WebGPU::Device& device, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUDeviceIdentifier identifier, WebGPUQueueIdentifier queueIdentifier)
    : m_backing(device)
    , m_objectHeap(objectHeap)
    , m_streamConnection(streamConnection.copyRef())
    , m_identifier(identifier)
    , m_queue(RemoteQueue::create(gpuConnectionToWebProcess, device.queue(), objectHeap, WTF::move(streamConnection), gpu, queueIdentifier))
#if ENABLE(VIDEO)
    , m_videoFrameObjectHeap(gpuConnectionToWebProcess.videoFrameObjectHeap())
#if PLATFORM(COCOA)
    , m_sharedVideoFrameReader(m_videoFrameObjectHeap.ptr(), gpuConnectionToWebProcess.webProcessIdentity())
#endif
#endif
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_gpu(gpu)
{
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteDevice::messageReceiverName(), m_identifier.toUInt64());
}

RemoteDevice::~RemoteDevice()
{
    // The device can outlive this receiver, so complete the callback that replies to the Web Process,
    // and stop sending uncaptured errors to it.
    m_backing->resolveDeviceLostPromise({ });
    m_backing->setUncapturedErrorCallback({ });
}

void RemoteDevice::stopListeningForIPC()
{
    m_streamConnection->stopReceivingMessages(Messages::RemoteDevice::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteDevice::destroy()
{
    m_backing->destroy();
}

void RemoteDevice::destruct()
{
    protect(m_objectHeap)->removeObject(m_identifier);
}

void RemoteDevice::createXRBinding(WebGPUXRBindingIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    RefPtr binding = m_backing->createXRBinding();
    MESSAGE_CHECK(binding);
    auto remoteBinding = RemoteXRBinding::create(*m_gpuConnectionToWebProcess.get(), *binding, objectHeap, protect(m_gpu), protect(m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteBinding);
}

void RemoteDevice::createBuffer(const WebGPU::BufferDescriptor& descriptor, WebGPUBufferIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto buffer = m_backing->createBuffer(descriptor);
    MESSAGE_CHECK(buffer);
    auto remoteBuffer = RemoteBuffer::create(*buffer, objectHeap, protect(m_streamConnection), protect(m_gpu), descriptor.mappedAtCreation, identifier);
    objectHeap->addObject(identifier, remoteBuffer);
}

void RemoteDevice::createTexture(const WebGPU::TextureDescriptor& descriptor, WebGPUTextureIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto convertedDescriptor = deserialize(objectHeap, descriptor);
    MESSAGE_CHECK(convertedDescriptor);

    auto texture = m_backing->createTexture(*convertedDescriptor);
    MESSAGE_CHECK(texture);
    auto remoteTexture = RemoteTexture::create(*m_gpuConnectionToWebProcess.get(), protect(m_gpu), texture.releaseNonNull(), objectHeap, protect(m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteTexture);
}

void RemoteDevice::createSampler(const WebGPU::SamplerDescriptor& descriptor, WebGPUSamplerIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto sampler = m_backing->createSampler(descriptor);
    MESSAGE_CHECK(sampler);
    auto remoteSampler = RemoteSampler::create(*sampler, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteSampler);
}

#if ENABLE(VIDEO) && PLATFORM(COCOA)
void RemoteDevice::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTF::move(semaphore));
}

void RemoteDevice::setSharedVideoFrameMemory(WebCore::SharedMemory::Handle&& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(WTF::move(handle));
}
#endif

void RemoteDevice::pauseAllErrorReporting(bool pauseErrorReporting)
{
    m_backing->pauseAllErrorReporting(pauseErrorReporting);
}

void RemoteDevice::createInvalidBindGroup(WebGPUBindGroupIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr bindGroup = m_backing->createInvalidBindGroup();
    MESSAGE_CHECK(bindGroup);
    auto remoteBindGroup = RemoteBindGroup::create(*bindGroup, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteBindGroup);
}

void RemoteDevice::createInvalidBindGroupLayout(WebGPUBindGroupLayoutIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr bindGroupLayout = m_backing->createInvalidBindGroupLayout();
    MESSAGE_CHECK(bindGroupLayout);
    auto remoteBindGroupLayout = RemoteBindGroupLayout::create(*bindGroupLayout, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteBindGroupLayout);
}

void RemoteDevice::createInvalidBuffer(WebGPUBufferIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr buffer = m_backing->createInvalidBuffer();
    MESSAGE_CHECK(buffer);
    auto remoteBuffer = RemoteBuffer::create(*buffer, objectHeap, protect(m_streamConnection), protect(m_gpu), false, identifier);
    objectHeap->addObject(identifier, remoteBuffer);
}

void RemoteDevice::createInvalidCommandBuffer(WebGPUCommandBufferIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr commandBuffer = m_backing->createInvalidCommandBuffer();
    MESSAGE_CHECK(commandBuffer);
    auto remoteCommandBuffer = RemoteCommandBuffer::create(*commandBuffer, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteCommandBuffer);
}

void RemoteDevice::createInvalidCommandEncoder(WebGPUCommandEncoderIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr commandEncoder = m_backing->createInvalidCommandEncoder();
    MESSAGE_CHECK(commandEncoder);
    auto remoteCommandEncoder = RemoteCommandEncoder::create(*m_gpuConnectionToWebProcess.get(), protect(m_gpu), *commandEncoder, objectHeap, protect(m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteCommandEncoder);
}

void RemoteDevice::createInvalidComputePipeline(WebGPUComputePipelineIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr computePipeline = m_backing->createInvalidComputePipeline();
    MESSAGE_CHECK(computePipeline);
    auto remoteComputePipeline = RemoteComputePipeline::create(*computePipeline, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteComputePipeline);
}

void RemoteDevice::createInvalidPipelineLayout(WebGPUPipelineLayoutIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr pipelineLayout = m_backing->createInvalidPipelineLayout();
    MESSAGE_CHECK(pipelineLayout);
    auto remotePipelineLayout = RemotePipelineLayout::create(*pipelineLayout, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remotePipelineLayout);
}

void RemoteDevice::createInvalidQuerySet(WebGPUQuerySetIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr querySet = m_backing->createInvalidQuerySet();
    MESSAGE_CHECK(querySet);
    auto remoteQuerySet = RemoteQuerySet::create(*querySet, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteQuerySet);
}

void RemoteDevice::createInvalidRenderBundleEncoder(String&& error, WebGPURenderBundleEncoderIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr renderBundleEncoder = m_backing->createInvalidRenderBundleEncoder(WTF::move(error));
    MESSAGE_CHECK(renderBundleEncoder);
    auto remoteRenderBundleEncoder = RemoteRenderBundleEncoder::create(*m_gpuConnectionToWebProcess.get(), protect(m_gpu), *renderBundleEncoder, objectHeap, protect(m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteRenderBundleEncoder);
}

void RemoteDevice::createInvalidRenderPipeline(WebGPURenderPipelineIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr renderPipeline = m_backing->createInvalidRenderPipeline();
    MESSAGE_CHECK(renderPipeline);
    auto remoteRenderPipeline = RemoteRenderPipeline::create(*renderPipeline, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteRenderPipeline);
}

void RemoteDevice::createInvalidSampler(WebGPUSamplerIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr sampler = m_backing->createInvalidSampler();
    MESSAGE_CHECK(sampler);
    auto remoteSampler = RemoteSampler::create(*sampler, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteSampler);
}

void RemoteDevice::createInvalidShaderModule(WebGPUShaderModuleIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr shaderModule = m_backing->createInvalidShaderModule();
    MESSAGE_CHECK(shaderModule);
    auto remoteShaderModule = RemoteShaderModule::create(*shaderModule, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteShaderModule);
}

void RemoteDevice::createInvalidTexture(WebGPUTextureIdentifier identifier)
{
    Ref objectHeap = m_objectHeap;
    RefPtr texture = m_backing->createInvalidTexture();
    MESSAGE_CHECK(texture);
    auto remoteTexture = RemoteTexture::create(*m_gpuConnectionToWebProcess.get(), protect(m_gpu), texture.releaseNonNull(), objectHeap, protect(m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteTexture);
}

void RemoteDevice::generateAValidationError(String&& message)
{
    m_backing->generateAValidationError(WTF::move(message));
}

#if PLATFORM(COCOA) && ENABLE(VIDEO)
void RemoteDevice::importExternalTextureFromVideoFrame(const WebGPU::ExternalTextureDescriptor& descriptor, WebGPUExternalTextureIdentifier identifier)
{
    std::optional<WebKit::SharedVideoFrame> sharedVideoFrame = descriptor.sharedFrame;
    RetainPtr<CVPixelBufferRef> pixelBuffer { nullptr };
    if (sharedVideoFrame) {
        if (auto videoFrame = m_sharedVideoFrameReader.read(WTF::move(*sharedVideoFrame)))
            pixelBuffer = videoFrame->pixelBuffer();
    } else if (descriptor.mediaIdentifier) {
        if (auto connection = m_gpuConnectionToWebProcess.get()) {
            connection->performWithMediaPlayerOnMainThread(*descriptor.mediaIdentifier, [&] (auto& player) mutable {
                auto videoFrame = player.videoFrameForCurrentTime();
                pixelBuffer = videoFrame ? videoFrame->pixelBuffer() : nullptr;
            });
        }
    }

    Ref objectHeap = m_objectHeap.get();
    auto convertedDescriptor = deserialize(objectHeap, descriptor, pixelBuffer);
    MESSAGE_CHECK(convertedDescriptor);

    RefPtr gpu = protect(m_gpu)->backing();
    if (!gpu)
        return;
    auto externalTexture = gpu->importExternalTexture(m_backing, *convertedDescriptor);
    MESSAGE_CHECK(externalTexture);
    auto remoteExternalTexture = RemoteExternalTexture::create(*externalTexture, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteExternalTexture);
}

void RemoteDevice::updateExternalTexture(WebKit::WebGPUExternalTextureIdentifier externalTextureIdentifier, const WebCore::MediaPlayerIdentifier& mediaPlayerIdentifier)
{
    auto externalTexture = protect(m_objectHeap)->get(externalTextureIdentifier);
    if (!externalTexture.get())
        return;

    if (auto connection = m_gpuConnectionToWebProcess.get()) {
        RetainPtr<CVPixelBufferRef> pixelBuffer { nullptr };
        connection->performWithMediaPlayerOnMainThread(mediaPlayerIdentifier, [externalTexture, &pixelBuffer] (auto& player) mutable {
            auto externalTexturePtr = externalTexture.get();
            if (!externalTexturePtr)
                return;
            auto videoFrame = player.videoFrameForCurrentTime();
            pixelBuffer = videoFrame ? videoFrame->pixelBuffer() : nullptr;
        });
        if (auto externalTexturePtr = externalTexture.get())
            externalTexturePtr->updateExternalTexture(pixelBuffer.get());
    }
}
#endif // PLATFORM(COCOA) && ENABLE(VIDEO)

void RemoteDevice::createBindGroupLayout(const WebGPU::BindGroupLayoutDescriptor& descriptor, WebGPUBindGroupLayoutIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::BindGroupLayoutEntry> entries;
    auto convertedDescriptor = deserialize(objectHeap, descriptor, entries);
    MESSAGE_CHECK(convertedDescriptor);

    auto bindGroupLayout = m_backing->createBindGroupLayout(*convertedDescriptor);
    MESSAGE_CHECK(bindGroupLayout);
    auto remoteBindGroupLayout = RemoteBindGroupLayout::create(*bindGroupLayout, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteBindGroupLayout);
}

void RemoteDevice::createPipelineLayout(const WebGPU::PipelineLayoutDescriptor& descriptor, WebGPUPipelineLayoutIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<Ref<::WebGPU::BindGroupLayout>> bindGroupLayouts;
    auto convertedDescriptor = deserialize(objectHeap, descriptor, bindGroupLayouts);
    MESSAGE_CHECK(convertedDescriptor);

    auto pipelineLayout = m_backing->createPipelineLayout(*convertedDescriptor);
    MESSAGE_CHECK(pipelineLayout);
    auto remotePipelineLayout = RemotePipelineLayout::create(*pipelineLayout,  objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remotePipelineLayout);
}

void RemoteDevice::createBindGroup(const WebGPU::BindGroupDescriptor& descriptor, WebGPUBindGroupIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::BindGroupEntry> entries;
    auto convertedDescriptor = deserialize(objectHeap, descriptor, entries);
    MESSAGE_CHECK(convertedDescriptor);

    auto bindGroup = m_backing->createBindGroup(*convertedDescriptor);
    MESSAGE_CHECK(bindGroup);
    auto remoteBindGroup = RemoteBindGroup::create(*bindGroup, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteBindGroup);
}

void RemoteDevice::createShaderModule(const WebGPU::ShaderModuleDescriptor& descriptor, WebGPUShaderModuleIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::ShaderModuleCompilationHint> hints;
    auto convertedDescriptor = deserialize(objectHeap, descriptor, hints);
    MESSAGE_CHECK(convertedDescriptor);

    auto shaderModule = m_backing->createShaderModule(*convertedDescriptor);
    MESSAGE_CHECK(shaderModule);
    auto remoteShaderModule = RemoteShaderModule::create(*shaderModule, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteShaderModule);

}

void RemoteDevice::createComputePipeline(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUComputePipelineIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::ConstantEntry> constants;
    auto convertedDescriptor = deserialize(objectHeap, descriptor, constants);
    MESSAGE_CHECK(convertedDescriptor);

    auto computePipeline = m_backing->createComputePipeline(*convertedDescriptor);
    MESSAGE_CHECK(computePipeline);
    auto remoteComputePipeline = RemoteComputePipeline::create(*computePipeline, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteComputePipeline);
}

void RemoteDevice::createComputePipelineWithPipelineLayoutFromPipeline(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUComputePipelineIdentifier identifier, WebGPUComputePipelineIdentifier pipelineToReplaceIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::ConstantEntry> constants;
    auto convertedDescriptor = deserialize(objectHeap, descriptor, constants, true);
    MESSAGE_CHECK_COMPLETION(convertedDescriptor, completionHandler(false));

    RefPtr pipelineToReplace = objectHeap->get(pipelineToReplaceIdentifier);
    MESSAGE_CHECK_COMPLETION(pipelineToReplace, completionHandler(false));

    m_backing->createComputePipelineWithPipelineLayoutFromPipelineAsync(*convertedDescriptor, protect(*pipelineToReplace), [completionHandler = WTF::move(completionHandler), objectHeap, streamConnection = protect(m_streamConnection), gpu = protect(m_gpu), identifier](std::expected<Ref<::WebGPU::ComputePipeline>, ::WebGPU::PipelineError>&& computePipeline) mutable {
        if (!computePipeline) {
            completionHandler(false);
            return;
        }

        auto remoteComputePipeline = RemoteComputePipeline::create(WTF::move(*computePipeline), objectHeap, WTF::move(streamConnection), gpu, identifier);
        objectHeap->addObject(identifier, remoteComputePipeline);
        completionHandler(true);
    });
}

void RemoteDevice::createRenderPipeline(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPURenderPipelineIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    WebGPU::RenderPipelineDescriptorStorage storage;
    auto convertedDescriptor = deserialize(objectHeap, descriptor, storage);
    MESSAGE_CHECK(convertedDescriptor);

    auto renderPipeline = m_backing->createRenderPipeline(*convertedDescriptor);
    MESSAGE_CHECK(renderPipeline);
    auto remoteRenderPipeline = RemoteRenderPipeline::create(*renderPipeline, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteRenderPipeline);
}

void RemoteDevice::createRenderPipelineWithPipelineLayoutFromPipeline(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPURenderPipelineIdentifier identifier, WebGPURenderPipelineIdentifier pipelineToReplaceIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    Ref objectHeap = m_objectHeap.get();
    WebGPU::RenderPipelineDescriptorStorage storage;
    auto convertedDescriptor = deserialize(objectHeap, descriptor, storage, true);
    MESSAGE_CHECK_COMPLETION(convertedDescriptor, completionHandler(false));

    RefPtr pipelineToReplace = objectHeap->get(pipelineToReplaceIdentifier);
    MESSAGE_CHECK_COMPLETION(pipelineToReplace, completionHandler(false));

    m_backing->createRenderPipelineWithPipelineLayoutFromPipelineAsync(*convertedDescriptor, protect(*pipelineToReplace), [completionHandler = WTF::move(completionHandler), objectHeap, streamConnection = protect(m_streamConnection), gpu = protect(m_gpu), identifier](std::expected<Ref<::WebGPU::RenderPipeline>, ::WebGPU::PipelineError>&& renderPipeline) mutable {
        if (!renderPipeline) {
            completionHandler(false);
            return;
        }

        auto remoteRenderPipeline = RemoteRenderPipeline::create(WTF::move(*renderPipeline), objectHeap, WTF::move(streamConnection), gpu, identifier);
        objectHeap->addObject(identifier, remoteRenderPipeline);
        completionHandler(true);
    });
}

void RemoteDevice::createComputePipelineAsync(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUComputePipelineIdentifier identifier, CompletionHandler<void(bool, String&&)>&& callback)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::ConstantEntry> constants;
    auto convertedDescriptor = deserialize(objectHeap, descriptor, constants);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        callback(false, ""_s);
        return;
    }

    m_backing->createComputePipelineAsync(*convertedDescriptor, [callback = WTF::move(callback), objectHeap, streamConnection = protect(m_streamConnection), gpu = protect(m_gpu), identifier](std::expected<Ref<::WebGPU::ComputePipeline>, ::WebGPU::PipelineError>&& computePipeline) mutable {
        if (!computePipeline) {
            callback(false, WTF::move(computePipeline.error().message));
            return;
        }

        auto remoteComputePipeline = RemoteComputePipeline::create(WTF::move(*computePipeline), objectHeap, WTF::move(streamConnection), gpu, identifier);
        objectHeap->addObject(identifier, remoteComputePipeline);
        callback(true, { });
    });
}

void RemoteDevice::createRenderPipelineAsync(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPURenderPipelineIdentifier identifier, CompletionHandler<void(bool, String&&)>&& callback)
{
    Ref objectHeap = m_objectHeap.get();
    WebGPU::RenderPipelineDescriptorStorage storage;
    auto convertedDescriptor = deserialize(objectHeap, descriptor, storage);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        callback(false, ""_s);
        return;
    }

    m_backing->createRenderPipelineAsync(*convertedDescriptor, [callback = WTF::move(callback), objectHeap, streamConnection = protect(m_streamConnection), gpu = protect(m_gpu), identifier](std::expected<Ref<::WebGPU::RenderPipeline>, ::WebGPU::PipelineError>&& renderPipeline) mutable {
        if (!renderPipeline) {
            callback(false, WTF::move(renderPipeline.error().message));
            return;
        }

        auto remoteRenderPipeline = RemoteRenderPipeline::create(WTF::move(*renderPipeline), objectHeap, WTF::move(streamConnection), gpu, identifier);
        objectHeap->addObject(identifier, remoteRenderPipeline);
        callback(true, { });
    });
}

void RemoteDevice::createCommandEncoder(const std::optional<WebGPU::CommandEncoderDescriptor>& descriptor, WebGPUCommandEncoderIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto commandEncoder = m_backing->createCommandEncoder(descriptor.value_or(::WebGPU::CommandEncoderDescriptor { }));
    MESSAGE_CHECK(commandEncoder);
    auto remoteCommandEncoder = RemoteCommandEncoder::create(*m_gpuConnectionToWebProcess.get(), protect(m_gpu), *commandEncoder, objectHeap, protect(m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteCommandEncoder);
}

void RemoteDevice::createRenderBundleEncoder(const WebGPU::RenderBundleEncoderDescriptor& descriptor, WebGPURenderBundleEncoderIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto convertedDescriptor = deserialize(objectHeap, descriptor);
    MESSAGE_CHECK(convertedDescriptor);

    auto renderBundleEncoder = m_backing->createRenderBundleEncoder(*convertedDescriptor);
    MESSAGE_CHECK(renderBundleEncoder);
    auto remoteRenderBundleEncoder = RemoteRenderBundleEncoder::create(*m_gpuConnectionToWebProcess.get(), protect(m_gpu), *renderBundleEncoder, objectHeap, protect(m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteRenderBundleEncoder);
}

void RemoteDevice::createQuerySet(const WebGPU::QuerySetDescriptor& descriptor, WebGPUQuerySetIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto querySet = m_backing->createQuerySet(descriptor);
    MESSAGE_CHECK(querySet);
    auto remoteQuerySet = RemoteQuerySet::create(*querySet, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteQuerySet);
}

void RemoteDevice::pushErrorScope(::WebGPU::ErrorFilter errorFilter)
{
    m_backing->pushErrorScope(errorFilter);
}

void RemoteDevice::popErrorScope(CompletionHandler<void(bool, std::optional<::WebGPU::Error>&&)>&& callback)
{
    m_backing->popErrorScope([callback = WTF::move(callback)](bool success, std::optional<::WebGPU::Error>&& error) mutable {
        callback(success, WTF::move(error));
    });
}

// The specification lets the user agent limit the number of uncapturederror events that a device
// raises (https://www.w3.org/TR/webgpu/#abstract-opdef-dispatch-error), so a device sends at most
// this many uncaptured errors in each second, and a page that generates errors in a loop cannot
// flood the Web process with messages.
static constexpr unsigned maxUncapturedErrorsPerSecond = 1000;

void RemoteDevice::setListensForUncapturedErrors(bool listens)
{
    if (!listens) {
        m_backing->setUncapturedErrorCallback({ });
        return;
    }

    // The device generates errors while it handles the messages of the stream, on its work queue, and
    // the replies to those messages are sent from there too. Sending each error right away, on the
    // same connection, makes the Web process get it before the replies that are sent after it.
    m_backing->setUncapturedErrorCallback([streamConnection = m_streamConnection, deviceIdentifier = m_identifier, gpuIdentifier = protect(m_gpu)->identifier(), windowStart = MonotonicTime::now(), errorCountInWindow = 0u, didLogDroppedError = false](::WebGPU::Error&& error) mutable {
        auto now = MonotonicTime::now();
        if (now - windowStart >= 1_s) {
            windowStart = now;
            errorCountInWindow = 0;
        }
        if (errorCountInWindow >= maxUncapturedErrorsPerSecond) {
            if (!didLogDroppedError) {
                RELEASE_LOG_ERROR(IPC, "RemoteDevice: Dropping uncaptured errors, the device generates more than %u per second", maxUncapturedErrorsPerSecond);
                didLogDroppedError = true;
            }
            return;
        }
        ++errorCountInWindow;
        auto sendResult = streamConnection->send(Messages::RemoteGPUProxy::DidGenerateUncapturedError(deviceIdentifier, WTF::move(error)), gpuIdentifier);
        UNUSED_VARIABLE(sendResult);
    });
}

// The reply says whether the device is lost: a callback is also completed when another one replaces
// it, and then the device is not lost. The device keeps the callback, so it is held weakly. A device
// that is gone is lost.
void RemoteDevice::resolveDeviceLostPromise(CompletionHandler<void(::WebGPU::DeviceLostReason, bool isLost)>&& callback)
{
    m_backing->resolveDeviceLostPromise([weakDevice = ThreadSafeWeakPtr { m_backing.get() }, callback = WTF::move(callback)](::WebGPU::DeviceLostReason reason, String&&) mutable {
        RefPtr device = weakDevice;
        callback(reason, !device || device->isLost());
    });
}

void RemoteDevice::setLabel(String&& label)
{
    m_backing->setLabel(WTF::move(label));
}

} // namespace WebKit

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_COMPLETION

#endif // ENABLE(GPU_PROCESS)
