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
#include "RemoteAdapterProxy.h"
#include "RemoteCompositorIntegrationProxy.h"
#include "RemoteDeviceProxy.h"
#include "RemoteGPU.h"
#include "RemoteGPUMessages.h"
#include "RemoteGPUProxyMessages.h"
#include "RemoteMeshProxy.h"
#include "RemotePresentationContextProxy.h"
#include "RemoteQueueProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUPresentationContextDescriptor.h>
#include <WebCore/WebGPUSupportedFeatures.h>
#include <WebCore/WebGPUSupportedLimits.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteGPUProxy);

RefPtr<RemoteGPUProxy> RemoteGPUProxy::create(WebGPU::ConvertToBackingContext& convertToBackingContext, ModelConvertToBackingContext& modelConvertToBackingContext, WebPage& page)
{
    return RemoteGPUProxy::create(convertToBackingContext, modelConvertToBackingContext, protect(page.ensureRemoteRenderingBackendProxy()), RunLoop::mainSingleton());
}

RefPtr<RemoteGPUProxy> RemoteGPUProxy::create(WebGPU::ConvertToBackingContext& convertToBackingContext, ModelConvertToBackingContext& modelConvertToBackingContext, RemoteRenderingBackendProxy& renderingBackend, SerialFunctionDispatcher& dispatcher)
{
    constexpr size_t connectionBufferSizeLog2 = 21;
    auto connectionPair = IPC::StreamClientConnection::create(connectionBufferSizeLog2, WebProcess::singleton().gpuProcessTimeoutDuration());
    if (!connectionPair)
        return nullptr;
    auto [clientConnection, serverConnectionHandle] = WTF::move(*connectionPair);
    Ref instance = adoptRef(*new RemoteGPUProxy(convertToBackingContext, modelConvertToBackingContext, dispatcher));
    instance->initializeIPC(WTF::move(clientConnection), renderingBackend.ensureBackendCreated(), WTF::move(serverConnectionHandle));
    // TODO: We must wait until initialized, because at the moment we cannot receive IPC messages
    // during wait while in synchronous stream send. Should be fixed as part of https://bugs.webkit.org/show_bug.cgi?id=217211.
    instance->waitUntilInitialized();
    return instance;
}


RemoteGPUProxy::RemoteGPUProxy(WebGPU::ConvertToBackingContext& convertToBackingContext, ModelConvertToBackingContext& modelConvertToBackingContext, SerialFunctionDispatcher& dispatcher)
    : m_convertToBackingContext(convertToBackingContext)
    , m_modelConvertToBackingContext(modelConvertToBackingContext)
    , m_dispatcher(dispatcher)
{
}

RemoteGPUProxy::~RemoteGPUProxy()
{
    disconnectGpuProcessIfNeeded();
}

void RemoteGPUProxy::initializeIPC(Ref<IPC::StreamClientConnection>&& streamConnection, RemoteRenderingBackendIdentifier renderingBackend, IPC::StreamServerConnection::Handle&& serverHandle)
{
    lazyInitialize(m_streamConnection, WTF::move(streamConnection));
    m_streamConnection->open(*this, *this);
    callOnMainRunLoopAndWait([&]() {
        Ref gpuProcessConnection = WebProcess::singleton().ensureGPUProcessConnection();
        gpuProcessConnection->createGPU(m_backing, renderingBackend, WTF::move(serverHandle));
        m_gpuProcessConnection = gpuProcessConnection.get();
    });
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
    abandonGPUProcess();
}

void RemoteGPUProxy::abandonGPUProcess()
{
    m_streamConnection->invalidate();
    m_lost = true;
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

void RemoteGPUProxy::waitUntilInitialized()
{
    if (m_didInitialize)
        return;
    if (m_streamConnection->waitForAndDispatchImmediately<Messages::RemoteGPUProxy::WasCreated>(m_backing) == IPC::Error::NoError)
        return;
    abandonGPUProcess();
}

void RemoteGPUProxy::requestAdapter(const ::WebGPU::RequestAdapterOptions& options, CompletionHandler<void(RefPtr<::WebGPU::Adapter>&&)>&& callback)
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
        abandonGPUProcess();
        callback(nullptr);
        return;
    }
    auto [response] = sendResult.takeReply();
    if (!response) {
        callback(nullptr);
        return;
    }

    ::WebGPU::AdapterInfo info {
        .name = WTF::move(response->name),
        .isFallbackAdapter = response->isFallbackAdapter,
        .subgroupMinSize = response->subgroupMinSize,
        .subgroupMaxSize = response->subgroupMaxSize,
    };
    callback(WebGPU::RemoteAdapterProxy::create(WTF::move(response->features), WebGPU::convertFromBacking(response->limits), WTF::move(info), options.xrCompatible, *this, m_convertToBackingContext, identifier));
}

RefPtr<WebKit::Mesh> RemoteGPUProxy::createModelBacking(unsigned width, unsigned height, WebModel::ImageAsset&& diffuseTexture, WebModel::ImageAsset&& specularTexture, bool standardDynamicRange, CompletionHandler<void(Vector<MachSendRight>&&)>&& callback)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto identifier = WebModelIdentifier::generate();

    auto sendResult = sendSync(Messages::RemoteGPU::CreateModelBacking(width, height, WTF::move(diffuseTexture), WTF::move(specularTexture), identifier, standardDynamicRange));
    if (!sendResult.succeeded()) {
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

    // FIXME: This is super yucky. We should solve this a better way. (For both WK1 and WK2.)
    // Maybe PresentationContext needs a present() function?
    Ref compositorIntegration = const_cast<WebGPU::RemoteCompositorIntegrationProxy&>(m_convertToBackingContext->convertToRawBacking(protect(descriptor.compositorIntegration).get()));

    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteGPU::CreatePresentationContext(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    Ref result = WebGPU::RemotePresentationContextProxy::create(*this, m_convertToBackingContext, identifier);
    compositorIntegration->setPresentationContext(result);
    return result;
}

RefPtr<WebCore::WebGPUCompositorIntegration> RemoteGPUProxy::createCompositorIntegration()
{
    // FIXME: Should we be consulting m_lost?

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteGPU::CreateCompositorIntegration(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return WebGPU::RemoteCompositorIntegrationProxy::create(*this, m_convertToBackingContext, identifier);
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

void RemoteGPUProxy::paintToCanvas(WebCore::NativeImage&, const WebCore::IntSize&, WebCore::GraphicsContext&)
{
    ASSERT_NOT_REACHED();
}

bool RemoteGPUProxy::isValid(const WebCore::WebGPUCompositorIntegration&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::Buffer&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::Adapter&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::BindGroup&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::BindGroupLayout&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::CommandBuffer&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::CommandEncoder&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::ComputePassEncoder&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::ComputePipeline&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::Device&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::ExternalTexture&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::PipelineLayout&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::PresentationContext&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::QuerySet&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::Queue&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::RenderBundleEncoder&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::RenderBundle&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::RenderPassEncoder&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::RenderPipeline&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::Sampler&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::ShaderModule&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::Texture&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::TextureView&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::XRBinding&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::XRSubImage&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::XRProjectionLayer&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
bool RemoteGPUProxy::isValid(const ::WebGPU::XRView&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
