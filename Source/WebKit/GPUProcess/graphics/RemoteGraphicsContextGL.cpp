/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "RemoteGraphicsContextGL.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)

#include "GPUConnectionToWebProcess.h"
#include "QualifiedRenderingResourceIdentifier.h"
#include "RemoteGraphicsContextGLMessages.h"
#include "RemoteGraphicsContextGLProxyMessages.h"
#include "StreamConnectionWorkQueue.h"
#include <WebCore/ByteArrayPixelBuffer.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/NotImplemented.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeap.h"
#endif

namespace WebKit {

using namespace WebCore;

namespace {
template<typename S, int I, typename T>
Vector<S> vectorCopyCast(const T& arrayReference)
{
    return { reinterpret_cast<const S*>(arrayReference.template data<I>()), arrayReference.size() };
}
}

// Currently we have one global WebGL processing instance.
IPC::StreamConnectionWorkQueue& remoteGraphicsContextGLStreamWorkQueue()
{
    static NeverDestroyed<IPC::StreamConnectionWorkQueue> instance("RemoteGraphicsContextGL work queue");
    return instance.get();
}

#if !PLATFORM(COCOA) && !USE(GRAPHICS_LAYER_WC) && !USE(LIBGBM)
Ref<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLAttributes&& attributes, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, IPC::StreamServerConnection::Handle&& connectionHandle)
{
    ASSERT_NOT_REACHED();
    auto instance = adoptRef(*new RemoteGraphicsContextGL(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(connectionHandle)));
    instance->initialize(WTFMove(attributes));
    return instance;
}
#endif

RemoteGraphicsContextGL::RemoteGraphicsContextGL(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, IPC::StreamServerConnection::Handle&& connectionHandle)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_streamConnection(IPC::StreamServerConnection::create(WTFMove(connectionHandle), remoteGraphicsContextGLStreamWorkQueue()))
    , m_graphicsContextGLIdentifier(graphicsContextGLIdentifier)
    , m_renderingBackend(renderingBackend)
#if ENABLE(VIDEO)
    , m_videoFrameObjectHeap(gpuConnectionToWebProcess.videoFrameObjectHeap())
#endif
    , m_renderingResourcesRequest(ScopedWebGLRenderingResourcesRequest::acquire())
    , m_webProcessIdentifier(gpuConnectionToWebProcess.webProcessIdentifier())
{
    assertIsMainRunLoop();
}

RemoteGraphicsContextGL::~RemoteGraphicsContextGL()
{
    ASSERT(!m_streamConnection);
    ASSERT(!m_context);
}

void RemoteGraphicsContextGL::initialize(GraphicsContextGLAttributes&& attributes)
{
    assertIsMainRunLoop();
    remoteGraphicsContextGLStreamWorkQueue().dispatch([attributes = WTFMove(attributes), protectedThis = Ref { *this }]() mutable {
        protectedThis->workQueueInitialize(WTFMove(attributes));
    });
}

void RemoteGraphicsContextGL::stopListeningForIPC(Ref<RemoteGraphicsContextGL>&& refFromConnection)
{
    assertIsMainRunLoop();
    remoteGraphicsContextGLStreamWorkQueue().dispatch([protectedThis = WTFMove(refFromConnection)]() {
        protectedThis->workQueueUninitialize();
    });
}

#if PLATFORM(MAC)
void RemoteGraphicsContextGL::displayWasReconfigured()
{
    assertIsMainRunLoop();
    remoteGraphicsContextGLStreamWorkQueue().dispatch([protectedThis = Ref { *this }]() {
        assertIsCurrent(protectedThis->workQueue());
        protectedThis->m_context->updateContextOnDisplayReconfiguration();
    });
}
#endif

