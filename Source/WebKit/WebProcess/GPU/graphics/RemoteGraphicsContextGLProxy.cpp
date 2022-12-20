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
#include <wtf/StdLibExtras.h>

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "RemoteVideoFrameProxy.h"
#include <WebCore/MediaPlayer.h>
#endif

namespace WebKit {

using namespace WebCore;

static constexpr size_t defaultStreamSize = 1 << 21;
static constexpr size_t readPixelsInlineSizeLimit = 64 * KB;

namespace {

template<typename... Types, typename... SpanTupleTypes, size_t... Indices>
IPC::ArrayReferenceTuple<Types...> toArrayReferenceTuple(const GCGLSpanTuple<SpanTupleTypes...>& spanTuple, std::index_sequence<Indices...>)
{
    return IPC::ArrayReferenceTuple<Types...> {
        reinterpret_cast<const std::tuple_element_t<Indices, std::tuple<Types...>>*>(spanTuple.template data<Indices>())...,
        spanTuple.bufSize };
}

template<typename... Types, typename... SpanTupleTypes>
IPC::ArrayReferenceTuple<Types...> toArrayReferenceTuple(const GCGLSpanTuple<SpanTupleTypes...>& spanTuple)
{
    static_assert(sizeof...(Types) == sizeof...(SpanTupleTypes));
    return toArrayReferenceTuple<Types...>(spanTuple, std::index_sequence_for<Types...> { });
}

}

RemoteGraphicsContextGLProxy::RemoteGraphicsContextGLProxy(IPC::Connection& connection, SerialFunctionDispatcher& dispatcher, const GraphicsContextGLAttributes& attributes, RenderingBackendIdentifier renderingBackend)
    : GraphicsContextGL(attributes)
{
    auto [clientConnection, serverConnectionHandle] = IPC::StreamClientConnection::create(defaultStreamSize);
    m_streamConnection = WTFMove(clientConnection);
    m_connection = &connection;
    m_connection->send(Messages::GPUConnectionToWebProcess::CreateGraphicsContextGL(attributes, m_graphicsContextGLIdentifier, renderingBackend, WTFMove(serverConnectionHandle)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    m_streamConnection->open(*this, dispatcher);
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
    for (auto extension : StringView(availableExtensions).split(' '))
        m_availableExtensions.add(extension.toString());
    for (auto extension : StringView(requestableExtensions).split(' '))
        m_requestableExtensions.add(extension.toString());
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
    if (isContextLost())
        return;
    // FIXME: the buffer is "relatively empty" always, but for consistency, we need to ensure
    // no pending operations are targeted for the `buffer`.
    buffer.flushDrawingContext();

    // FIXME: We cannot synchronize so that we would know no pending operations are using the `buffer`.

    // FIXME: Currently RemoteImageBufferProxy::getPixelBuffer et al do not wait for the flushes of the images
    // inside the display lists. Rather, it assumes that processing its sequence (e.g. the display list) will equal to read flush being
    // fulfilled. For below, we cannot create a new flush id since we go through different sequence (RemoteGraphicsContextGL sequence)

    // FIXME: Maybe implement IPC::Fence or something similar.
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PaintRenderingResultsToCanvas(buffer.renderingResourceIdentifier()));
    if (!sendResult) {
        markContextLost();
        return;
    }
}

void RemoteGraphicsContextGLProxy::paintCompositedResultsToCanvas(ImageBuffer& buffer)
{
    if (isContextLost())
        return;
    buffer.flushDrawingContext();
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PaintCompositedResultsToCanvas(buffer.renderingResourceIdentifier()));
    if (!sendResult) {
        markContextLost();
        return;
    }
}

#if ENABLE(MEDIA_STREAM)
RefPtr<WebCore::VideoFrame> RemoteGraphicsContextGLProxy::paintCompositedResultsToVideoFrame()
{
    ASSERT(isMainRunLoop());
    if (isContextLost())
        return nullptr;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PaintCompositedResultsToVideoFrame());
    if (!sendResult) {
        markContextLost();
        return nullptr;
    }
    auto [result] = sendResult.takeReply();
    if (!result)
        return nullptr;
    return RemoteVideoFrameProxy::create(WebProcess::singleton().ensureGPUProcessConnection().connection(), WebProcess::singleton().ensureGPUProcessConnection().videoFrameObjectHeapProxy(), WTFMove(*result));
}
#endif

#if ENABLE(VIDEO)
bool RemoteGraphicsContextGLProxy::copyTextureFromMedia(MediaPlayer& mediaPlayer, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY)
{
    if (isContextLost())
        return false;
    auto videoFrame = mediaPlayer.videoFrameForCurrentTime();
    // Video in WP while WebGL in GPUP is not supported.
    if (!videoFrame || !is<RemoteVideoFrameProxy>(*videoFrame))
        return false;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::CopyTextureFromVideoFrame(downcast<RemoteVideoFrameProxy>(*videoFrame).newReadReference(), texture, target, level, internalFormat, format, type, premultiplyAlpha, flipY));
    if (!sendResult) {
        markContextLost();
        return false;
    }

    auto [result] = sendResult.takeReply();
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
}

