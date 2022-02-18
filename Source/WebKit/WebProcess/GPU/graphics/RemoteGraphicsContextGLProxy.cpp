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

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "RemoteVideoFrameProxy.h"
#endif

namespace WebKit {

using namespace WebCore;

static constexpr size_t defaultStreamSize = 1 << 21;

RemoteGraphicsContextGLProxy::RemoteGraphicsContextGLProxy(GPUProcessConnection& gpuProcessConnection, const GraphicsContextGLAttributes& attributes, RenderingBackendIdentifier renderingBackend)
    : GraphicsContextGL(attributes)
    , m_gpuProcessConnection(&gpuProcessConnection)
    , m_streamConnection(gpuProcessConnection.connection(), defaultStreamSize)
{
    m_gpuProcessConnection->addClient(*this);
    m_gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::RemoteGraphicsContextGLProxy::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64(), *this);
    connection().send(Messages::GPUConnectionToWebProcess::CreateGraphicsContextGL(attributes, m_graphicsContextGLIdentifier, renderingBackend, m_streamConnection.streamBuffer()), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    // TODO: We must wait until initialized, because at the moment we cannot receive IPC messages
    // during wait while in synchronous stream send. Should be fixed as part of https://bugs.webkit.org/show_bug.cgi?id=217211.
    waitUntilInitialized();
}

RemoteGraphicsContextGLProxy::~RemoteGraphicsContextGLProxy()
{
    disconnectGpuProcessIfNeeded();
}

void RemoteGraphicsContextGLProxy::setContextVisibility(bool)
{
    notImplemented();
}

bool RemoteGraphicsContextGLProxy::isGLES2Compliant() const
{
#if ENABLE(WEBGL2)
    return contextAttributes().webGLVersion == GraphicsContextGLWebGLVersion::WebGL2;
#else
    return false;
#endif
}

void RemoteGraphicsContextGLProxy::markContextChanged()
{
    // FIXME: The caller should track this state.
    if (m_layerComposited) {
        GraphicsContextGL::markContextChanged();
        if (isContextLost())
            return;
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MarkContextChanged());
        if (!sendResult)
            markContextLost();
    }
}

bool RemoteGraphicsContextGLProxy::supportsExtension(const String& name)
{
    waitUntilInitialized();
    return m_availableExtensions.contains(name) || m_requestableExtensions.contains(name);
}

void RemoteGraphicsContextGLProxy::ensureExtensionEnabled(const String& name)
{
    waitUntilInitialized();
    if (m_requestableExtensions.contains(name) && !m_enabledExtensions.contains(name)) {
        m_enabledExtensions.add(name);
        if (isContextLost())
            return;
        auto sendResult = send(Messages::RemoteGraphicsContextGL::EnsureExtensionEnabled(name));
        if (!sendResult)
            markContextLost();
    }
}

bool RemoteGraphicsContextGLProxy::isExtensionEnabled(const String& name)
{
    waitUntilInitialized();
    return m_availableExtensions.contains(name) || m_enabledExtensions.contains(name);
}

void RemoteGraphicsContextGLProxy::initialize(const String& availableExtensions, const String& requestableExtensions)
{
    for (auto& extension : availableExtensions.split(' '))
        m_availableExtensions.add(extension);
    for (auto& extension : requestableExtensions.split(' '))
        m_requestableExtensions.add(extension);
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

void RemoteGraphicsContextGLProxy::paintRenderingResultsToCanvas(ImageBuffer& buffer)
{
    // FIXME: the buffer is "relatively empty" always, but for consistency, we need to ensure
    // no pending operations are targeted for the `buffer`.
    buffer.flushDrawingContext();

    // FIXME: We cannot synchronize so that we would know no pending operations are using the `buffer`.

    // FIXME: Currently RemoteImageBufferProxy::getPixelBuffer et al do not wait for the flushes of the images
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

#if ENABLE(MEDIA_STREAM)
RefPtr<WebCore::MediaSample> RemoteGraphicsContextGLProxy::paintCompositedResultsToMediaSample()
{
    if (isContextLost())
        return nullptr;
    std::optional<RemoteVideoFrameProxy::Properties> result;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PaintCompositedResultsToMediaSample(), Messages::RemoteGraphicsContextGL::PaintCompositedResultsToMediaSample::Reply(result));
    if (!sendResult) {
        markContextLost();
        return nullptr;
    }
    if (!result)
        return nullptr;
    return RemoteVideoFrameProxy::create(m_gpuProcessConnection->connection(), m_gpuProcessConnection->videoFrameObjectHeapProxy(), WTFMove(*result));
}
#endif

#if ENABLE(VIDEO)
bool RemoteGraphicsContextGLProxy::copyTextureFromMedia(MediaPlayer& mediaPlayer, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY)
{
    auto videoFrame = mediaPlayer.videoFrameForCurrentTime();
    // Video in WP while WebGL in GPUP is not supported.
    if (!videoFrame || !is<RemoteVideoFrameProxy>(*videoFrame))
        return false;
    bool result = false;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::CopyTextureFromVideoFrame(downcast<RemoteVideoFrameProxy>(*videoFrame).read(), texture, target, level, internalFormat, format, type, premultiplyAlpha, flipY), Messages::RemoteGraphicsContextGL::CopyTextureFromVideoFrame::Reply(result));
    if (!sendResult) {
        markContextLost();
        return false;
    }

    return result;
}
#endif

void RemoteGraphicsContextGLProxy::synthesizeGLError(GCGLenum error)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::SynthesizeGLError(error));
        if (!sendResult)
            markContextLost();
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
            markContextLost();
        return static_cast<GCGLenum>(returnValue);
    }
    return std::exchange(m_errorWhenContextIsLost, NO_ERROR);
}

void RemoteGraphicsContextGLProxy::simulateEventForTesting(SimulatedEventForTesting event)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::SimulateEventForTesting(event));
        if (!sendResult)
            markContextLost();
    }
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

bool RemoteGraphicsContextGLProxy::handleMessageToRemovedDestination(IPC::Connection&, IPC::Decoder& decoder)
{
    // Skip messages intended for already removed messageReceiverMap() destinations.
    // These are business as usual. These can happen for example by:
    //  - The object is created and immediately destroyed, before WasCreated was handled.
    //  - A send to GPU process times out, we remove the object. If the GPU process delivers the message at the same
    //    time, it might be in the message delivery callback.
    // When adding new messages to RemoteGraphicsContextGLProxy, add them to this list.
    ASSERT(decoder.messageName() == Messages::RemoteGraphicsContextGLProxy::WasCreated::name()
        || decoder.messageName() == Messages::RemoteGraphicsContextGLProxy::WasLost::name()
        || decoder.messageName() == Messages::RemoteGraphicsContextGLProxy::WasChanged::name());
    return true;
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
