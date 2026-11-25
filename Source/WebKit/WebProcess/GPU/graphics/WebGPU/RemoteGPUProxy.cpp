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
#include "RemoteGPUProxy.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcessConnection.h"
#include "ModelConvertToBackingContext.h"
#include "RemoteCompositorIntegrationProxy.h"
#include "RemoteDeviceProxy.h"
#include "RemoteGPU.h"
#include "RemoteGPUMessages.h"
#include "RemoteGPUProxyMessages.h"
#include "RemoteMeshProxy.h"
#include "RemotePresentationContextProxy.h"
#include "RemoteQueueProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteWebGPUInstanceProxy.h"
#include "WebGPUDescriptorSerializers.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUPresentationContextDescriptor.h>
#include <WebCore/WebGPUSupportedFeatures.h>
#include <WebCore/WebGPUSupportedLimits.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteGPUProxy);

RefPtr<RemoteGPUProxy> RemoteGPUProxy::create(ModelConvertToBackingContext& modelConvertToBackingContext, WebPage& page)
{
    return RemoteGPUProxy::create(modelConvertToBackingContext, protect(page.ensureRemoteRenderingBackendProxy()), RunLoop::mainSingleton());
}

RefPtr<RemoteGPUProxy> RemoteGPUProxy::create(ModelConvertToBackingContext& modelConvertToBackingContext, RemoteRenderingBackendProxy& renderingBackend, SerialFunctionDispatcher& dispatcher)
{
    constexpr size_t connectionBufferSizeLog2 = 21;
    auto connectionPair = IPC::StreamClientConnection::create(connectionBufferSizeLog2, WebProcess::singleton().gpuProcessTimeoutDuration());
    if (!connectionPair)
        return nullptr;
    RefPtr renderingBackendIPCConnection = renderingBackend.gpuProcessIPCConnection();
    if (!renderingBackendIPCConnection)
        return nullptr;
    auto [clientConnection, serverConnectionHandle] = WTF::move(*connectionPair);
    Ref instance = adoptRef(*new RemoteGPUProxy(modelConvertToBackingContext, renderingBackend, dispatcher));
    instance->initializeIPC(WTF::move(clientConnection), *renderingBackendIPCConnection, WTF::move(serverConnectionHandle));
    // TODO: We must wait until initialized, because at the moment we cannot receive IPC messages
    // during wait while in synchronous stream send. Should be fixed as part of https://bugs.webkit.org/show_bug.cgi?id=217211.
    instance->waitUntilInitialized();
    return instance;
}


RemoteGPUProxy::RemoteGPUProxy(ModelConvertToBackingContext& modelConvertToBackingContext, RemoteRenderingBackendProxy& renderingBackend, SerialFunctionDispatcher& dispatcher)
    : m_modelConvertToBackingContext(modelConvertToBackingContext)
    , m_dispatcher(dispatcher)
    , m_renderingBackend(renderingBackend)
{
}

RemoteGPUProxy::~RemoteGPUProxy()
{
    disconnectGpuProcessIfNeeded();
}

void RemoteGPUProxy::initializeIPC(Ref<IPC::StreamClientConnection>&& streamConnection, IPC::Connection& renderingBackendIPCConnection, IPC::StreamServerConnection::Handle&& serverHandle)
{
    lazyInitialize(m_streamConnection, WTF::move(streamConnection));
    m_streamConnection->open(*this, *this);
    bool didCreateGPU = false;
    callOnMainRunLoopAndWait([&] {
        Ref gpuProcessConnection = WebProcess::singleton().ensureGPUProcessConnection();
        if (&gpuProcessConnection->connection() != &renderingBackendIPCConnection)
            return;
        gpuProcessConnection->createGPU(m_backing, m_renderingBackend->renderingBackendIdentifier(), WTF::move(serverHandle));
        m_gpuProcessConnection = gpuProcessConnection.get();
        lazyInitialize(m_gpuProcessIPCConnection, Ref { gpuProcessConnection->connection() });
        didCreateGPU = true;
    });
    if (!didCreateGPU)
        abandonGPUProcess();
}