GCGLenum RemoteGraphicsContextGLProxy::getError()
{
    if (!isContextLost()) {
        auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::GetError());
        if (!sendResult)
            markContextLost();
        auto [returnValue] = sendResult.takeReplyOr(0);
        return static_cast<GCGLenum>(returnValue);
    }
    return NO_ERROR;
}

void RemoteGraphicsContextGLProxy::simulateEventForTesting(SimulatedEventForTesting event)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::SimulateEventForTesting(event));
        if (!sendResult)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::readnPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<GCGLvoid> data)
{
    if (data.size() > readPixelsInlineSizeLimit) {
        readnPixelsSharedMemory(x, y, width, height, format, type, data);
        return;
    }

    if (!isContextLost()) {
        auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::ReadnPixels0(x, y, width, height, format, type, IPC::ArrayReference<uint8_t>(reinterpret_cast<uint8_t*>(data.data()), data.size())));
        if (sendResult) {
            auto [dataReply] = sendResult.takeReply();
            memcpy(data.data(), dataReply.data(), data.size());
        } else
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::readnPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::ReadnPixels1(x, y, width, height, format, type, static_cast<uint64_t>(offset)));
        if (!sendResult)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::readnPixelsSharedMemory(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<GCGLvoid> data)
{
    if (!isContextLost()) {
        auto buffer = SharedMemory::allocate(data.size());
        if (!buffer) {
            markContextLost();
            return;
        }
        auto handle = buffer->createHandle(SharedMemory::Protection::ReadWrite);
        if (!handle || handle->isNull()) {
            markContextLost();
            return;
        }
        memcpy(buffer->data(), data.data(), data.size());
        auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::ReadnPixels2(x, y, width, height, format, type, WTFMove(*handle)));
        if (sendResult) {
            auto [success] = sendResult.takeReply();
            if (success)
                memcpy(data.data(), buffer->data(), data.size());
        } else
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::multiDrawArraysANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei> firstsAndCounts)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawArraysANGLE(mode, toArrayReferenceTuple<int32_t, int32_t>(firstsAndCounts)));
        if (!sendResult)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::multiDrawArraysInstancedANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei, const GCGLsizei> firstsCountsAndInstanceCounts)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawArraysInstancedANGLE(mode, toArrayReferenceTuple<int32_t, int32_t, int32_t>(firstsCountsAndInstanceCounts)));
        if (!sendResult)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::multiDrawElementsANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei> countsAndOffsets, GCGLenum type)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawElementsANGLE(mode, toArrayReferenceTuple<int32_t, int32_t>(countsAndOffsets), type));
        if (!sendResult)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::multiDrawElementsInstancedANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei, const GCGLsizei> countsOffsetsAndInstanceCounts, GCGLenum type)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawElementsInstancedANGLE(mode, toArrayReferenceTuple<int32_t, int32_t, int32_t>(countsOffsetsAndInstanceCounts), type));
        if (!sendResult)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::multiDrawArraysInstancedBaseInstanceANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei, const GCGLsizei, const GCGLuint> firstsCountsInstanceCountsAndBaseInstances)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawArraysInstancedBaseInstanceANGLE(mode, toArrayReferenceTuple<int32_t, int32_t, int32_t, uint32_t>(firstsCountsInstanceCountsAndBaseInstances)));
        if (!sendResult)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei, const GCGLsizei, const GCGLint, const GCGLuint> countsOffsetsInstanceCountsBaseVerticesAndBaseInstances, GCGLenum type)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawElementsInstancedBaseVertexBaseInstanceANGLE(mode, toArrayReferenceTuple<int32_t, int32_t, int32_t, int32_t, uint32_t>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances), type));
        if (!sendResult)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::wasCreated(bool didSucceed, IPC::Semaphore&& wakeUpSemaphore, IPC::Semaphore&& clientWaitSemaphore, String&& availableExtensions, String&& requestedExtensions)
{
    if (isContextLost())
        return;
    if (!didSucceed) {
        markContextLost();
        return;
    }
    ASSERT(!m_didInitialize);
    m_streamConnection->setSemaphores(WTFMove(wakeUpSemaphore), WTFMove(clientWaitSemaphore));
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
    dispatchContextChangedNotification();
}

void RemoteGraphicsContextGLProxy::markContextLost()
{
    disconnectGpuProcessIfNeeded();
    forceContextLost();
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
    if (m_streamConnection->waitForAndDispatchImmediately<Messages::RemoteGraphicsContextGLProxy::WasCreated>(m_graphicsContextGLIdentifier, defaultSendTimeout))
        return;
    markContextLost();
}

void RemoteGraphicsContextGLProxy::didClose(IPC::Connection&)
{
    ASSERT(!isContextLost());
    abandonGpuProcess();
    markContextLost();
}

void RemoteGraphicsContextGLProxy::abandonGpuProcess()
{
    m_connection = nullptr;
}

void RemoteGraphicsContextGLProxy::disconnectGpuProcessIfNeeded()
{
    if (m_connection) {
        m_streamConnection->invalidate();
        m_connection->send(Messages::GPUConnectionToWebProcess::ReleaseGraphicsContextGL(m_graphicsContextGLIdentifier), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
        m_connection = nullptr;
    }
    ASSERT(isContextLost());
}

} // namespace WebKit

#endif
