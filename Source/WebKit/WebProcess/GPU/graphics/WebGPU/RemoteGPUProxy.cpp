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
#include "RemoteGPUProxy.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcessConnection.h"
#include "RemoteAdapterProxy.h"
#include "RemoteGPU.h"
#include "RemoteGPUMessages.h"
#include "RemoteGPUProxyMessages.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPUSupportedFeatures.h>
#include <pal/graphics/WebGPU/WebGPUSupportedLimits.h>

namespace WebKit {

static constexpr size_t defaultStreamSize = 1 << 21;

RemoteGPUProxy::RemoteGPUProxy(GPUProcessConnection& gpuProcessConnection, WebGPU::ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, RenderingBackendIdentifier renderingBackend)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_gpuProcessConnection(&gpuProcessConnection)
{
    auto [clientConnection, serverConnectionHandle] = IPC::StreamClientConnection::create(defaultStreamSize);
    m_streamConnection = WTFMove(clientConnection);
    m_gpuProcessConnection->addClient(*this);
    m_gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::CreateRemoteGPU(identifier, renderingBackend, WTFMove(serverConnectionHandle)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    m_streamConnection->open(*this);
    // TODO: We must wait until initialized, because at the moment we cannot receive IPC messages
    // during wait while in synchronous stream send. Should be fixed as part of https://bugs.webkit.org/show_bug.cgi?id=217211.
    waitUntilInitialized();
}

RemoteGPUProxy::~RemoteGPUProxy() = default;

void RemoteGPUProxy::gpuProcessConnectionDidClose(GPUProcessConnection& connection)
{
    ASSERT(m_gpuProcessConnection);
    ASSERT(&connection == m_gpuProcessConnection);
    abandonGPUProcess();
}

void RemoteGPUProxy::abandonGPUProcess()
{
    m_streamConnection->invalidate();
    m_gpuProcessConnection = nullptr;
    m_lost = true;
}

void RemoteGPUProxy::wasCreated(bool didSucceed, IPC::Semaphore&& wakeUpSemaphore, IPC::Semaphore&& clientWaitSemaphore)
{
    ASSERT(!m_didInitialize);
    m_streamConnection->setSemaphores(WTFMove(wakeUpSemaphore), WTFMove(clientWaitSemaphore));
    m_didInitialize = true;
    m_lost = !didSucceed;
}

void RemoteGPUProxy::waitUntilInitialized()
{
    if (m_didInitialize)
        return;
    if (m_streamConnection->waitForAndDispatchImmediately<Messages::RemoteGPUProxy::WasCreated>(m_backing, defaultSendTimeout))
        return;
    m_lost = true;
}

void RemoteGPUProxy::requestAdapter(const PAL::WebGPU::RequestAdapterOptions& options, CompletionHandler<void(RefPtr<PAL::WebGPU::Adapter>&&)>&& callback)
{
    if (m_lost) {
        callback(nullptr);
        return;
    }

    auto convertedOptions = m_convertToBackingContext->convertToBacking(options);
    ASSERT(convertedOptions);
    if (!convertedOptions) {
        callback(nullptr);
        return;
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = sendSync(Messages::RemoteGPU::RequestAdapter(*convertedOptions, identifier));
    if (!sendResult) {
        m_lost = true;
        callback(nullptr);
        return;
    }
    auto [response] = sendResult.takeReply();
    if (!response) {
        callback(nullptr);
        return;
    }

    auto resultSupportedFeatures = PAL::WebGPU::SupportedFeatures::create(WTFMove(response->features.features));
    auto resultSupportedLimits = PAL::WebGPU::SupportedLimits::create(
        response->limits.maxTextureDimension1D,
        response->limits.maxTextureDimension2D,
        response->limits.maxTextureDimension3D,
        response->limits.maxTextureArrayLayers,
        response->limits.maxBindGroups,
        response->limits.maxDynamicUniformBuffersPerPipelineLayout,
        response->limits.maxDynamicStorageBuffersPerPipelineLayout,
        response->limits.maxSampledTexturesPerShaderStage,
        response->limits.maxSamplersPerShaderStage,
        response->limits.maxStorageBuffersPerShaderStage,
        response->limits.maxStorageTexturesPerShaderStage,
        response->limits.maxUniformBuffersPerShaderStage,
        response->limits.maxUniformBufferBindingSize,
        response->limits.maxStorageBufferBindingSize,
        response->limits.minUniformBufferOffsetAlignment,
        response->limits.minStorageBufferOffsetAlignment,
        response->limits.maxVertexBuffers,
        response->limits.maxVertexAttributes,
        response->limits.maxVertexBufferArrayStride,
        response->limits.maxInterStageShaderComponents,
        response->limits.maxComputeWorkgroupStorageSize,
        response->limits.maxComputeInvocationsPerWorkgroup,
        response->limits.maxComputeWorkgroupSizeX,
        response->limits.maxComputeWorkgroupSizeY,
        response->limits.maxComputeWorkgroupSizeZ,
        response->limits.maxComputeWorkgroupsPerDimension
    );
    callback(WebGPU::RemoteAdapterProxy::create(WTFMove(response->name), WTFMove(resultSupportedFeatures), WTFMove(resultSupportedLimits), response->isFallbackAdapter, *this, m_convertToBackingContext, identifier));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
