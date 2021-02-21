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
#include <WebCore/ImageBuffer.h>

#if PLATFORM(COCOA)
#include <WebCore/GraphicsContextCG.h>
#include <WebCore/GraphicsContextGLIOSurfaceSwapChain.h>
#endif

namespace WebKit {

using namespace WebCore;

RefPtr<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::create(const GraphicsContextGLAttributes& attributes, RenderingBackendIdentifier renderingBackend)
{
    return adoptRef(new RemoteGraphicsContextGLProxy(WebProcess::singleton().ensureGPUProcessConnection(), attributes, renderingBackend));
}

static constexpr size_t defaultStreamSize = 1 << 21;

RemoteGraphicsContextGLProxy::RemoteGraphicsContextGLProxy(GPUProcessConnection& gpuProcessConnection, const GraphicsContextGLAttributes& attributes, RenderingBackendIdentifier renderingBackend)
    : RemoteGraphicsContextGLProxyBase(attributes)
    , m_gpuProcessConnection(&gpuProcessConnection)
    , m_streamConnection(gpuProcessConnection.connection(), defaultStreamSize)
{
    m_gpuProcessConnection->addClient(*this);
    m_gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::RemoteGraphicsContextGLProxy::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64(), *this);
    connection().send(Messages::GPUConnectionToWebProcess::CreateGraphicsContextGL(attributes, m_graphicsContextGLIdentifier, renderingBackend, m_streamConnection.streamBuffer()), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

RemoteGraphicsContextGLProxy::~RemoteGraphicsContextGLProxy()
{
    disconnectGpuProcessIfNeeded();
#if PLATFORM(COCOA)
    platformSwapChain().recycleBuffer();
#endif
}

void RemoteGraphicsContextGLProxy::reshape(int width, int height)
{
    if (isContextLost())
        return;
    m_currentWidth = width;
    m_currentHeight = height;
    auto sendResult = send(Messages::RemoteGraphicsContextGL::Reshape(width, height));
    if (!sendResult)
        markContextLost();
}

void RemoteGraphicsContextGLProxy::prepareForDisplay()
{
    if (isContextLost())
        return;
#if PLATFORM(COCOA)
    MachSendRight displayBufferSendRight;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PrepareForDisplay(), Messages::RemoteGraphicsContextGL::PrepareForDisplay::Reply(displayBufferSendRight));
    if (!sendResult) {
        markContextLost();
        return;
    }
    auto displayBuffer = IOSurface::createFromSendRight(WTFMove(displayBufferSendRight), sRGBColorSpaceRef());
    if (displayBuffer) {
        // Claim in the WebProcess ownership of the IOSurface constructed by the GPUProcess so that Jetsam knows which processes to kill.
        displayBuffer->setOwnership(mach_task_self());

        auto& sc = platformSwapChain();
        sc.recycleBuffer();
        sc.present({ WTFMove(displayBuffer), nullptr });
    }
#else
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PrepareForDisplay(), Messages::RemoteGraphicsContextGL::PrepareForDisplay::Reply());
    if (!sendResult) {
        markContextLost();
        return;
    }
#endif
    markLayerComposited();
}

void RemoteGraphicsContextGLProxy::ensureExtensionEnabled(const String& extension)
{
    if (isContextLost())
        return;
    auto sendResult = send(Messages::RemoteGraphicsContextGL::EnsureExtensionEnabled(extension));
    if (!sendResult)
        markContextLost();
}

void RemoteGraphicsContextGLProxy::notifyMarkContextChanged()
{
    if (isContextLost())
        return;
    auto sendResult = send(Messages::RemoteGraphicsContextGL::NotifyMarkContextChanged());
    if (!sendResult)
        markContextLost();
}

void RemoteGraphicsContextGLProxy::simulateContextChanged()
{
    // FIXME: Currently not implemented because it's not clear this is the right way. https://bugs.webkit.org/show_bug.cgi?id=219349
    notImplemented();
}

void RemoteGraphicsContextGLProxy::paintRenderingResultsToCanvas(ImageBuffer& buffer)
{
    // FIXME: the buffer is "relatively empty" always, but for consistency, we need to ensure
    // no pending operations are targeted for the `buffer`.
    buffer.flushDrawingContext();

    // FIXME: We cannot synchronize so that we would know no pending operations are using the `buffer`.

    // FIXME: Currently RemoteImageBufferProxy::getImageData et al do not wait for the flushes of the images
    // inside the display lists. Rather, it assumes that processing its sequence (e.g. the display list) will equal to read flush being
    // fulfilled. For below, we cannot create a new flush id since we go through different sequence (RemoteGraphicsContextGL sequence)

    // FIXME: Maybe implement IPC::Fence or something similar.
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PaintRenderingResultsToCanvas(buffer.renderingResourceIdentifier()), Messages::RemoteGraphicsContextGL::PaintRenderingResultsToCanvas::Reply());
    if (!sendResult) {
        markContextLost();
        return;
    }
}

