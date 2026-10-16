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
#include "RemoteXRBinding.h"
#include "StreamServerConnection.h"
#include "WebGPUCommandEncoderDescriptor.h"
#include "WebGPUObjectHeap.h"
#include "WebGPUOutOfMemoryError.h"
#include "WebGPUValidationError.h"
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

static std::optional<WebGPU::Error> convertToBacking(std::optional<::WebGPU::Error>&& error)
{
    if (!error)
        return std::nullopt;

    switch (error->type) {
    case ::WebGPU::ErrorType::OutOfMemory:
        return { WebGPU::OutOfMemoryError { } };
    case ::WebGPU::ErrorType::Validation:
        return { WebGPU::ValidationError { WTF::move(error->message) } };
    case ::WebGPU::ErrorType::Internal:
        return { WebGPU::InternalError { WTF::move(error->message) } };
    }
    RELEASE_ASSERT_NOT_REACHED();
}

RemoteDevice::RemoteDevice(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, ::WebGPU::Device& device, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier)
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
    // The device can outlive this receiver, so complete the callbacks that reply to the Web Process.
    m_backing->resolveDeviceLostPromise({ });
    m_backing->resolveUncapturedErrorEvent({ });
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

void RemoteDevice::createXRBinding(WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    RefPtr binding = m_backing->createXRBinding();
    MESSAGE_CHECK(binding);
    auto remoteBinding = RemoteXRBinding::create(*m_gpuConnectionToWebProcess.get(), *binding, objectHeap, protect(m_gpu), protect(m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteBinding);
}

void RemoteDevice::createBuffer(const WebGPU::BufferDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto buffer = m_backing->createBuffer(descriptor);
    MESSAGE_CHECK(buffer);
    auto remoteBuffer = RemoteBuffer::create(*buffer, objectHeap, protect(m_streamConnection), protect(m_gpu), descriptor.mappedAtCreation, identifier);
    objectHeap->addObject(identifier, remoteBuffer);
}

void RemoteDevice::createTexture(const WebGPU::TextureDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor);
    MESSAGE_CHECK(convertedDescriptor);

    auto texture = m_backing->createTexture(*convertedDescriptor);
    MESSAGE_CHECK(texture);
    auto remoteTexture = RemoteTexture::create(*m_gpuConnectionToWebProcess.get(), protect(m_gpu), texture.releaseNonNull(), objectHeap, protect(m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteTexture);
}

void RemoteDevice::createSampler(const WebGPU::SamplerDescriptor& descriptor, WebGPUIdentifier identifier)
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

#if PLATFORM(COCOA) && ENABLE(VIDEO)
void RemoteDevice::importExternalTextureFromVideoFrame(const WebGPU::ExternalTextureDescriptor& descriptor, WebGPUIdentifier identifier)
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
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, pixelBuffer);
    MESSAGE_CHECK(convertedDescriptor);

    RefPtr gpu = protect(m_gpu)->backing();
    if (!gpu)
        return;
    auto externalTexture = gpu->importExternalTexture(m_backing, *convertedDescriptor);
    MESSAGE_CHECK(externalTexture);
    auto remoteExternalTexture = RemoteExternalTexture::create(*externalTexture, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteExternalTexture);
}

void RemoteDevice::updateExternalTexture(WebKit::WebGPUIdentifier externalTextureIdentifier, const WebCore::MediaPlayerIdentifier& mediaPlayerIdentifier)
{
    auto externalTexture = protect(m_objectHeap)->convertExternalTextureFromBacking(externalTextureIdentifier);
    if (!externalTexture.get())
        return;

    externalTexture.get()->destroy();
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

void RemoteDevice::createBindGroupLayout(const WebGPU::BindGroupLayoutDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::BindGroupLayoutEntry> entries;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, entries);
    MESSAGE_CHECK(convertedDescriptor);

    auto bindGroupLayout = m_backing->createBindGroupLayout(*convertedDescriptor);
    MESSAGE_CHECK(bindGroupLayout);
    auto remoteBindGroupLayout = RemoteBindGroupLayout::create(*bindGroupLayout, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteBindGroupLayout);
}

void RemoteDevice::createPipelineLayout(const WebGPU::PipelineLayoutDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<Ref<::WebGPU::BindGroupLayout>> bindGroupLayouts;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, bindGroupLayouts);
    MESSAGE_CHECK(convertedDescriptor);

    auto pipelineLayout = m_backing->createPipelineLayout(*convertedDescriptor);
    MESSAGE_CHECK(pipelineLayout);
    auto remotePipelineLayout = RemotePipelineLayout::create(*pipelineLayout,  objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remotePipelineLayout);
}

void RemoteDevice::createBindGroup(const WebGPU::BindGroupDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::BindGroupEntry> entries;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, entries);
    MESSAGE_CHECK(convertedDescriptor);

    auto bindGroup = m_backing->createBindGroup(*convertedDescriptor);
    MESSAGE_CHECK(bindGroup);
    auto remoteBindGroup = RemoteBindGroup::create(*bindGroup, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteBindGroup);
}

