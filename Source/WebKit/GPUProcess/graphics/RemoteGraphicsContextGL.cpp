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
#include <WebCore/GraphicsContextGLOpenGL.h>
#include <WebCore/NotImplemented.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeap.h"
#endif

namespace WebKit {

using namespace WebCore;

// Currently we have one global WebGL processing instance.
IPC::StreamConnectionWorkQueue& remoteGraphicsContextGLStreamWorkQueue()
{
    static NeverDestroyed<IPC::StreamConnectionWorkQueue> instance("RemoteGraphicsContextGL work queue");
    return instance.get();
}

#if !PLATFORM(COCOA) && !PLATFORM(WIN)
Ref<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLAttributes&& attributes, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, IPC::StreamConnectionBuffer&& stream)
{
    ASSERT_NOT_REACHED();
    auto instance = adoptRef(*new RemoteGraphicsContextGL(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(stream)));
    instance->initialize(WTFMove(attributes));
    return instance;
}
#endif

RemoteGraphicsContextGL::RemoteGraphicsContextGL(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, IPC::StreamConnectionBuffer&& stream)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_streamConnection(IPC::StreamServerConnection::create(gpuConnectionToWebProcess.connection(), WTFMove(stream), remoteGraphicsContextGLStreamWorkQueue()))
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
    // Might be destroyed on main thread or stream processing thread.
    m_streamThread.reset();
}

void RemoteGraphicsContextGL::initialize(GraphicsContextGLAttributes&& attributes)
{
    assertIsMainRunLoop();
    remoteGraphicsContextGLStreamWorkQueue().dispatch([attributes = WTFMove(attributes), protectedThis = Ref { *this }]() mutable {
        protectedThis->workQueueInitialize(WTFMove(attributes));
    });
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteGraphicsContextGL::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64());
}

void RemoteGraphicsContextGL::stopListeningForIPC(Ref<RemoteGraphicsContextGL>&& refFromConnection)
{
    assertIsMainRunLoop();
    m_streamConnection->stopReceivingMessages(Messages::RemoteGraphicsContextGL::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64());
    remoteGraphicsContextGLStreamWorkQueue().dispatch([protectedThis = WTFMove(refFromConnection)]() {
        protectedThis->workQueueUninitialize();
        protectedThis->m_renderingResourcesRequest = { };
    });
}

#if PLATFORM(MAC)
void RemoteGraphicsContextGL::displayWasReconfigured()
{
    assertIsMainRunLoop();
    remoteGraphicsContextGLStreamWorkQueue().dispatch([protectedThis = Ref { *this }]() {
        assertIsCurrent(protectedThis->m_streamThread);
        protectedThis->m_context->updateContextOnDisplayReconfiguration();
    });
}
#endif

void RemoteGraphicsContextGL::workQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    m_streamThread.reset();
    assertIsCurrent(m_streamThread);
    platformWorkQueueInitialize(WTFMove(attributes));
    if (m_context) {
        m_context->addClient(*this);
        String extensions = m_context->getString(GraphicsContextGL::EXTENSIONS);
        String requestableExtensions = m_context->getString(GraphicsContextGL::REQUESTABLE_EXTENSIONS_ANGLE);
        send(Messages::RemoteGraphicsContextGLProxy::WasCreated(true, remoteGraphicsContextGLStreamWorkQueue().wakeUpSemaphore(), extensions, requestableExtensions));
    } else
        send(Messages::RemoteGraphicsContextGLProxy::WasCreated(false, IPC::Semaphore { }, "", ""));
}

void RemoteGraphicsContextGL::workQueueUninitialize()
{
    assertIsCurrent(m_streamThread);
    m_context = nullptr;
    m_streamConnection = nullptr;
}

void RemoteGraphicsContextGL::didComposite()
{
    assertIsCurrent(m_streamThread);
}

void RemoteGraphicsContextGL::forceContextLost()
{
    assertIsCurrent(m_streamThread);
    send(Messages::RemoteGraphicsContextGLProxy::WasLost());
}

void RemoteGraphicsContextGL::recycleContext()
{
    ASSERT_NOT_REACHED();
}

