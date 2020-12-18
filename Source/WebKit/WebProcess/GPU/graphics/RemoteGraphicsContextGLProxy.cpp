/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteGraphicsContextGLProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcessConnection.h"
#include "RemoteGraphicsContextGLMessages.h"
#include "RemoteGraphicsContextGLProxyMessages.h"
#include "WebProcess.h"

#if PLATFORM(COCOA)
#include <WebCore/GraphicsContextCG.h>
#include <WebCore/GraphicsContextGLIOSurfaceSwapChain.h>
#endif

namespace WebKit {

using namespace WebCore;

RefPtr<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::create(const GraphicsContextGLAttributes& attributes)
{
    return adoptRef(new RemoteGraphicsContextGLProxy(attributes));
}

RemoteGraphicsContextGLProxy::RemoteGraphicsContextGLProxy(const GraphicsContextGLAttributes& attrs)
    : RemoteGraphicsContextGLProxyBase(attrs)
{
    IPC::MessageReceiverMap& messageReceiverMap = WebProcess::singleton().ensureGPUProcessConnection().messageReceiverMap();
    messageReceiverMap.addMessageReceiver(Messages::RemoteGraphicsContextGLProxy::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64(), *this);
    send(Messages::GPUConnectionToWebProcess::CreateGraphicsContextGL(attrs, m_graphicsContextGLIdentifier), 0);
}

RemoteGraphicsContextGLProxy::~RemoteGraphicsContextGLProxy()
{
    IPC::MessageReceiverMap& messageReceiverMap = WebProcess::singleton().ensureGPUProcessConnection().messageReceiverMap();
    messageReceiverMap.removeMessageReceiver(*this);
    send(Messages::GPUConnectionToWebProcess::ReleaseGraphicsContextGL(m_graphicsContextGLIdentifier), 0);
#if PLATFORM(COCOA)
    platformSwapChain().recycleBuffer();
#endif
}

IPC::Connection* RemoteGraphicsContextGLProxy::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureGPUProcessConnection().connection();
}

uint64_t RemoteGraphicsContextGLProxy::messageSenderDestinationID() const
{
    return m_graphicsContextGLIdentifier.toUInt64();
}

void RemoteGraphicsContextGLProxy::reshape(int width, int height)
{
    m_currentWidth = width;
    m_currentHeight = height;
    send(Messages::RemoteGraphicsContextGL::Reshape(width, height), m_graphicsContextGLIdentifier);
}

void RemoteGraphicsContextGLProxy::prepareForDisplay()
{
#if PLATFORM(COCOA)
    MachSendRight displayBufferSendRight;
    sendSync(Messages::RemoteGraphicsContextGL::PrepareForDisplay(), Messages::RemoteGraphicsContextGL::PrepareForDisplay::Reply(displayBufferSendRight), m_graphicsContextGLIdentifier, 10_s);
    auto displayBuffer = IOSurface::createFromSendRight(WTFMove(displayBufferSendRight), sRGBColorSpaceRef());
    if (displayBuffer) {
        auto& sc = platformSwapChain();
        sc.recycleBuffer();
        sc.present({ WTFMove(displayBuffer), nullptr });
    }
#else
    sendSync(Messages::RemoteGraphicsContextGL::PrepareForDisplay(), Messages::RemoteGraphicsContextGL::PrepareForDisplay::Reply(), m_graphicsContextGLIdentifier, 10_s);
#endif
    markLayerComposited();
}

void RemoteGraphicsContextGLProxy::ensureExtensionEnabled(const String& extension)
{
    send(Messages::RemoteGraphicsContextGL::EnsureExtensionEnabled(extension), m_graphicsContextGLIdentifier);
}

void RemoteGraphicsContextGLProxy::notifyMarkContextChanged()
{
    send(Messages::RemoteGraphicsContextGL::NotifyMarkContextChanged(), m_graphicsContextGLIdentifier);
}

void RemoteGraphicsContextGLProxy::simulateContextChanged()
{
    // FIXME: Currently not implemented because it's not clear this is the right way. https://bugs.webkit.org/show_bug.cgi?id=219349
    notImplemented();
}

void RemoteGraphicsContextGLProxy::paintRenderingResultsToCanvas(ImageBuffer*)
{
    notImplemented();
}

void RemoteGraphicsContextGLProxy::paintCompositedResultsToCanvas(ImageBuffer*)
{
    notImplemented();
}

RefPtr<ImageData> RemoteGraphicsContextGLProxy::paintRenderingResultsToImageData()
{
    notImplemented();
    return nullptr;
}

void RemoteGraphicsContextGLProxy::wasCreated(String&& availableExtensions, String&& requestedExtensions)
{
    if (m_didInitialize) {
        // Initialization timed out before, so lose the context now.
        wasLost();
        return;
    }
    initialize(availableExtensions, requestedExtensions);
    m_didInitialize = true;
}

void RemoteGraphicsContextGLProxy::wasLost()
{
    for (auto* client : copyToVector(m_clients))
        client->forceContextLost();
}

void RemoteGraphicsContextGLProxy::wasChanged()
{
    for (auto* client : copyToVector(m_clients))
        client->dispatchContextChangedNotification();
}

void RemoteGraphicsContextGLProxy::waitUntilInitialized()
{
    if (m_didInitialize)
        return;
    Ref<IPC::Connection> connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
    if (connection->waitForAndDispatchImmediately<Messages::RemoteGraphicsContextGLProxy::WasCreated>(m_graphicsContextGLIdentifier, 10_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives))
        return;
    // Timed out, so report initialized with dummy data. We should lose the context.
    wasCreated("", "");
    // FIXME: Need to mark the context as lost, but it's not safe to force lost in this call stack.
}

} // namespace WebKit

#endif
