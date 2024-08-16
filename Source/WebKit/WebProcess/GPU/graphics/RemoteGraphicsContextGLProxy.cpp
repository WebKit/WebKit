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
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/BitmapImage.h>
#include <WebCore/GCGLSpan.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/PixelBufferConversion.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "RemoteVideoFrameProxy.h"
#include <WebCore/MediaPlayer.h>
#endif

namespace WebKit {

using namespace WebCore;

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

RefPtr<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::create(const WebCore::GraphicsContextGLAttributes& attributes, WebPage& page)
{
    return RemoteGraphicsContextGLProxy::create(attributes, page.ensureRemoteRenderingBackendProxy(), RunLoop::main());
}

RefPtr<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::create(const WebCore::GraphicsContextGLAttributes& attributes, RemoteRenderingBackendProxy& renderingBackend, SerialFunctionDispatcher& dispatcher)
{
    constexpr unsigned defaultConnectionBufferSizeLog2 = 21;
    unsigned connectionBufferSizeLog2 = defaultConnectionBufferSizeLog2;
    if (attributes.failContextCreationForTesting == WebCore::GraphicsContextGLAttributes::SimulatedCreationFailure::IPCBufferOOM)
        connectionBufferSizeLog2 = 50; // Expect this to fail.
    auto connectionPair = IPC::StreamClientConnection::create(connectionBufferSizeLog2, WebProcess::singleton().gpuProcessTimeoutDuration());
    if (!connectionPair)
        return nullptr;
    auto [clientConnection, serverConnectionHandle] = WTFMove(*connectionPair);
    Ref instance = platformCreate(attributes, dispatcher);
    instance->initializeIPC(WTFMove(clientConnection), renderingBackend.ensureBackendCreated(), WTFMove(serverConnectionHandle));
    if (attributes.failContextCreationForTesting == WebCore::GraphicsContextGLAttributes::SimulatedCreationFailure::CreationTimeout)
        instance->markContextLost();
    // TODO: We must wait until initialized, because at the moment we cannot receive IPC messages
    // during wait while in synchronous stream send. Should be fixed as part of https://bugs.webkit.org/show_bug.cgi?id=217211.
    instance->waitUntilInitialized();
    return instance;
}

RemoteGraphicsContextGLProxy::RemoteGraphicsContextGLProxy(const GraphicsContextGLAttributes& attributes, SerialFunctionDispatcher& dispatcher)
    : GraphicsContextGL(attributes)
    , m_dispatcher(dispatcher)
{
}

RemoteGraphicsContextGLProxy::~RemoteGraphicsContextGLProxy()
{
    disconnectGpuProcessIfNeeded();
}

void RemoteGraphicsContextGLProxy::initializeIPC(Ref<IPC::StreamClientConnection>&& streamConnection, RenderingBackendIdentifier renderingBackend, IPC::StreamServerConnection::Handle&& serverHandle)
{
    m_streamConnection = WTFMove(streamConnection);
    m_streamConnection->open(*this, *this);
    callOnMainRunLoopAndWait([&]() {
        auto& gpuProcessConnection = WebProcess::singleton().ensureGPUProcessConnection();
        gpuProcessConnection.createGraphicsContextGL(m_identifier, contextAttributes(), renderingBackend, WTFMove(serverHandle));
        m_gpuProcessConnection = gpuProcessConnection;
#if ENABLE(VIDEO)
        m_videoFrameObjectHeapProxy = &gpuProcessConnection.videoFrameObjectHeapProxy();
#endif
    });

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
        if (sendResult != IPC::Error::NoError)
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
    if (sendResult != IPC::Error::NoError)
        markContextLost();
}

void RemoteGraphicsContextGLProxy::drawSurfaceBufferToImageBuffer(SurfaceBuffer buffer, ImageBuffer& imageBuffer)
{
    if (isContextLost())
        return;
    imageBuffer.flushDrawingContext();
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::DrawSurfaceBufferToImageBuffer(buffer, imageBuffer.renderingResourceIdentifier()));
    if (!sendResult.succeeded()) {
        markContextLost();
        return;
    }
}

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
RefPtr<WebCore::VideoFrame> RemoteGraphicsContextGLProxy::surfaceBufferToVideoFrame(SurfaceBuffer buffer)
{
    ASSERT(isMainRunLoop());
    if (isContextLost())
        return nullptr;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::SurfaceBufferToVideoFrame(buffer));
    if (!sendResult.succeeded()) {
        markContextLost();
        return nullptr;
    }
    auto [result] = sendResult.takeReply();
    if (!result)
        return nullptr;
    return RemoteVideoFrameProxy::create(WebProcess::singleton().ensureGPUProcessConnection().protectedConnection(), WebProcess::singleton().ensureGPUProcessConnection().videoFrameObjectHeapProxy(), WTFMove(*result));
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
        if (sendResult != IPC::Error::NoError)
            markContextLost();
    }, [this](SharedMemory::Handle&& handle) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::SetSharedVideoFrameMemory { WTFMove(handle) });
        if (sendResult != IPC::Error::NoError)
            markContextLost();
    });
    if (!sharedVideoFrame || isContextLost())
        return false;

    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::CopyTextureFromVideoFrame(WTFMove(*sharedVideoFrame), texture, target, level, internalFormat, format, type, premultiplyAlpha, flipY));
    if (!sendResult.succeeded()) {
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
        if (!sendResult.succeeded())
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
        if (sendResult != IPC::Error::NoError)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::getBufferSubData(GCGLenum target, GCGLintptr offset, std::span<uint8_t> data)
{
    if (isContextLost())
        return;

    if (data.empty())
        return;

    static constexpr size_t getBufferSubDataInlineSizeLimit = 64 * KB; // NOTE: when changing, change the value in RemoteGraphicsContextGL too.
    static constexpr size_t getBufferSubDataSharedMemorySizeLimit = 100 * MB;

    if (data.size() > getBufferSubDataInlineSizeLimit) {
        RefPtr<SharedMemory> replyBuffer = SharedMemory::allocate(std::min(data.size(), getBufferSubDataSharedMemorySizeLimit));
        if (!replyBuffer)
            goto inlineCase;
        while (!data.empty()) {
            auto handle = replyBuffer->createHandle(SharedMemory::Protection::ReadWrite);
            if (!handle)
                goto inlineCase;
            auto transferSize = std::min(data.size(), getBufferSubDataSharedMemorySizeLimit);
            auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::GetBufferSubDataSharedMemory(target, offset, transferSize, WTFMove(*handle)));
            if (!sendResult.succeeded()) {
                markContextLost();
                return;
            }
            auto [valid] = sendResult.takeReply();
            if (!valid)
                return;
            std::ranges::copy(replyBuffer->span().subspan(0, transferSize), data.begin());
            data = data.subspan(transferSize);
            offset += transferSize;
        }
        return;
    }
inlineCase:
    while (data.size()) {
        auto transferSize = std::min(data.size(), getBufferSubDataInlineSizeLimit);
        auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::GetBufferSubDataInline(target, offset, transferSize));
        if (!sendResult.succeeded()) {
            markContextLost();
            return;
        }
        auto [inlineBuffer] = sendResult.takeReply();
        if (inlineBuffer.empty())
            return;
        RELEASE_ASSERT(transferSize == inlineBuffer.size());
        std::ranges::copy(inlineBuffer, data.begin());
        data = data.subspan(transferSize);
        offset += transferSize;
    }
}