void RemoteGraphicsContextGL::workQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    assertIsCurrent(workQueue());
    platformWorkQueueInitialize(WTFMove(attributes));
    m_streamConnection->open();
    if (m_context) {
        m_context->setClient(this);
        String extensions = m_context->getString(GraphicsContextGL::EXTENSIONS);
        String requestableExtensions = m_context->getString(GraphicsContextGL::REQUESTABLE_EXTENSIONS_ANGLE);
        send(Messages::RemoteGraphicsContextGLProxy::WasCreated(true, remoteGraphicsContextGLStreamWorkQueue().wakeUpSemaphore(), m_streamConnection->clientWaitSemaphore(), extensions, requestableExtensions));
        m_streamConnection->startReceivingMessages(*this, Messages::RemoteGraphicsContextGL::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64());
    } else
        send(Messages::RemoteGraphicsContextGLProxy::WasCreated(false, { }, { }, emptyString(), emptyString()));
}

void RemoteGraphicsContextGL::workQueueUninitialize()
{
    assertIsCurrent(workQueue());
    m_streamConnection->invalidate();
    if (m_context) {
        m_context->setClient(nullptr);
        m_context = nullptr;
        m_streamConnection->stopReceivingMessages(Messages::RemoteGraphicsContextGL::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64());
    }
    m_streamConnection = nullptr;
    m_renderingResourcesRequest = { };
}

void RemoteGraphicsContextGL::didComposite()
{
    assertIsCurrent(workQueue());
}

void RemoteGraphicsContextGL::forceContextLost()
{
    assertIsCurrent(workQueue());
    send(Messages::RemoteGraphicsContextGLProxy::WasLost());
}

void RemoteGraphicsContextGL::dispatchContextChangedNotification()
{
    assertIsCurrent(workQueue());
    send(Messages::RemoteGraphicsContextGLProxy::WasChanged());
}

void RemoteGraphicsContextGL::reshape(int32_t width, int32_t height)
{
    assertIsCurrent(workQueue());
    if (width && height)
        m_context->reshape(width, height);
    else
        forceContextLost();
}

#if !PLATFORM(COCOA) && !USE(GRAPHICS_LAYER_WC) && !USE(LIBGBM)
void RemoteGraphicsContextGL::prepareForDisplay(CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    notImplemented();
    completionHandler();
}
#endif

void RemoteGraphicsContextGL::synthesizeGLError(uint32_t error)
{
    assertIsCurrent(workQueue());
    m_context->synthesizeGLError(static_cast<GCGLenum>(error));
}

void RemoteGraphicsContextGL::getError(CompletionHandler<void(uint32_t)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    completionHandler(static_cast<uint32_t>(m_context->getError()));
}

void RemoteGraphicsContextGL::ensureExtensionEnabled(String&& extension)
{
    assertIsCurrent(workQueue());
    m_context->ensureExtensionEnabled(extension);
}

void RemoteGraphicsContextGL::markContextChanged()
{
    assertIsCurrent(workQueue());
    m_context->markContextChanged();
}

void RemoteGraphicsContextGL::paintRenderingResultsToCanvas(WebCore::RenderingResourceIdentifier imageBuffer, CompletionHandler<void()>&& completionHandler)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    paintRenderingResultsToCanvasWithQualifiedIdentifier({ imageBuffer, m_webProcessIdentifier }, WTFMove(completionHandler));
}

void RemoteGraphicsContextGL::paintRenderingResultsToCanvasWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier imageBuffer, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    paintPixelBufferToImageBuffer(m_context->readRenderingResultsForPainting(), imageBuffer, WTFMove(completionHandler));
}

void RemoteGraphicsContextGL::paintCompositedResultsToCanvas(WebCore::RenderingResourceIdentifier imageBuffer, CompletionHandler<void()>&& completionHandler)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    paintCompositedResultsToCanvasWithQualifiedIdentifier({ imageBuffer, m_webProcessIdentifier }, WTFMove(completionHandler));
}

void RemoteGraphicsContextGL::paintCompositedResultsToCanvasWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier imageBuffer, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    paintPixelBufferToImageBuffer(m_context->readCompositedResultsForPainting(), imageBuffer, WTFMove(completionHandler));
}

