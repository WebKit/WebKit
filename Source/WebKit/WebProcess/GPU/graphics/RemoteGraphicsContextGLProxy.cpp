/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include "RemoteGraphicsContextGLInitializationState.h"
#include "RemoteGraphicsContextGLMessages.h"
#include "RemoteGraphicsContextGLProxyMessages.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "WebProcess.h"
#include <WebCore/BitmapImage.h>
#include <WebCore/GCGLSpan.h>
#include <WebCore/ImageBuffer.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "RemoteVideoFrameProxy.h"
#include <WebCore/MediaPlayer.h>
#endif

namespace WebKit {

using namespace WebCore;

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

RefPtr<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::create(IPC::Connection& connection, const WebCore::GraphicsContextGLAttributes& attributes, RemoteRenderingBackendProxy& renderingBackend
#if ENABLE(VIDEO)
    , Ref<RemoteVideoFrameObjectHeapProxy>&& videoFrameObjectHeapProxy
#endif
    )
{
    constexpr unsigned defaultConnectionBufferSizeLog2 = 21;
    unsigned connectionBufferSizeLog2 = defaultConnectionBufferSizeLog2;
    if (attributes.remoteIPCBufferSizeLog2ForTesting)
        connectionBufferSizeLog2 = attributes.remoteIPCBufferSizeLog2ForTesting;
    auto [clientConnection, serverConnectionHandle] = IPC::StreamClientConnection::create(connectionBufferSizeLog2);
    if (!clientConnection)
        return nullptr;
    auto instance = platformCreate(connection, clientConnection.releaseNonNull(), attributes
#if ENABLE(VIDEO)
        , WTFMove(videoFrameObjectHeapProxy)
#endif
    );
    instance->initializeIPC(WTFMove(serverConnectionHandle), renderingBackend);
    return instance;
}


RemoteGraphicsContextGLProxy::RemoteGraphicsContextGLProxy(IPC::Connection& connection, RefPtr<IPC::StreamClientConnection> streamConnection, const GraphicsContextGLAttributes& attributes
#if ENABLE(VIDEO)
    , Ref<RemoteVideoFrameObjectHeapProxy>&& videoFrameObjectHeapProxy
#endif
    )
    : GraphicsContextGL(attributes)
    , m_connection(&connection) // NOLINT
    , m_streamConnection(WTFMove(streamConnection)) // NOLINT
#if ENABLE(VIDEO)
    , m_videoFrameObjectHeapProxy(WTFMove(videoFrameObjectHeapProxy))
#endif
{
}

void RemoteGraphicsContextGLProxy::initializeIPC(IPC::StreamServerConnection::Handle&& serverConnectionHandle, RemoteRenderingBackendProxy& renderingBackend)
{
    m_connection->send(Messages::GPUConnectionToWebProcess::CreateGraphicsContextGL(contextAttributes(), m_graphicsContextGLIdentifier, renderingBackend.ensureBackendCreated(), WTFMove(serverConnectionHandle)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    m_streamConnection->open(*this, renderingBackend.dispatcher());
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
    return contextAttributes().webGLVersion == GraphicsContextGLWebGLVersion::WebGL2;
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

void RemoteGraphicsContextGLProxy::initialize(const RemoteGraphicsContextGLInitializationState& initializationState)
{
    for (auto extension : StringView(initializationState.availableExtensions).split(' '))
        m_availableExtensions.add(extension.toString());
    for (auto extension : StringView(initializationState.requestableExtensions).split(' '))
        m_requestableExtensions.add(extension.toString());
    m_externalImageTarget = initializationState.externalImageTarget;
    m_externalImageBindingQuery = initializationState.externalImageBindingQuery;
}

std::optional<GraphicsContextGL::ExternalImageAttachResult> RemoteGraphicsContextGLProxy::createAndBindExternalImage(GCGLenum, GraphicsContextGL::ExternalImageSource)
{
    notImplemented();
    return { };
}

GCEGLSync RemoteGraphicsContextGLProxy::createEGLSync(ExternalEGLSyncEvent)
{
    notImplemented();
    return { };
}

std::tuple<GCGLenum, GCGLenum> RemoteGraphicsContextGLProxy::externalImageTextureBindingPoint()
{
    if (isContextLost())
        return std::make_tuple(0, 0);

    return std::make_tuple(m_externalImageTarget, m_externalImageBindingQuery);
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

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
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
    auto videoFrame = mediaPlayer.videoFrameForCurrentTime();
    if (!videoFrame)
        return false;

    return copyTextureFromVideoFrame(*videoFrame, texture, target, level, internalFormat, format, type, premultiplyAlpha, flipY);
}

bool RemoteGraphicsContextGLProxy::copyTextureFromVideoFrame(WebCore::VideoFrame& videoFrame, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY)
{
#if PLATFORM(COCOA)
    if (isContextLost())
        return false;

    auto sharedVideoFrame = m_sharedVideoFrameWriter.write(videoFrame, [this](auto& semaphore) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::SetSharedVideoFrameSemaphore { semaphore });
        if (!sendResult)
            markContextLost();
    }, [this](auto&& handle) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::SetSharedVideoFrameMemory { WTFMove(handle) });
        if (!sendResult)
            markContextLost();
    });
    if (!sharedVideoFrame || isContextLost())
        return false;

    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::CopyTextureFromVideoFrame(*sharedVideoFrame, texture, target, level, internalFormat, format, type, premultiplyAlpha, flipY));
    if (!sendResult) {
        markContextLost();
        return false;
    }

    auto [result] = sendResult.takeReply();
    return result;