void RemoteGraphicsContextGL::dispatchContextChangedNotification()
{
    assertIsCurrent(m_streamThread);
    send(Messages::RemoteGraphicsContextGLProxy::WasChanged());
}

void RemoteGraphicsContextGL::reshape(int32_t width, int32_t height)
{
    assertIsCurrent(m_streamThread);
    if (width && height)
        m_context->reshape(width, height);
    else
        forceContextLost();
}

#if !PLATFORM(COCOA) && !PLATFORM(WIN)
void RemoteGraphicsContextGL::prepareForDisplay(CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(m_streamThread);
    notImplemented();
    completionHandler();
}
#endif

void RemoteGraphicsContextGL::synthesizeGLError(uint32_t error)
{
    assertIsCurrent(m_streamThread);
    m_context->synthesizeGLError(static_cast<GCGLenum>(error));
}

void RemoteGraphicsContextGL::getError(CompletionHandler<void(uint32_t)>&& completionHandler)
{
    assertIsCurrent(m_streamThread);
    completionHandler(static_cast<uint32_t>(m_context->getError()));
}

void RemoteGraphicsContextGL::ensureExtensionEnabled(String&& extension)
{
    assertIsCurrent(m_streamThread);
    m_context->ensureExtensionEnabled(extension);
}

void RemoteGraphicsContextGL::markContextChanged()
{
    assertIsCurrent(m_streamThread);
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
    assertIsCurrent(m_streamThread);
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
    assertIsCurrent(m_streamThread);
    paintPixelBufferToImageBuffer(m_context->readCompositedResultsForPainting(), imageBuffer, WTFMove(completionHandler));
}

#if ENABLE(MEDIA_STREAM)
void RemoteGraphicsContextGL::paintCompositedResultsToMediaSample(CompletionHandler<void(std::optional<WebKit::RemoteVideoFrameProxy::Properties>&&)>&& completionHandler)
{
    assertIsCurrent(m_streamThread);
    std::optional<WebKit::RemoteVideoFrameProxy::Properties> result;
    if (auto videoFrame = m_context->paintCompositedResultsToMediaSample()) {
        auto write = RemoteVideoFrameWriteReference::generateForAdd();
        auto newFrameReference = write.retiredReference();
        result = RemoteVideoFrameProxy::properties(WTFMove(newFrameReference), *videoFrame);
        m_videoFrameObjectHeap->retire(WTFMove(write), WTFMove(videoFrame), std::nullopt);
    }
    completionHandler(WTFMove(result));
}
#endif

void RemoteGraphicsContextGL::paintPixelBufferToImageBuffer(std::optional<WebCore::PixelBuffer>&& pixelBuffer, QualifiedRenderingResourceIdentifier target, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(m_streamThread);
    // FIXME: We do not have functioning read/write fences in RemoteRenderingBackend. Thus this is synchronous,
    // as are the messages that call these.
    Lock lock;
    Condition conditionVariable;
    bool isFinished = false;

    // FIXME: This should not be needed. Maybe ArrayBufferView should be ThreadSafeRefCounted as it is used in accross multiple threads.
    // The call below is synchronous and we transfer the ownership of the `pixelBuffer`.
    if (pixelBuffer)
        pixelBuffer->data().disableThreadingChecks();
    m_renderingBackend->dispatch([&, contextAttributes = m_context->contextAttributes()]() mutable {
        if (auto imageBuffer = m_renderingBackend->remoteResourceCache().cachedImageBuffer(target)) {
            // Here we do not try to play back pending commands for imageBuffer. Currently this call is only made for empty
            // image buffers and there's no good way to add display lists.
            if (pixelBuffer)
                GraphicsContextGL::paintToCanvas(contextAttributes, WTFMove(*pixelBuffer), imageBuffer->backendSize(), imageBuffer->context());
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
    m_videoFrameObjectHeap->retire(WTFMove(read), defaultTimeout);
    completionHandler(false);
}
#endif

void RemoteGraphicsContextGL::simulateEventForTesting(WebCore::GraphicsContextGL::SimulatedEventForTesting event)
{
    assertIsCurrent(m_streamThread);
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

} // namespace WebKit

#endif
