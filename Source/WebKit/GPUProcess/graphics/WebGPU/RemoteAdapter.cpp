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
#include "RemoteAdapter.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapterMessages.h"
#include "RemoteDevice.h"
#include "RemoteQueue.h"
#include "StreamServerConnection.h"
#include "WebGPUDeviceDescriptor.h"
#include "WebGPUObjectHeap.h"
#include "WebGPUSupportedFeatures.h"
#include "WebGPUSupportedLimits.h"
#include <WebCore/WebGPUAdapter.h>
#include <WebCore/WebGPUDevice.h>

namespace WebKit {

RemoteAdapter::RemoteAdapter(GPUConnectionToWebProcess& gpuConnectionToWebProcess, WebCore::WebGPU::Adapter& adapter, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(adapter)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_identifier(identifier)
{
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteAdapter::messageReceiverName(), m_identifier.toUInt64());
}

RemoteAdapter::~RemoteAdapter() = default;

void RemoteAdapter::destruct()
{
    m_objectHeap.removeObject(m_identifier);
}

void RemoteAdapter::stopListeningForIPC()
{
    m_streamConnection->stopReceivingMessages(Messages::RemoteAdapter::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteAdapter::requestDevice(const WebGPU::DeviceDescriptor& descriptor, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier, CompletionHandler<void(WebGPU::SupportedFeatures&&, WebGPU::SupportedLimits&&)>&& callback)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        callback({ { } }, { });
        return;
    }

    m_backing->requestDevice(*convertedDescriptor, [callback = WTFMove(callback), objectHeap = Ref { m_objectHeap }, streamConnection = m_streamConnection.copyRef(), identifier, queueIdentifier, &gpuConnectionToWebProcess = m_gpuConnectionToWebProcess] (RefPtr<WebCore::WebGPU::Device>&& devicePtr) mutable {
        if (!devicePtr.get()) {
            callback({ }, { });
            return;
        }

        auto device = devicePtr.releaseNonNull();
        auto remoteDevice = RemoteDevice::create(gpuConnectionToWebProcess, device, objectHeap, WTFMove(streamConnection), identifier, queueIdentifier);
        objectHeap->addObject(identifier, remoteDevice);
        objectHeap->addObject(queueIdentifier, remoteDevice->queue());
        const auto& features = device->features();
        const auto& limits = device->limits();
        callback(WebGPU::SupportedFeatures { features.features() }, WebGPU::SupportedLimits {
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
        });
    });
}

} // namespace WebKit

#endif // HAVE(GPU_PROCESS)