void RemoteGraphicsContextGLProxy::readPixels(IntRect rect, GCGLenum format, GCGLenum type, std::span<uint8_t> dataStore, GCGLint alignment, GCGLint rowLength, GCGLboolean packReverseRowOrder)
{
    if (isContextLost())
        return;
    // The structure of `dataStore` is defined by rect, format, type, alignment, rowLength.
    // We know that `rect.position() < { 0, 0 }` is out of bounds, and thus should not be touched.
    // ANGLE will return which of the pixels > 0 are out of bounds, and thus should not be touched.
    // Alignment, rowLength - width should not be touched.
    // Each `dataStore` row is [oob1*][image][oob2*][remaining rowLength*][alignment*] == dataStoreRowBytes.
    // GPUP reply row is [image][oob2*].
    // The calculations here are all non-overflowing, as the caller has validated that.

    unsigned bytesPerGroup = computeBytesPerGroup(format, type);
    unsigned dataStoreWidth = rowLength > 0 ? rowLength : rect.width();
    unsigned dataStoreRowBytes = roundUpToMultipleOf(alignment, dataStoreWidth * bytesPerGroup);

    IntSize bottomLeftOutOfBounds;
    if (rect.x() < 0) {
        bottomLeftOutOfBounds.setWidth(-rect.x());
        rect.shiftXEdgeTo(0);
    }
    if (rect.y() < 0) {
        bottomLeftOutOfBounds.setHeight(-rect.y());
        rect.shiftYEdgeTo(0);
    }

    unsigned replyRowBytes = rect.width() * bytesPerGroup;
    unsigned replyImageBytes = rect.height() * replyRowBytes;

    if (!rect.isEmpty() && !bottomLeftOutOfBounds.isZero()) {
        // Will not overflow, because rect.size() * { dataStoreRowBytes, bytesPerGroup } is validated and it will fit to the uint32_t.
        // bottomLeftOutOfBounds must be smaller than rect.size() in case adjusted rect is non-empty.
        unsigned skipRowBytes = bottomLeftOutOfBounds.width() * bytesPerGroup;
        dataStore = dataStore.subspan(dataStoreRowBytes * bottomLeftOutOfBounds.height() + skipRowBytes);
    }

    static constexpr size_t readPixelsInlineSizeLimit = 64 * KB; // NOTE: when changing, change the value in RemoteGraphicsContextGL too.

    auto copyToData = [&](std::span<const uint8_t> replyData, IntSize readArea) {
        if (readArea.isEmpty())
            return;
        unsigned copyRowBytes = readArea.width() * bytesPerGroup;
        copyRows(replyRowBytes, replyData, dataStoreRowBytes, dataStore, readArea.height(), copyRowBytes);
    };

    if (replyImageBytes > readPixelsInlineSizeLimit) {
        RefPtr<SharedMemory> replyBuffer = SharedMemory::allocate(replyImageBytes);
        if (!replyBuffer)
            goto inlineCase;
        auto handle = replyBuffer->createHandle(SharedMemory::Protection::ReadWrite);
        if (!handle)
            goto inlineCase;
        auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::ReadPixelsSharedMemory(rect, format, type, packReverseRowOrder, WTFMove(*handle)));
        if (!sendResult.succeeded()) {
            markContextLost();
            return;
        }
        auto [readArea] = sendResult.takeReply();
        if (!readArea)
            return;
        copyToData(replyBuffer->span(), *readArea);
        return;
    }
