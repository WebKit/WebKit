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
#include "WebGPUDescriptorDeserializers.h"
#include "WebGPUDeviceDescriptor.h"
#include "WebGPUObjectHeap.h"
#include "WebGPUSupportedLimits.h"
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUDevice.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAdapter);

RemoteAdapter::RemoteAdapter(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, ::WebGPU::Adapter& adapter, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUAdapterIdentifier identifier)
    : m_backing(adapter)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTF::move(streamConnection))
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_gpu(gpu)
    , m_identifier(identifier)
{
    protect(m_streamConnection)->startReceivingMessages(*this, Messages::RemoteAdapter::messageReceiverName(), m_identifier.toUInt64());
}

RemoteAdapter::~RemoteAdapter() = default;

void RemoteAdapter::destruct()
{
    protect(m_objectHeap)->removeObject(m_identifier);
}

void RemoteAdapter::stopListeningForIPC()
{
    protect(m_streamConnection)->stopReceivingMessages(Messages::RemoteAdapter::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteAdapter::requestDevice(const WebGPU::DeviceDescriptor& descriptor, WebGPUDeviceIdentifier identifier, WebGPUQueueIdentifier queueIdentifier, CompletionHandler<void(Vector<::WebGPU::FeatureName>&&, WebGPU::SupportedLimits&&, String&&)>&& callback)
{
    Ref objectHeap = m_objectHeap;
    auto convertedDescriptor = deserialize(objectHeap, descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        callback({ }, { }, "Device descriptor is invalid"_s);
        return;
    }

    // A reply without a device has the error, and empty features and limits.
    protect(m_backing)->requestDevice(*convertedDescriptor, [callback = WTF::move(callback), objectHeap = WTF::move(objectHeap), streamConnection = protect(m_streamConnection), identifier, queueIdentifier, gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get(), gpu = protect(m_gpu)](std::expected<Ref<::WebGPU::Device>, String>&& result) mutable {
        if (!result) {
            callback({ }, { }, WTF::move(result.error()));
            return;
        }
        if (!gpuConnectionToWebProcess) {
            callback({ }, { }, "Web Process connection is closed"_s);
            return;
        }

        Ref device = WTF::move(*result);
        auto remoteDevice = RemoteDevice::create(*gpuConnectionToWebProcess, gpu, device, objectHeap, WTF::move(streamConnection), identifier, queueIdentifier);
        objectHeap->addObject(identifier, remoteDevice);
        objectHeap->addObject(queueIdentifier, remoteDevice->queue());
        callback(device->features(), ::WebGPU::Limits { device->limits() }, { });
    });
}

} // namespace WebKit

#endif // HAVE(GPU_PROCESS)
