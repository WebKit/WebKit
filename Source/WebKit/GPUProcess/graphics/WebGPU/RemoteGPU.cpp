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
#include "RemoteGPU.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "RemoteAdapter.h"
#include "RemoteCompositorIntegration.h"
#include "RemoteGPUMessages.h"
#include "RemoteGPUProxyMessages.h"
#include "RemotePresentationContext.h"
#include "RemoteRenderingBackend.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/NativeImage.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/WebGPU.h>
#include <WebCore/WebGPUAdapter.h>
#include <WebCore/WebGPUPresentationContext.h>
#include <WebCore/WebGPUPresentationContextDescriptor.h>
#include <wtf/threads/BinarySemaphore.h>

#if HAVE(WEBGPU_IMPLEMENTATION)
#include <WebCore/ProcessIdentity.h>
#include <WebCore/WebGPUCreateImpl.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_OPTIONAL_CONNECTION_BASE(assertion, connection())

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteGPU);

RemoteGPU::RemoteGPU(WebGPUIdentifier identifier, GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_sharedPreferencesForWebProcess(gpuConnectionToWebProcess.sharedPreferencesForWebProcess())
    , m_workQueue(IPC::StreamConnectionWorkQueue::create("WebGPU work queue"_s))
    , m_streamConnection(WTFMove(streamConnection))
    , m_objectHeap(WebGPU::ObjectHeap::create())
    , m_identifier(identifier)
    , m_renderingBackend(renderingBackend)
{
    weakPtrFactory().prepareForUseOnlyOnNonMainThread();
}

RemoteGPU::~RemoteGPU() = default;

RefPtr<IPC::Connection> RemoteGPU::connection() const
{
    RefPtr connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return nullptr;
    return &connection->connection();
}

void RemoteGPU::initialize()
{
    assertIsMainRunLoop();
    workQueue().dispatch([protectedThis = Ref { *this }]() mutable {
        protectedThis->workQueueInitialize();
    });
}

void RemoteGPU::stopListeningForIPC()
{
    assertIsMainRunLoop();
    workQueue().dispatch([this]() {
        workQueueUninitialize();
    });
    workQueue().stopAndWaitForCompletion();
}

void RemoteGPU::workQueueInitialize()
{
    assertIsCurrent(workQueue());
    m_streamConnection->open(workQueue());
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteGPU::messageReceiverName(), m_identifier.toUInt64());

#if HAVE(WEBGPU_IMPLEMENTATION)
    // BEWARE: This is a retain cycle.
    // this owns m_backing, but m_backing contains a callback which has a stong reference to this.
    // The retain cycle is required because callbacks need to execute even if this is disowned
    // (because the callbacks handle resource cleanup, etc.).
    // The retain cycle is broken in workQueueUninitialize().
    auto gpuProcessConnection = m_gpuConnectionToWebProcess.get();
    auto backing = WebCore::WebGPU::create([protectedThis = Ref { *this }](WebCore::WebGPU::WorkItem&& workItem) {
        protectedThis->workQueue().dispatch(WTFMove(workItem));
    }, gpuProcessConnection ? &gpuProcessConnection->webProcessIdentity() : nullptr);
#else
    RefPtr<WebCore::WebGPU::GPU> backing;
#endif
    if (backing) {
        m_backing = backing.releaseNonNull();
        send(Messages::RemoteGPUProxy::WasCreated(true, workQueue().wakeUpSemaphore(), m_streamConnection->clientWaitSemaphore()));
    } else
        send(Messages::RemoteGPUProxy::WasCreated(false, { }, { }));
}

void RemoteGPU::workQueueUninitialize()
{
    assertIsCurrent(workQueue());
    m_streamConnection->stopReceivingMessages(Messages::RemoteGPU::messageReceiverName(), m_identifier.toUInt64());
    m_streamConnection->invalidate();
    m_streamConnection = nullptr;
    m_objectHeap->clear();
    m_backing = nullptr;
}