inlineCase:
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::ReadPixelsInline(rect, format, type, packReverseRowOrder));
    if (!sendResult.succeeded()) {
        markContextLost();
        return;
    }
    auto [readArea, inlineReply] = sendResult.takeReply();
    if (!readArea)
        return;
    copyToData(inlineReply, *readArea);
}

void RemoteGraphicsContextGLProxy::multiDrawArraysANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei> firstsAndCounts)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawArraysANGLE(mode, toArrayReferenceTuple<int32_t, int32_t>(firstsAndCounts)));
        if (sendResult != IPC::Error::NoError)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::multiDrawArraysInstancedANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei, const GCGLsizei> firstsCountsAndInstanceCounts)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawArraysInstancedANGLE(mode, toArrayReferenceTuple<int32_t, int32_t, int32_t>(firstsCountsAndInstanceCounts)));
        if (sendResult != IPC::Error::NoError)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::multiDrawElementsANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei> countsAndOffsets, GCGLenum type)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawElementsANGLE(mode, toArrayReferenceTuple<int32_t, int32_t>(countsAndOffsets), type));
        if (sendResult != IPC::Error::NoError)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::multiDrawElementsInstancedANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei, const GCGLsizei> countsOffsetsAndInstanceCounts, GCGLenum type)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawElementsInstancedANGLE(mode, toArrayReferenceTuple<int32_t, int32_t, int32_t>(countsOffsetsAndInstanceCounts), type));
        if (sendResult != IPC::Error::NoError)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::multiDrawArraysInstancedBaseInstanceANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei, const GCGLsizei, const GCGLuint> firstsCountsInstanceCountsAndBaseInstances)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawArraysInstancedBaseInstanceANGLE(mode, toArrayReferenceTuple<int32_t, int32_t, int32_t, uint32_t>(firstsCountsInstanceCountsAndBaseInstances)));
        if (sendResult != IPC::Error::NoError)
            markContextLost();
    }
}

void RemoteGraphicsContextGLProxy::multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei, const GCGLsizei, const GCGLint, const GCGLuint> countsOffsetsInstanceCountsBaseVerticesAndBaseInstances, GCGLenum type)
{
    if (!isContextLost()) {
        auto sendResult = send(Messages::RemoteGraphicsContextGL::MultiDrawElementsInstancedBaseVertexBaseInstanceANGLE(mode, toArrayReferenceTuple<int32_t, int32_t, int32_t, int32_t, uint32_t>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances), type));
        if (sendResult != IPC::Error::NoError)
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

void RemoteGraphicsContextGLProxy::addDebugMessage(GCGLenum type, GCGLenum id, GCGLenum severity, String&& message)
{
    if (isContextLost())
        return;
    if (m_client)
        m_client->addDebugMessage(type, id, severity, WTFMove(message));
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
        || decoder.messageName() == Messages::RemoteGraphicsContextGLProxy::WasLost::name());
    return true;
}

void RemoteGraphicsContextGLProxy::waitUntilInitialized()
{
    if (isContextLost())
        return;
    if (m_didInitialize)
        return;
    if (m_streamConnection->waitForAndDispatchImmediately<Messages::RemoteGraphicsContextGLProxy::WasCreated>(m_identifier) == IPC::Error::NoError)
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
    if (!m_streamConnection)
        return;
    m_streamConnection->invalidate();
    m_streamConnection = nullptr;
    m_gpuProcessConnection = nullptr;
}

void RemoteGraphicsContextGLProxy::disconnectGpuProcessIfNeeded()
{
#if PLATFORM(COCOA)
    m_sharedVideoFrameWriter.disable();
#endif
    if (!m_streamConnection)
        return;
    m_streamConnection->invalidate();
    m_streamConnection = nullptr;
    ensureOnMainRunLoop([identifier = m_identifier, weakGPUProcessConnection = WTFMove(m_gpuProcessConnection)]() {
        RefPtr gpuProcessConnection = weakGPUProcessConnection.get();
        if (!gpuProcessConnection)
            return;
        gpuProcessConnection->releaseGraphicsContextGL(identifier);
    });
    ASSERT(isContextLost());
}

uint32_t RemoteGraphicsContextGLProxy::createObjectName()
{
    return ++m_nextObjectName;
}

} // namespace WebKit

#endif