#if ENABLE(MEDIA_STREAM)
void RemoteGraphicsContextGL::paintCompositedResultsToVideoFrame(CompletionHandler<void(std::optional<WebKit::RemoteVideoFrameProxy::Properties>&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    std::optional<WebKit::RemoteVideoFrameProxy::Properties> result;
    if (auto videoFrame = m_context->paintCompositedResultsToVideoFrame())
        result = m_videoFrameObjectHeap->add(videoFrame.releaseNonNull());
    completionHandler(WTFMove(result));
}
#endif

void RemoteGraphicsContextGL::paintPixelBufferToImageBuffer(RefPtr<WebCore::PixelBuffer>&& pixelBuffer, QualifiedRenderingResourceIdentifier target, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    // FIXME: We do not have functioning read/write fences in RemoteRenderingBackend. Thus this is synchronous,
    // as are the messages that call these.
    Lock lock;
    Condition conditionVariable;
    bool isFinished = false;

    // FIXME: This should not be needed. Maybe ArrayBufferView should be ThreadSafeRefCounted as it is used in accross multiple threads.
    // The call below is synchronous and we transfer the ownership of the `pixelBuffer`.
    if (is<ByteArrayPixelBuffer>(pixelBuffer))
        downcast<ByteArrayPixelBuffer>(*pixelBuffer).data().disableThreadingChecks();
    m_renderingBackend->dispatch([&, contextAttributes = m_context->contextAttributes()]() mutable {
        if (auto imageBuffer = m_renderingBackend->remoteResourceCache().cachedImageBuffer(target)) {
            // Here we do not try to play back pending commands for imageBuffer. Currently this call is only made for empty
            // image buffers and there's no good way to add display lists.
            if (pixelBuffer)
                GraphicsContextGL::paintToCanvas(contextAttributes, pixelBuffer.releaseNonNull(), imageBuffer->backendSize(), imageBuffer->context());
            else
                imageBuffer->context().clearRect({ IntPoint(), imageBuffer->backendSize() });
            // Unfortunately "flush" implementation in RemoteRenderingBackend overloads ordering and effects.
            imageBuffer->flushContext();
        }
        Locker locker { lock };
        isFinished = true;
        conditionVariable.notifyOne();
    });
    Locker locker { lock };
    conditionVariable.wait(lock, [&] {
        return isFinished;
    });
    completionHandler();
}

#if ENABLE(VIDEO) && !USE(AVFOUNDATION)
void RemoteGraphicsContextGL::copyTextureFromVideoFrame(WebKit::RemoteVideoFrameReadReference read, uint32_t, uint32_t, int32_t, uint32_t, uint32_t, uint32_t, bool, bool , CompletionHandler<void(bool)>&& completionHandler)
{
    notImplemented();
    m_videoFrameObjectHeap->get(WTFMove(read));
    completionHandler(false);
}
#endif

void RemoteGraphicsContextGL::simulateEventForTesting(WebCore::GraphicsContextGL::SimulatedEventForTesting event)
{
    assertIsCurrent(workQueue());
    // FIXME: only run this in testing mode. https://bugs.webkit.org/show_bug.cgi?id=222544
    if (event == WebCore::GraphicsContextGL::SimulatedEventForTesting::Timeout) {
        // Simulate the timeout by just discarding the context. The subsequent messages act like
        // unauthorized or old messages from Web process, they are skipped.
        callOnMainRunLoop([gpuConnectionToWebProcess = m_gpuConnectionToWebProcess, identifier = m_graphicsContextGLIdentifier]() {
            if (auto connectionToWeb = gpuConnectionToWebProcess.get())
                connectionToWeb->releaseGraphicsContextGLForTesting(identifier);
        });
        return;
    }
    if (event == WebCore::GraphicsContextGL::SimulatedEventForTesting::ContextChange) {
#if PLATFORM(MAC)
        callOnMainRunLoop([weakConnection = m_gpuConnectionToWebProcess]() {
            if (auto connection = weakConnection.get())
                connection->dispatchDisplayWasReconfiguredForTesting();
        });
#endif
        return;
    }
    m_context->simulateEventForTesting(event);
}

void RemoteGraphicsContextGL::readnPixels0(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type, IPC::ArrayReference<uint8_t>&& data, CompletionHandler<void(IPC::ArrayReference<uint8_t>)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    Vector<uint8_t, 4> pixels(data);
    m_context->readnPixels(x, y, width, height, format, type, pixels);
    completionHandler(IPC::ArrayReference<uint8_t>(reinterpret_cast<uint8_t*>(pixels.data()), pixels.size()));
}

void RemoteGraphicsContextGL::readnPixels1(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type, uint64_t offset)
{
    assertIsCurrent(workQueue());
    m_context->readnPixels(x, y, width, height, format, type, static_cast<GCGLintptr>(offset));
}

void RemoteGraphicsContextGL::multiDrawArraysANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t>&& firstsAndCounts)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    Vector<GCGLint> firsts = vectorCopyCast<GCGLint, 0>(firstsAndCounts);
    Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 1>(firstsAndCounts);
    m_context->multiDrawArraysANGLE(mode, GCGLSpanTuple { firsts, counts });
}

