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
#include "RemoteGPU.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "RemoteAdapter.h"
#include "RemoteGPUMessages.h"
#include "RemoteGPUProxyMessages.h"
#include "RemoteRenderingBackend.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <pal/graphics/WebGPU/WebGPU.h>
#include <pal/graphics/WebGPU/WebGPUAdapter.h>

#if HAVE(WEBGPU_IMPLEMENTATION)
#import <pal/graphics/WebGPU/Impl/WebGPUCreateImpl.h>
#endif

namespace WebKit {

RemoteGPU::RemoteGPU(WebGPUIdentifier identifier, GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackend& renderingBackend, IPC::StreamServerConnection::Handle&& connectionHandle)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_workQueue(IPC::StreamConnectionWorkQueue::create("WebGPU work queue"))
    , m_streamConnection(IPC::StreamServerConnection::create(WTFMove(connectionHandle), workQueue()))
    , m_objectHeap(WebGPU::ObjectHeap::create())
    , m_identifier(identifier)
    , m_renderingBackend(renderingBackend)
    , m_webProcessIdentifier(gpuConnectionToWebProcess.webProcessIdentifier())
{
}

RemoteGPU::~RemoteGPU() = default;

void RemoteGPU::initialize()
{
    assertIsMainRunLoop();
    m_streamConnection->open();
    workQueue().dispatch([protectedThis = Ref { *this }]() mutable {
        protectedThis->workQueueInitialize();
    });
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteGPU::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteGPU::stopListeningForIPC(Ref<RemoteGPU>&& refFromConnection)
{
    assertIsMainRunLoop();
    m_streamConnection->invalidate();
    m_streamConnection->stopReceivingMessages(Messages::RemoteGPU::messageReceiverName(), m_identifier.toUInt64());
    workQueue().dispatch([protectedThis = WTFMove(refFromConnection)]() {
        protectedThis->workQueueUninitialize();
    });
}

void RemoteGPU::workQueueInitialize()
{
    assertIsCurrent(workQueue());
#if HAVE(WEBGPU_IMPLEMENTATION)
    // BEWARE: This is a retain cycle.
    // this owns m_backing, but m_backing contains a callback which has a stong reference to this.
    // The retain cycle is required because callbacks need to execute even if this is disowned
    // (because the callbacks handle resource cleanup, etc.).
    // The retain cycle is broken in workQueueUninitialize().
    auto backing = PAL::WebGPU::create([protectedThis = Ref { *this }](PAL::WebGPU::WorkItem&& workItem) {
        protectedThis->workQueue().dispatch(WTFMove(workItem));
    });
#else
    RefPtr<PAL::WebGPU::GPU> backing;
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
    m_streamConnection = nullptr;
    m_objectHeap->clear();
    m_backing = nullptr;
}

void RemoteGPU::requestAdapter(const WebGPU::RequestAdapterOptions& options, WebGPUIdentifier identifier, CompletionHandler<void(std::optional<RequestAdapterResponse>&&)>&& callback)
{
    assertIsCurrent(workQueue());
    ASSERT(m_backing);

    auto convertedOptions = m_objectHeap->convertFromBacking(options);
    ASSERT(convertedOptions);
    if (!convertedOptions) {
        callback(std::nullopt);
        return;
    }

    m_backing->requestAdapter(*convertedOptions, [callback = WTFMove(callback), objectHeap = m_objectHeap.copyRef(), streamConnection = Ref { *m_streamConnection }, identifier] (RefPtr<PAL::WebGPU::Adapter>&& adapter) mutable {
        if (!adapter) {
            callback(std::nullopt);
            return;
        }

        auto remoteAdapter = RemoteAdapter::create(*adapter, objectHeap, WTFMove(streamConnection), identifier);
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
            limits.maxVertexAttributes(),
            limits.maxVertexBufferArrayStride(),
            limits.maxInterStageShaderComponents(),
            limits.maxComputeWorkgroupStorageSize(),
            limits.maxComputeInvocationsPerWorkgroup(),
            limits.maxComputeWorkgroupSizeX(),
            limits.maxComputeWorkgroupSizeY(),
            limits.maxComputeWorkgroupSizeZ(),
            limits.maxComputeWorkgroupsPerDimension(),
        }, adapter->isFallbackAdapter() } });
    });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
