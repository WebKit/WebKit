/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
#include "WebWorkerClient.h"

#include "ImageBufferShareableBitmapBackend.h"
#include "RemoteGPUProxy.h"
#include "RemoteImageBufferProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "WebGPUDowncastConvertToBackingContext.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Page.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBGL) && ENABLE(GPU_PROCESS)
#include "RemoteGraphicsContextGLProxy.h"
#endif

#if ENABLE(WEBGL)
#include <WebCore/GraphicsContextGL.h>
#endif

namespace WebKit {
using namespace WebCore;

#if ENABLE(GPU_PROCESS)
class GPUProcessWebWorkerClient final : public WebWorkerClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GPUProcessWebWorkerClient);
public:
    using WebWorkerClient::WebWorkerClient;
    UniqueRef<WorkerClient> createNestedWorkerClient(SerialFunctionDispatcher&) final;
    RefPtr<WebCore::ImageBuffer> sinkIntoImageBuffer(std::unique_ptr<WebCore::SerializedImageBuffer>) final;
    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::ImageBufferPixelFormat, OptionSet<WebCore::ImageBufferOptions>) const final;
#if ENABLE(WEBGL)
    RefPtr<WebCore::GraphicsContextGL> createGraphicsContextGL(const WebCore::GraphicsContextGLAttributes&) const final;
#endif
#if HAVE(WEBGPU_IMPLEMENTATION)
    RefPtr<WebCore::WebGPU::GPU> createGPUForWebGPU() const override;
#endif
private:
    RemoteRenderingBackendProxy& ensureRenderingBackend() const;

    mutable std::unique_ptr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
};


UniqueRef<WorkerClient> GPUProcessWebWorkerClient::createNestedWorkerClient(SerialFunctionDispatcher& dispatcher)
{
    assertIsCurrent(*this->dispatcher());
    return UniqueRef<WorkerClient> { *new GPUProcessWebWorkerClient { dispatcher, m_displayID } };
}

RemoteRenderingBackendProxy& GPUProcessWebWorkerClient::ensureRenderingBackend() const
{
    RefPtr dispatcher = this->dispatcher();
    RELEASE_ASSERT(dispatcher);
    assertIsCurrent(*dispatcher);
    if (!m_remoteRenderingBackendProxy)
        m_remoteRenderingBackendProxy = RemoteRenderingBackendProxy::create(*dispatcher);
    return *m_remoteRenderingBackendProxy;
}

RefPtr<ImageBuffer> GPUProcessWebWorkerClient::sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer> imageBuffer)
{
    RefPtr dispatcher = this->dispatcher();
    if (!dispatcher)
        return nullptr;
    if (is<RemoteSerializedImageBufferProxy>(imageBuffer)) {
        auto remote = std::unique_ptr<RemoteSerializedImageBufferProxy>(static_cast<RemoteSerializedImageBufferProxy*>(imageBuffer.release()));
        return RemoteSerializedImageBufferProxy::sinkIntoImageBuffer(WTFMove(remote), ensureRenderingBackend());
    }
    return WebWorkerClient::sinkIntoImageBuffer(WTFMove(imageBuffer));
}

RefPtr<ImageBuffer> GPUProcessWebWorkerClient::createImageBuffer(const FloatSize& size, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferPixelFormat pixelFormat, OptionSet<ImageBufferOptions> options) const
{
    if (RefPtr dispatcher = this->dispatcher())
        assertIsCurrent(*dispatcher);
    if (WebProcess::singleton().shouldUseRemoteRenderingFor(purpose))
        return ensureRenderingBackend().createImageBuffer(size, purpose, resolutionScale, colorSpace, pixelFormat, options);
    return nullptr;
}

RefPtr<GraphicsContextGL> GPUProcessWebWorkerClient::createGraphicsContextGL(const GraphicsContextGLAttributes& attributes) const
{
    RefPtr dispatcher = this->dispatcher();
    if (!dispatcher)
        return nullptr;
    assertIsCurrent(*dispatcher);
    if (WebProcess::singleton().shouldUseRemoteRenderingForWebGL())
        return RemoteGraphicsContextGLProxy::create(attributes, ensureRenderingBackend(), *dispatcher);
    return WebWorkerClient::createGraphicsContextGL(attributes);
}

#if HAVE(WEBGPU_IMPLEMENTATION)
RefPtr<WebCore::WebGPU::GPU> GPUProcessWebWorkerClient::createGPUForWebGPU() const
{
    RefPtr dispatcher = this->dispatcher();
    if (!dispatcher)
        return nullptr;
    assertIsCurrent(*dispatcher);
    return RemoteGPUProxy::create(WebGPU::DowncastConvertToBackingContext::create(), ensureRenderingBackend(), *dispatcher);
}
#endif

#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebWorkerClient);

UniqueRef<WebWorkerClient> WebWorkerClient::create(Page& page, SerialFunctionDispatcher& dispatcher)
{
    ASSERT(isMainRunLoop());
#if ENABLE(GPU_PROCESS)
    return UniqueRef<GPUProcessWebWorkerClient> { *new GPUProcessWebWorkerClient { dispatcher, page.displayID() } };
#else
    return UniqueRef<WebWorkerClient> { *new WebWorkerClient { dispatcher, page.displayID() } };
#endif
}

WebWorkerClient::WebWorkerClient(SerialFunctionDispatcher& dispatcher, WebCore::PlatformDisplayID displayID)
    : m_dispatcher(dispatcher)
    , m_displayID(displayID)
{
}

WebWorkerClient::~WebWorkerClient() = default;

UniqueRef<WorkerClient> WebWorkerClient::createNestedWorkerClient(SerialFunctionDispatcher& dispatcher)
{
    assertIsCurrent(*this->dispatcher().get());
    return UniqueRef<WorkerClient> { *new WebWorkerClient { dispatcher, m_displayID } };
}

PlatformDisplayID WebWorkerClient::displayID() const
{
    assertIsCurrent(*dispatcher().get());
    return m_displayID;
}

RefPtr<ImageBuffer> WebWorkerClient::sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer> imageBuffer)
{
    assertIsCurrent(*dispatcher().get());
    return SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(imageBuffer));
}

RefPtr<ImageBuffer> WebWorkerClient::createImageBuffer(const FloatSize& size, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferPixelFormat pixelFormat, OptionSet<ImageBufferOptions> options) const
{
    assertIsCurrent(*dispatcher().get());
    return nullptr;
}

#if ENABLE(WEBGL)
RefPtr<GraphicsContextGL> WebWorkerClient::createGraphicsContextGL(const GraphicsContextGLAttributes& attributes) const
{
    assertIsCurrent(*dispatcher().get());
    return WebCore::createWebProcessGraphicsContextGL(attributes);
}
#endif

#if HAVE(WEBGPU_IMPLEMENTATION)
RefPtr<WebCore::WebGPU::GPU> WebWorkerClient::createGPUForWebGPU() const
{
    return nullptr;
}
#endif

}