void RemoteGraphicsContextGL::multiDrawArraysInstancedANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t>&& firstsCountsAndInstanceCounts)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    Vector<GCGLint> firsts = vectorCopyCast<GCGLint, 0>(firstsCountsAndInstanceCounts);
    Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 1>(firstsCountsAndInstanceCounts);
    Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(firstsCountsAndInstanceCounts);
    m_context->multiDrawArraysInstancedANGLE(mode, GCGLSpanTuple { firsts, counts, instanceCounts });
}

void RemoteGraphicsContextGL::multiDrawElementsANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t>&& countsAndOffsets, uint32_t type)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    const Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 0>(countsAndOffsets);
    // Currently offsets are copied in the m_context.
    const GCGLsizei* offsets = reinterpret_cast<const GCGLsizei*>(countsAndOffsets.data<1>());
    m_context->multiDrawElementsANGLE(mode, GCGLSpanTuple { counts.data(), offsets, counts.size() }, type);
}

void RemoteGraphicsContextGL::multiDrawElementsInstancedANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t>&& countsOffsetsAndInstanceCounts, uint32_t type)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    const Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 0>(countsOffsetsAndInstanceCounts);
    // Currently offsets are copied in the m_context.
    const GCGLsizei* offsets = reinterpret_cast<const GCGLsizei*>(countsOffsetsAndInstanceCounts.data<1>());
    const Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(countsOffsetsAndInstanceCounts);
    m_context->multiDrawElementsInstancedANGLE(mode, GCGLSpanTuple { counts.data(), offsets, instanceCounts.data(), counts.size() }, type);
}

void RemoteGraphicsContextGL::multiDrawArraysInstancedBaseInstanceANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t, uint32_t>&& firstsCountsInstanceCountsAndBaseInstances)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    Vector<GCGLint> firsts = vectorCopyCast<GCGLint, 0>(firstsCountsInstanceCountsAndBaseInstances);
    Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 1>(firstsCountsInstanceCountsAndBaseInstances);
    Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(firstsCountsInstanceCountsAndBaseInstances);
    Vector<GCGLuint> baseInstances = vectorCopyCast<GCGLuint, 3>(firstsCountsInstanceCountsAndBaseInstances);
    m_context->multiDrawArraysInstancedBaseInstanceANGLE(mode, GCGLSpanTuple { firsts, counts, instanceCounts, baseInstances });
}

void RemoteGraphicsContextGL::multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t, int32_t, uint32_t>&& countsOffsetsInstanceCountsBaseVerticesAndBaseInstances, uint32_t type)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    const Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 0>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances);
    // Currently offsets are copied in the m_context.
    const GCGLsizei* offsets = reinterpret_cast<const GCGLsizei*>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances.data<1>());
    const Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances);
    const Vector<GCGLint> baseVertices = vectorCopyCast<GCGLint, 3>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances);
    const Vector<GCGLuint> baseInstances = vectorCopyCast<GCGLuint, 4>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances);
    m_context->multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(mode, GCGLSpanTuple { counts.data(), offsets, instanceCounts.data(), baseVertices.data(), baseInstances.data(), counts.size() }, type);
}

} // namespace WebKit

#endif