void RemoteGraphicsContextGLProxy::paintCompositedResultsToCanvas(ImageBuffer& buffer)
{
    buffer.flushDrawingContext();
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PaintCompositedResultsToCanvas(buffer.renderingResourceIdentifier()), Messages::RemoteGraphicsContextGL::PaintCompositedResultsToCanvas::Reply());
    if (!sendResult) {
        markContextLost();
        return;
    }
}

bool RemoteGraphicsContextGLProxy::copyTextureFromMedia(MediaPlayer& mediaPlayer, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY)
{
    bool result = false;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::CopyTextureFromMedia(mediaPlayer.identifier(), texture, target, level, internalFormat, format, type, premultiplyAlpha, flipY), Messages::RemoteGraphicsContextGL::CopyTextureFromMedia::Reply(result));
    if (!sendResult) {
        markContextLost();
        return false;
    }

    return result;
}

void RemoteGraphicsContextGLProxy::synthesizeGLError(GCGLenum error)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::SynthesizeGLError(error));
        if (!sendResult)
            wasLost();
        return;
    }
    m_errorWhenContextIsLost = error;
}

GCGLenum RemoteGraphicsContextGLProxy::getError()
{
    if (!isContextLost()) {
        uint32_t returnValue = 0;
        auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::GetError(), Messages::RemoteGraphicsContextGL::GetError::Reply(returnValue));
        if (!sendResult)
            wasLost();
        return static_cast<GCGLenum>(returnValue);
    }
    return std::exchange(m_errorWhenContextIsLost, NO_ERROR);
}

void RemoteGraphicsContextGLProxy::wasCreated(bool didSucceed, IPC::Semaphore&& semaphore, String&& availableExtensions, String&& requestedExtensions)
{
    if (isContextLost())
        return;
    if (!didSucceed) {
        markContextLost();
        return;
    }
    ASSERT(!m_didInitialize);
    m_streamConnection.setWakeUpSemaphore(WTFMove(semaphore));
    m_didInitialize = true;
    initialize(availableExtensions, requestedExtensions);
}

void RemoteGraphicsContextGLProxy::wasLost()
{
    if (isContextLost())
        return;
    markContextLost();
}

void RemoteGraphicsContextGLProxy::wasChanged()
{
    if (isContextLost())
        return;
    for (auto* client : copyToVector(m_clients))
        client->dispatchContextChangedNotification();
}

void RemoteGraphicsContextGLProxy::markContextLost()
{
    disconnectGpuProcessIfNeeded();
    for (auto* client : copyToVector(m_clients))
        client->forceContextLost();
}

void RemoteGraphicsContextGLProxy::waitUntilInitialized()
{
    if (isContextLost())
        return;
    if (m_didInitialize)
        return;
    if (connection().waitForAndDispatchImmediately<Messages::RemoteGraphicsContextGLProxy::WasCreated>(m_graphicsContextGLIdentifier, defaultSendTimeout))
        return;
    markContextLost();
}

void RemoteGraphicsContextGLProxy::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    ASSERT(!isContextLost());
    abandonGpuProcess();
    markContextLost();
}

void RemoteGraphicsContextGLProxy::abandonGpuProcess()
{
    auto gpuProcessConnection = std::exchange(m_gpuProcessConnection, nullptr);
    gpuProcessConnection->removeClient(*this);
    gpuProcessConnection->messageReceiverMap().removeMessageReceiver(Messages::RemoteGraphicsContextGLProxy::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64());
    m_gpuProcessConnection = nullptr;
}

void RemoteGraphicsContextGLProxy::disconnectGpuProcessIfNeeded()
{
    if (auto gpuProcessConnection = std::exchange(m_gpuProcessConnection, nullptr)) {
        gpuProcessConnection->removeClient(*this);
        gpuProcessConnection->messageReceiverMap().removeMessageReceiver(Messages::RemoteGraphicsContextGLProxy::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64());
        gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::ReleaseGraphicsContextGL(m_graphicsContextGLIdentifier), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    }
    ASSERT(isContextLost());
}

} // namespace WebKit

#endif
