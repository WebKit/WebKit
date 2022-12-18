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
#include "RemoteImageBufferProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "WebPage.h"
#include "WebProcess.h"

#if ENABLE(WEBGL) && ENABLE(GPU_PROCESS)
#include "RemoteGraphicsContextGLProxy.h"
#endif

#if ENABLE(WEBGL)
#include <WebCore/GraphicsContextGL.h>
#endif

namespace WebKit {
using namespace WebCore;

WebWorkerClient::WebWorkerClient(WebPage* page, SerialFunctionDispatcher& dispatcher)
    : m_dispatcher(dispatcher)
#if ENABLE(GPU_PROCESS)
    , m_connection(WebProcess::singleton().ensureGPUProcessConnection().connection())
#endif
{
    ASSERT(isMainRunLoop());
#if ENABLE(GPU_PROCESS)
    m_creationParameters = { RenderingBackendIdentifier::generate(),
        page->webPageProxyIdentifier(),
        page->identifier() };
#endif
    m_displayID = page->corePage()->displayID();
}

#if ENABLE(GPU_PROCESS)
WebWorkerClient::WebWorkerClient(IPC::Connection& connection, SerialFunctionDispatcher& dispatcher, RemoteRenderingBackendCreationParameters& creationParameters, WebCore::PlatformDisplayID& displayID)
    : m_dispatcher(dispatcher)
    , m_connection(connection)
    , m_creationParameters(creationParameters)
    , m_displayID(displayID)
{ }
#else
WebWorkerClient::WebWorkerClient(SerialFunctionDispatcher& dispatcher, WebCore::PlatformDisplayID& displayID)
    : m_dispatcher(dispatcher)
    , m_displayID(displayID)
{ }
#endif

#if ENABLE(GPU_PROCESS)
RemoteRenderingBackendProxy& WebWorkerClient::ensureRenderingBackend() const
{
    assertIsCurrent(m_dispatcher);
    if (!m_remoteRenderingBackendProxy)
        m_remoteRenderingBackendProxy = RemoteRenderingBackendProxy::create(m_creationParameters, m_dispatcher);
    return *m_remoteRenderingBackendProxy;
}
#endif

std::unique_ptr<WorkerClient> WebWorkerClient::clone(SerialFunctionDispatcher& dispatcher)
{
    assertIsCurrent(m_dispatcher);
#if ENABLE(GPU_PROCESS)
    return makeUnique<WebWorkerClient>(m_connection, dispatcher, m_creationParameters, m_displayID);
#else
    return makeUnique<WebWorkerClient>(dispatcher, m_displayID);
#endif
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

RefPtr<ImageBuffer> WebWorkerClient::createImageBuffer(const FloatSize& size, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, bool avoidBackendSizeCheck) const
{
    assertIsCurrent(m_dispatcher);
#if ENABLE(GPU_PROCESS)
    if (WebProcess::singleton().shouldUseRemoteRenderingFor(purpose))
        return ensureRenderingBackend().createImageBuffer(size, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat, avoidBackendSizeCheck);
#endif
    return nullptr;
}

#if ENABLE(WEBGL)
RefPtr<GraphicsContextGL> WebWorkerClient::createGraphicsContextGL(const GraphicsContextGLAttributes& attributes) const
{
#if ENABLE(GPU_PROCESS)
    if (WebProcess::singleton().shouldUseRemoteRenderingForWebGL())
        return RemoteGraphicsContextGLProxy::create(m_connection, attributes, ensureRenderingBackend());
#endif
    return WebCore::createWebProcessGraphicsContextGL(attributes, &m_dispatcher);
}
#endif

}