void RemoteGPUProxy::disconnectGpuProcessIfNeeded()
{
    if (m_lost)
        return;
    m_streamConnection->invalidate();
    // FIXME: deallocate m_streamConnection once the children work without the connection.
    ensureOnMainRunLoop([identifier = m_backing, weakGPUProcessConnection = WTF::move(m_gpuProcessConnection)]() {
        RefPtr gpuProcessConnection = weakGPUProcessConnection.get();
        if (!gpuProcessConnection)
            return;
        gpuProcessConnection->releaseGPU(identifier);
    });
}

void RemoteGPUProxy::didClose(IPC::Connection&)
{
    ASSERT(m_streamConnection);
    gpuProcessConnectionDidClose();
}

void RemoteGPUProxy::abandonGPUProcess()
{
    m_streamConnection->invalidate();
    m_lost = true;
}

void RemoteGPUProxy::gpuProcessConnectionDidClose()
{
    Ref protectedThis { *this };
    abandonGPUProcess();
    // Each stream has its own Mach port and receive queue, so this connection can observe the GPU
    // process exiting before GPUProcessConnection or RemoteRenderingBackendProxy has dispatched
    // its own didClose(). Tear those down now so a replacement RemoteGPUProxy connects to a new
    // GPU process and a rendering backend created on it.
    if (RefPtr gpuProcessIPCConnection = m_gpuProcessIPCConnection) {
        ensureOnMainRunLoop([gpuProcessIPCConnection] {
            WebProcess::singleton().gpuProcessConnectionDidClose(*gpuProcessIPCConnection);
        });
        m_renderingBackend->gpuProcessConnectionDidClose(*gpuProcessIPCConnection);
    }
}

void RemoteGPUProxy::dispatch(Function<void()>&& function)
{
    if (RefPtr dispatcher = m_dispatcher.get())
        dispatcher->dispatch(WTF::move(function));
}

bool RemoteGPUProxy::isCurrent() const
{
    RefPtr dispatcher = m_dispatcher.get();
    return dispatcher && dispatcher->isCurrent();
}

void RemoteGPUProxy::wasCreated(bool didSucceed)
{
    ASSERT(!m_didInitialize);
    m_didInitialize = true;
    if (!didSucceed)
        abandonGPUProcess();
}

void RemoteGPUProxy::addUncapturedErrorListener(WebGPUDeviceIdentifier identifier, WebGPU::RemoteDeviceProxy& device)
{
    ASSERT(isCurrent());
    // Forget the devices that went away while listening.
    m_uncapturedErrorListeners.removeIf([](auto& entry) {
        return !entry.value.get();
    });
    m_uncapturedErrorListeners.set(identifier, device);
}

void RemoteGPUProxy::removeUncapturedErrorListener(WebGPUDeviceIdentifier identifier)
{
    ASSERT(isCurrent());
    m_uncapturedErrorListeners.remove(identifier);
}

void RemoteGPUProxy::didGenerateUncapturedError(WebGPUDeviceIdentifier identifier, ::WebGPU::Error&& error)
{
    ASSERT(isCurrent());
    // This message and the replies to the messages of the stream come on the same connection, and the
    // connection dispatches them in the order the GPU process sent them.
    auto iterator = m_uncapturedErrorListeners.find(identifier);
    if (iterator == m_uncapturedErrorListeners.end())
        return;
    RefPtr device = iterator->value;
    if (!device) {
        m_uncapturedErrorListeners.remove(iterator);
        return;
    }
    device->didGenerateUncapturedError(WTF::move(error));
}

void RemoteGPUProxy::waitUntilInitialized()
{
    if (m_didInitialize || m_lost)
        return;
    if (m_streamConnection->waitForAndDispatchImmediately<Messages::RemoteGPUProxy::WasCreated>(m_backing) == IPC::Error::NoError)
        return;
    gpuProcessConnectionDidClose();
}

Ref<WebGPU::RemoteWebGPUInstanceProxy> RemoteGPUProxy::ensureInstance()
{
    // Only the dispatcher thread creates and replaces the instance, so it needs no lock.
    assertIsCurrent(*this);
    if (RefPtr instance = m_instance)
        return instance.releaseNonNull();
    Ref instance = WebGPU::RemoteWebGPUInstanceProxy::create(*this);
    m_instance = instance.get();
    return instance;
}

