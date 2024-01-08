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

#if ENABLE(WEBGL) && ENABLE(GPU_PROCESS)
#include "RemoteGraphicsContextGLProxy.h"
#endif

#if ENABLE(WEBGL)
#include <WebCore/GraphicsContextGL.h>
#endif

namespace WebKit {
using namespace WebCore;


UniqueRef<WebWorkerClient> WebWorkerClient::create(WebPage& page, SerialFunctionDispatcher& dispatcher)
{
    ASSERT(isMainRunLoop());
    return UniqueRef<WebWorkerClient> { *new WebWorkerClient (dispatcher, page.corePage()->displayID()
#if ENABLE(GPU_PROCESS)
        , WebProcess::singleton().ensureGPUProcessConnection().connection()
        , page.webPageProxyIdentifier()
        , page.identifier()
#if ENABLE(VIDEO)
        , WebProcess::singleton().ensureGPUProcessConnection().videoFrameObjectHeapProxy()
#endif
#endif
        ) };
}

WebWorkerClient::WebWorkerClient(SerialFunctionDispatcher& dispatcher, WebCore::PlatformDisplayID displayID
#if ENABLE(GPU_PROCESS)
    , Ref<IPC::Connection> connection, WebPageProxyIdentifier pageProxyID, WebCore::PageIdentifier pageID
#if ENABLE(VIDEO)
    , Ref<RemoteVideoFrameObjectHeapProxy> videoFrameObjectHeapProxy
#endif
#endif
    )
    : m_dispatcher(dispatcher)
    , m_displayID(displayID)
#if ENABLE(GPU_PROCESS)
    , m_connection(WTFMove(connection))
    , m_pageProxyID(pageProxyID)
    , m_pageID(pageID)
#if ENABLE(VIDEO)
    , m_videoFrameObjectHeapProxy(WTFMove(videoFrameObjectHeapProxy))
#endif
#endif
{
}

#if ENABLE(GPU_PROCESS)
RemoteRenderingBackendProxy& WebWorkerClient::ensureRenderingBackend() const
{
    assertIsCurrent(m_dispatcher);
    if (!m_remoteRenderingBackendProxy)
        m_remoteRenderingBackendProxy = RemoteRenderingBackendProxy::create({ RenderingBackendIdentifier::generate(), m_pageProxyID, m_pageID }, m_dispatcher);
    return *m_remoteRenderingBackendProxy;
}
#endif

UniqueRef<WorkerClient> WebWorkerClient::createNestedWorkerClient(SerialFunctionDispatcher& dispatcher)
{
    assertIsCurrent(m_dispatcher);
    return UniqueRef<WorkerClient> { *new WebWorkerClient(dispatcher, m_displayID
#if ENABLE(GPU_PROCESS)
        , m_connection
        , m_pageProxyID
        , m_pageID
#if ENABLE(VIDEO)
        , m_videoFrameObjectHeapProxy.copyRef()
#endif
#endif
        ) };
}

PlatformDisplayID WebWorkerClient::displayID() const
{
    assertIsCurrent(m_dispatcher);
    return m_displayID;
}

RefPtr<ImageBuffer> WebWorkerClient::sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer> imageBuffer)
{
#if ENABLE(GPU_PROCESS)
    if (!is<RemoteSerializedImageBufferProxy>(imageBuffer))
        return SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(imageBuffer));
    auto remote = std::unique_ptr<RemoteSerializedImageBufferProxy>(static_cast<RemoteSerializedImageBufferProxy*>(imageBuffer.release()));
    return RemoteSerializedImageBufferProxy::sinkIntoImageBuffer(WTFMove(remote), ensureRenderingBackend());
#else
    return SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(imageBuffer));
#endif
}

RefPtr<ImageBuffer> WebWorkerClient::createImageBuffer(const FloatSize& size, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, OptionSet<ImageBufferOptions> options) const
{
    assertIsCurrent(m_dispatcher);
#if ENABLE(GPU_PROCESS)
    if (WebProcess::singleton().shouldUseRemoteRenderingFor(purpose))
        return ensureRenderingBackend().createImageBuffer(size, purpose, resolutionScale, colorSpace, pixelFormat, options);
#endif
    return nullptr;
}

#if ENABLE(WEBGL)
RefPtr<GraphicsContextGL> WebWorkerClient::createGraphicsContextGL(const GraphicsContextGLAttributes& attributes) const
{
#if ENABLE(GPU_PROCESS)
    if (WebProcess::singleton().shouldUseRemoteRenderingForWebGL())
#if ENABLE(VIDEO)
        return RemoteGraphicsContextGLProxy::create(m_connection, attributes, ensureRenderingBackend(), m_videoFrameObjectHeapProxy.copyRef());
#else
    return RemoteGraphicsContextGLProxy::create(m_connection, attributes, ensureRenderingBackend());
#endif
#endif
    return WebCore::createWebProcessGraphicsContextGL(attributes, &m_dispatcher);
}
#endif

#if HAVE(WEBGPU_IMPLEMENTATION)
RefPtr<WebCore::WebGPU::GPU> WebWorkerClient::createGPUForWebGPU() const
{
#if ENABLE(GPU_PROCESS)
    return RemoteGPUProxy::create(m_connection, WebGPU::DowncastConvertToBackingContext::create(), WebGPUIdentifier::generate(), ensureRenderingBackend().ensureBackendCreated());
#else
    return nullptr;
#endif
}
#endif

}