#else
    return false;
#endif
}

RefPtr<Image> RemoteGraphicsContextGLProxy::videoFrameToImage(WebCore::VideoFrame& frame)
{
    if (isContextLost())
        return { };

    RefPtr<NativeImage> nativeImage;
#if PLATFORM(COCOA)
    callOnMainRunLoopAndWait([&] {
        nativeImage = m_videoFrameObjectHeapProxy->getNativeImage(frame);
    });
#endif
    return BitmapImage::create(WTFMove(nativeImage));
}
#endif

GCGLErrorCodeSet RemoteGraphicsContextGLProxy::getErrors()
{
    if (!isContextLost()) {
        auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::GetErrors());
        if (!sendResult)
            markContextLost();
        auto [returnValue] = sendResult.takeReplyOr(GCGLErrorCodeSet { });
        return returnValue;
    }
    return { };
}

void RemoteGraphicsContextGLProxy::simulateEventForTesting(SimulatedEventForTesting event)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::SimulateEventForTesting(event));
        if (!sendResult)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::readPixels(IntRect rect, GCGLenum format, GCGLenum type, std::span<uint8_t> data)
{
    if (isContextLost())
        return;
    if (data.size() > readPixelsInlineSizeLimit) {
        readPixelsSharedMemory(rect, format, type, data);
        return;
    }
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::ReadPixelsInline(rect, format, type, IPC::ArrayReference<uint8_t>(reinterpret_cast<uint8_t*>(data.data()), data.size())));
    if (!sendResult) {
        markContextLost();
        return;
    }
    auto [dataReply] = sendResult.takeReply();
    memcpy(data.data(), dataReply.data(), data.size());
}

void RemoteGraphicsContextGLProxy::readPixelsSharedMemory(IntRect rect, GCGLenum format, GCGLenum type, std::span<uint8_t> data)
{
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
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::ReadPixelsSharedMemory(rect, format, type, WTFMove(*handle)));
    if (!sendResult) {
        markContextLost();
        return;
    }
    auto [success] = sendResult.takeReply();
    if (!success)
        return;
    memcpy(data.data(), buffer->data(), data.size());
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

void RemoteGraphicsContextGLProxy::wasCreated(IPC::Semaphore&& wakeUpSemaphore, IPC::Semaphore&& clientWaitSemaphore, std::optional<RemoteGraphicsContextGLInitializationState>&& initializationState)
{
    if (isContextLost())
        return;
    if (!initializationState) {
        markContextLost();
        return;
    }
    ASSERT(!m_didInitialize);
    m_streamConnection->setSemaphores(WTFMove(wakeUpSemaphore), WTFMove(clientWaitSemaphore));
    m_didInitialize = true;
    initialize(initializationState.value());
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
#if PLATFORM(COCOA)
    m_sharedVideoFrameWriter.disable();
#endif
    if (m_connection) {
        m_streamConnection->invalidate();
        m_connection->send(Messages::GPUConnectionToWebProcess::ReleaseGraphicsContextGL(m_graphicsContextGLIdentifier), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
        m_connection = nullptr;
    }
    ASSERT(isContextLost());
}

} // namespace WebKit

#endif