void RemoteDevice::createShaderModule(const WebGPU::ShaderModuleDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::ShaderModuleCompilationHint> hints;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, hints);
    MESSAGE_CHECK(convertedDescriptor);

    auto shaderModule = m_backing->createShaderModule(*convertedDescriptor);
    MESSAGE_CHECK(shaderModule);
    auto remoteShaderModule = RemoteShaderModule::create(*shaderModule, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteShaderModule);

}

void RemoteDevice::createComputePipeline(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::ConstantEntry> constants;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, constants);
    MESSAGE_CHECK(convertedDescriptor);

    auto computePipeline = m_backing->createComputePipeline(*convertedDescriptor);
    MESSAGE_CHECK(computePipeline);
    auto remoteComputePipeline = RemoteComputePipeline::create(*computePipeline, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteComputePipeline);
}

void RemoteDevice::createComputePipelineWithPipelineLayoutFromPipeline(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUIdentifier identifier, WebGPUIdentifier pipelineToReplaceIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::ConstantEntry> constants;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, constants, true);
    MESSAGE_CHECK_COMPLETION(convertedDescriptor, completionHandler(false));

    RefPtr pipelineToReplace = objectHeap->convertComputePipelineFromBacking(pipelineToReplaceIdentifier);
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

void RemoteDevice::createRenderPipeline(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    WebGPU::RenderPipelineDescriptorStorage storage;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, storage);
    MESSAGE_CHECK(convertedDescriptor);

    auto renderPipeline = m_backing->createRenderPipeline(*convertedDescriptor);
    MESSAGE_CHECK(renderPipeline);
    auto remoteRenderPipeline = RemoteRenderPipeline::create(*renderPipeline, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteRenderPipeline);
}

void RemoteDevice::createRenderPipelineWithPipelineLayoutFromPipeline(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPUIdentifier identifier, WebGPUIdentifier pipelineToReplaceIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    Ref objectHeap = m_objectHeap.get();
    WebGPU::RenderPipelineDescriptorStorage storage;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, storage, true);
    MESSAGE_CHECK_COMPLETION(convertedDescriptor, completionHandler(false));

    RefPtr pipelineToReplace = objectHeap->convertRenderPipelineFromBacking(pipelineToReplaceIdentifier);
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

void RemoteDevice::createComputePipelineAsync(const WebGPU::ComputePipelineDescriptor& descriptor, WebGPUIdentifier identifier, CompletionHandler<void(bool, String&&)>&& callback)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<::WebGPU::ConstantEntry> constants;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, constants);
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

void RemoteDevice::createRenderPipelineAsync(const WebGPU::RenderPipelineDescriptor& descriptor, WebGPUIdentifier identifier, CompletionHandler<void(bool, String&&)>&& callback)
{
    Ref objectHeap = m_objectHeap.get();
    WebGPU::RenderPipelineDescriptorStorage storage;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, storage);
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

void RemoteDevice::createCommandEncoder(const std::optional<WebGPU::CommandEncoderDescriptor>& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto commandEncoder = m_backing->createCommandEncoder(descriptor.value_or(::WebGPU::CommandEncoderDescriptor { }));
    MESSAGE_CHECK(commandEncoder);
    auto remoteCommandEncoder = RemoteCommandEncoder::create(*m_gpuConnectionToWebProcess.get(), protect(m_gpu), *commandEncoder, objectHeap, protect(m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteCommandEncoder);
}

void RemoteDevice::createRenderBundleEncoder(const WebGPU::RenderBundleEncoderDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor);
    MESSAGE_CHECK(convertedDescriptor);

    auto renderBundleEncoder = m_backing->createRenderBundleEncoder(*convertedDescriptor);
    MESSAGE_CHECK(renderBundleEncoder);
    auto remoteRenderBundleEncoder = RemoteRenderBundleEncoder::create(*m_gpuConnectionToWebProcess.get(), protect(m_gpu), *renderBundleEncoder, objectHeap, protect(m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteRenderBundleEncoder);
}

void RemoteDevice::createQuerySet(const WebGPU::QuerySetDescriptor& descriptor, WebGPUIdentifier identifier)
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

void RemoteDevice::popErrorScope(CompletionHandler<void(bool, std::optional<WebGPU::Error>&&)>&& callback)
{
    m_backing->popErrorScope([callback = WTF::move(callback)](bool success, std::optional<::WebGPU::Error>&& error) mutable {
        callback(success, convertToBacking(WTF::move(error)));
    });
}

void RemoteDevice::resolveUncapturedErrorEvent(CompletionHandler<void(bool, std::optional<WebGPU::Error>&&)>&& callback)
{
    m_backing->resolveUncapturedErrorEvent([callback = WTF::move(callback)](bool hasUncapturedError, std::optional<::WebGPU::Error>&& error) mutable {
        callback(hasUncapturedError, convertToBacking(WTF::move(error)));
    });
}

void RemoteDevice::resolveDeviceLostPromise(CompletionHandler<void(::WebGPU::DeviceLostReason)>&& callback)
{
    m_backing->resolveDeviceLostPromise([callback = WTF::move(callback)](::WebGPU::DeviceLostReason reason, String&&) mutable {
        callback(reason);
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
