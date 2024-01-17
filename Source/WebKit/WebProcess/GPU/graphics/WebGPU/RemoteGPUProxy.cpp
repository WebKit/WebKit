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
#include "RemoteGPUProxy.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcessConnection.h"
#include "RemoteAdapterProxy.h"
#include "RemoteCompositorIntegrationProxy.h"
#include "RemoteGPU.h"
#include "RemoteGPUMessages.h"
#include "RemoteGPUProxyMessages.h"
#include "RemotePresentationContextProxy.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/WebGPUPresentationContextDescriptor.h>
#include <WebCore/WebGPUSupportedFeatures.h>
#include <WebCore/WebGPUSupportedLimits.h>

namespace WebKit {

RefPtr<RemoteGPUProxy> RemoteGPUProxy::create(IPC::Connection& connection, WebGPU::ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, RenderingBackendIdentifier renderingBackend)
{
    constexpr size_t connectionBufferSizeLog2 = 21;
    auto connectionPair = IPC::StreamClientConnection::create(connectionBufferSizeLog2);
    if (!connectionPair)
        return nullptr;
    auto [clientConnection, serverConnectionHandle] = WTFMove(*connectionPair);
    auto remoteGPUProxy = adoptRef(new RemoteGPUProxy(connection, clientConnection, convertToBackingContext, identifier));
    remoteGPUProxy->initializeIPC(WTFMove(serverConnectionHandle), renderingBackend);
    return remoteGPUProxy;
}


RemoteGPUProxy::RemoteGPUProxy(IPC::Connection& connection, Ref<IPC::StreamClientConnection> clientConnection, WebGPU::ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_connection(&connection)
    , m_streamConnection(WTFMove(clientConnection))
{
}

RemoteGPUProxy::~RemoteGPUProxy()
{
    disconnectGpuProcessIfNeeded();
}

void RemoteGPUProxy::initializeIPC(IPC::StreamServerConnection::Handle&& serverConnectionHandle, RenderingBackendIdentifier renderingBackend)
{
    m_connection->send(Messages::GPUConnectionToWebProcess::CreateRemoteGPU(m_backing, renderingBackend, WTFMove(serverConnectionHandle)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    m_streamConnection->open(*this);
    // TODO: We must wait until initialized, because at the moment we cannot receive IPC messages
    // during wait while in synchronous stream send. Should be fixed as part of https://bugs.webkit.org/show_bug.cgi?id=217211.
    waitUntilInitialized();
}

void RemoteGPUProxy::disconnectGpuProcessIfNeeded()
{
    if (m_connection) {
        m_streamConnection->invalidate();
        m_connection->send(Messages::GPUConnectionToWebProcess::ReleaseRemoteGPU(m_backing), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
        m_connection = nullptr;
    }
}

void RemoteGPUProxy::didClose(IPC::Connection&)
{
    ASSERT(m_connection);
    abandonGPUProcess();
}

void RemoteGPUProxy::abandonGPUProcess()
{
    m_streamConnection->invalidate();
    m_connection = nullptr;
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
    if (m_streamConnection->waitForAndDispatchImmediately<Messages::RemoteGPUProxy::WasCreated>(m_backing, defaultSendTimeout) == IPC::Error::NoError)
        return;
    m_lost = true;
}

void RemoteGPUProxy::requestAdapter(const WebCore::WebGPU::RequestAdapterOptions& options, CompletionHandler<void(RefPtr<WebCore::WebGPU::Adapter>&&)>&& callback)
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
    if (!sendResult.succeeded()) {
        m_lost = true;
        callback(nullptr);
        return;
    }
    auto [response] = sendResult.takeReply();
    if (!response) {
        callback(nullptr);
        return;
    }

    auto resultSupportedFeatures = WebCore::WebGPU::SupportedFeatures::create(WTFMove(response->features.features));
    auto resultSupportedLimits = WebCore::WebGPU::SupportedLimits::create(
        response->limits.maxTextureDimension1D,
        response->limits.maxTextureDimension2D,
        response->limits.maxTextureDimension3D,
        response->limits.maxTextureArrayLayers,
        response->limits.maxBindGroups,
        response->limits.maxBindGroupsPlusVertexBuffers,
        response->limits.maxBindingsPerBindGroup,
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
        response->limits.maxBufferSize,
        response->limits.maxVertexAttributes,
        response->limits.maxVertexBufferArrayStride,
        response->limits.maxInterStageShaderComponents,
        response->limits.maxInterStageShaderVariables,
        response->limits.maxColorAttachments,
        response->limits.maxColorAttachmentBytesPerSample,
        response->limits.maxComputeWorkgroupStorageSize,
        response->limits.maxComputeInvocationsPerWorkgroup,
        response->limits.maxComputeWorkgroupSizeX,
        response->limits.maxComputeWorkgroupSizeY,
        response->limits.maxComputeWorkgroupSizeZ,
        response->limits.maxComputeWorkgroupsPerDimension
    );
    callback(WebGPU::RemoteAdapterProxy::create(WTFMove(response->name), WTFMove(resultSupportedFeatures), WTFMove(resultSupportedLimits), response->isFallbackAdapter, *this, m_convertToBackingContext, identifier));
}

Ref<WebCore::WebGPU::PresentationContext> RemoteGPUProxy::createPresentationContext(const WebCore::WebGPU::PresentationContextDescriptor& descriptor)
{
    // FIXME: Should we be consulting m_lost?

    // FIXME: This is super yucky. We should solve this a better way. (For both WK1 and WK2.)
    // Maybe PresentationContext needs a present() function?
    auto& compositorIntegration = const_cast<WebGPU::RemoteCompositorIntegrationProxy&>(m_convertToBackingContext->convertToRawBacking(descriptor.compositorIntegration));

    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return WebGPU::RemotePresentationContextProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteGPU::CreatePresentationContext(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    auto result = WebGPU::RemotePresentationContextProxy::create(*this, m_convertToBackingContext, identifier);
    compositorIntegration.setPresentationContext(result);
    return result;
}

Ref<WebCore::WebGPU::CompositorIntegration> RemoteGPUProxy::createCompositorIntegration()
{
    // FIXME: Should we be consulting m_lost?

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteGPU::CreateCompositorIntegration(identifier));
    UNUSED_VARIABLE(sendResult);

    return WebGPU::RemoteCompositorIntegrationProxy::create(*this, m_convertToBackingContext, identifier);
}

void RemoteGPUProxy::paintToCanvas(WebCore::NativeImage&, const WebCore::IntSize&, WebCore::GraphicsContext&)
{
    ASSERT_NOT_REACHED();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