void RemoteGPUProxy::requestAdapter(const ::WebGPU::RequestAdapterOptions& options, CompletionHandler<void(RefPtr<::WebGPU::Adapter>&&)>&& callback)
{
    ensureInstance()->requestAdapter(options, WTF::move(callback));
}

RefPtr<WebKit::Mesh> RemoteGPUProxy::createModelBacking(unsigned width, unsigned height, WebModel::ImageAsset&& diffuseTexture, WebModel::ImageAsset&& specularTexture, bool standardDynamicRange, CompletionHandler<void(Vector<MachSendRight>&&)>&& callback)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto identifier = WebModelIdentifier::generate();

    auto sendResult = sendSync(Messages::RemoteGPU::CreateModelBacking(width, height, WTF::move(diffuseTexture), WTF::move(specularTexture), identifier, standardDynamicRange));
    if (!sendResult.succeeded()) {
        gpuProcessConnectionDidClose();
        callback({ });
        return nullptr;
    }

    auto [response] = sendResult.takeReply();
    callback(WTF::move(response));

    UNUSED_PARAM(sendResult);

    auto result = RemoteMeshProxy::create(root(), m_modelConvertToBackingContext, identifier);
    result->setLabel("Placeholder model label"_s);
    return result;
#else
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(standardDynamicRange);
    UNUSED_PARAM(callback);
    return nullptr;
#endif
}

RefPtr<::WebGPU::PresentationContext> RemoteGPUProxy::createPresentationContext(const WebCore::WebGPUPresentationContextDescriptor& descriptor)
{
    // FIXME: Should we be consulting m_lost?

    auto convertedDescriptor = WebGPU::serialize(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUPresentationContextIdentifier::generate();
    auto sendResult = send(Messages::RemoteGPU::CreatePresentationContext(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return WebGPU::RemotePresentationContextProxy::create(*this, identifier);
}

RefPtr<WebCore::WebGPUCompositorIntegration> RemoteGPUProxy::createCompositorIntegration()
{
    // FIXME: Should we be consulting m_lost?

    auto identifier = WebGPUCompositorIntegrationIdentifier::generate();
    auto sendResult = send(Messages::RemoteGPU::CreateCompositorIntegration(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return WebGPU::RemoteCompositorIntegrationProxy::create(*this, identifier);
}

void RemoteGPUProxy::copyExternalImageToTexture(::WebGPU::Queue& queue, const WebCore::WebGPUExternalImageSource& source, const WebCore::WebGPUImageCopyTextureTagged& destination, const ::WebGPU::Extent3D& copySize)
{
    // Every ::WebGPU::Queue in the Web Process is a RemoteQueueProxy.
    downcast<WebGPU::RemoteQueueProxy>(queue).copyExternalImageToTexture(source, destination, copySize);
}

RefPtr<WebCore::NativeImage> RemoteGPUProxy::nativeImage(::WebGPU::Queue& queue, WebCore::VideoFrame& videoFrame)
{
    return downcast<WebGPU::RemoteQueueProxy>(queue).getNativeImage(videoFrame);
}

RefPtr<::WebGPU::ExternalTexture> RemoteGPUProxy::importExternalTexture(::WebGPU::Device& device, const WebCore::WebGPUExternalTextureDescriptor& descriptor)
{
    // Every ::WebGPU::Device in the Web Process is a RemoteDeviceProxy.
    return downcast<WebGPU::RemoteDeviceProxy>(device).importExternalTexture(descriptor);
}

#if PLATFORM(COCOA) && ENABLE(VIDEO)
void RemoteGPUProxy::updateExternalTexture(::WebGPU::Device& device, const ::WebGPU::ExternalTexture& externalTexture, const WebCore::MediaPlayerIdentifier& mediaPlayerIdentifier)
{
    downcast<WebGPU::RemoteDeviceProxy>(device).updateExternalTexture(externalTexture, mediaPlayerIdentifier);
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