void RemoteGPU::requestAdapter(const WebGPU::RequestAdapterOptions& options, WebGPUIdentifier identifier, CompletionHandler<void(std::optional<RemoteGPURequestAdapterResponse>&&)>&& callback)
{
    assertIsCurrent(workQueue());
    ASSERT(m_backing);

    auto convertedOptions = m_objectHeap->convertFromBacking(options);
    ASSERT(convertedOptions);
    if (!convertedOptions) {
        callback(std::nullopt);
        return;
    }

    m_backing->requestAdapter(*convertedOptions, [callback = WTFMove(callback), objectHeap = m_objectHeap.copyRef(), streamConnection = Ref { *m_streamConnection }, identifier, gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get(), gpu = Ref { *this }] (RefPtr<WebCore::WebGPU::Adapter>&& adapter) mutable {
        if (!adapter) {
            callback(std::nullopt);
            return;
        }

        auto remoteAdapter = RemoteAdapter::create(*gpuConnectionToWebProcess, gpu, *adapter, objectHeap, WTFMove(streamConnection), identifier);
        objectHeap->addObject(identifier, remoteAdapter);

        auto name = adapter->name();
        const auto& features = adapter->features();
        const auto& limits = adapter->limits();
        callback({ { WTFMove(name), WebGPU::SupportedFeatures { features.features() }, WebGPU::SupportedLimits {
            limits.maxTextureDimension1D(),
            limits.maxTextureDimension2D(),
            limits.maxTextureDimension3D(),
            limits.maxTextureArrayLayers(),
            limits.maxBindGroups(),
            limits.maxBindGroupsPlusVertexBuffers(),
            limits.maxBindingsPerBindGroup(),
            limits.maxDynamicUniformBuffersPerPipelineLayout(),
            limits.maxDynamicStorageBuffersPerPipelineLayout(),
            limits.maxSampledTexturesPerShaderStage(),
            limits.maxSamplersPerShaderStage(),
            limits.maxStorageBuffersPerShaderStage(),
            limits.maxStorageTexturesPerShaderStage(),
            limits.maxUniformBuffersPerShaderStage(),
            limits.maxUniformBufferBindingSize(),
            limits.maxStorageBufferBindingSize(),
            limits.minUniformBufferOffsetAlignment(),
            limits.minStorageBufferOffsetAlignment(),
            limits.maxVertexBuffers(),
            limits.maxBufferSize(),
            limits.maxVertexAttributes(),
            limits.maxVertexBufferArrayStride(),
            limits.maxInterStageShaderComponents(),
            limits.maxInterStageShaderVariables(),
            limits.maxColorAttachments(),
            limits.maxColorAttachmentBytesPerSample(),
            limits.maxComputeWorkgroupStorageSize(),
            limits.maxComputeInvocationsPerWorkgroup(),
            limits.maxComputeWorkgroupSizeX(),
            limits.maxComputeWorkgroupSizeY(),
            limits.maxComputeWorkgroupSizeZ(),
            limits.maxComputeWorkgroupsPerDimension(),
        }, adapter->isFallbackAdapter() } });
    });
}

void RemoteGPU::createPresentationContext(const WebGPU::PresentationContextDescriptor& descriptor, WebGPUIdentifier identifier)
{
    assertIsCurrent(workQueue());
    ASSERT(m_backing);

    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    MESSAGE_CHECK(convertedDescriptor);

    auto presentationContext = m_backing->createPresentationContext(*convertedDescriptor);
    MESSAGE_CHECK(presentationContext);
    auto remotePresentationContext = RemotePresentationContext::create(*m_gpuConnectionToWebProcess.get(), *this, *presentationContext, m_objectHeap, *m_streamConnection, identifier);
    m_objectHeap->addObject(identifier, remotePresentationContext);
}

RefPtr<GPUConnectionToWebProcess> RemoteGPU::gpuConnectionToWebProcess() const
{
    return m_gpuConnectionToWebProcess.get();
}

void RemoteGPU::createCompositorIntegration(WebGPUIdentifier identifier)
{
    assertIsCurrent(workQueue());
    ASSERT(m_backing);

    auto compositorIntegration = m_backing->createCompositorIntegration();
    MESSAGE_CHECK(compositorIntegration);
    auto remoteCompositorIntegration = RemoteCompositorIntegration::create(*compositorIntegration, m_objectHeap, *m_streamConnection, *this, identifier);
    m_objectHeap->addObject(identifier, remoteCompositorIntegration);
}

void RemoteGPU::paintNativeImageToImageBuffer(WebCore::NativeImage& nativeImage, WebCore::RenderingResourceIdentifier imageBufferIdentifier)
{
    assertIsCurrent(workQueue());
    BinarySemaphore semaphore;

    auto* gpu = m_backing.get();
    m_renderingBackend->dispatch([&]() mutable {
        if (auto imageBuffer = m_renderingBackend->imageBuffer(imageBufferIdentifier)) {
            gpu->paintToCanvas(nativeImage, imageBuffer->backendSize(), imageBuffer->context());
            imageBuffer->flushDrawingContext();
        }
        semaphore.signal();
    });
    semaphore.wait();
}

void RemoteGPU::isValid(WebGPUIdentifier identifier, CompletionHandler<void(bool, bool)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    auto* gpu = static_cast<WebCore::WebGPU::GPU*>(m_backing.get());
    if (!gpu) {
        completionHandler(false, false);
        return;
    }

    auto result = m_objectHeap->objectExistsAndValid(*gpu, identifier);
    completionHandler(result.valid, result.exists);
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
