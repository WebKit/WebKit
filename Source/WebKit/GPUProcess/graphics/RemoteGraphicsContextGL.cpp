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
#include "RemoteGraphicsContextGL.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)

#include "GPUConnectionToWebProcess.h"
#include "RemoteGraphicsContextGLMessages.h"
#include "RemoteGraphicsContextGLProxyMessages.h"
#include "StreamConnectionWorkQueue.h"
#include <WebCore/GraphicsContextGLOpenGL.h>
#include <WebCore/NotImplemented.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#if USE(AVFOUNDATION)
#include <WebCore/GraphicsContextGLCV.h>
#endif

namespace WebKit {

using namespace WebCore;

// Currently we have one global WebGL processing instance.
static IPC::StreamConnectionWorkQueue& remoteGraphicsContextGLStreamWorkQueue()
{
    static NeverDestroyed<IPC::StreamConnectionWorkQueue> instance("RemoteGraphicsContextGL work queue");
    return instance.get();
}

#if !PLATFORM(COCOA)
Ref<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLAttributes&& attributes, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, IPC::StreamConnectionBuffer&& stream)
{
    ASSERT_NOT_REACHED();
    auto instance = adoptRef(*new RemoteGraphicsContextGL(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(stream)));
    instance->initialize(WTFMove(attributes));
    return instance;
}
#endif

RemoteGraphicsContextGL::RemoteGraphicsContextGL(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, IPC::StreamConnectionBuffer&& stream)
    : m_gpuConnectionToWebProcess(makeWeakPtr(gpuConnectionToWebProcess))
    , m_streamConnection(IPC::StreamServerConnection<RemoteGraphicsContextGL>::create(gpuConnectionToWebProcess.connection(), WTFMove(stream), remoteGraphicsContextGLStreamWorkQueue()))
    , m_graphicsContextGLIdentifier(graphicsContextGLIdentifier)
    , m_renderingBackend(makeRef(renderingBackend))
{
    ASSERT(RunLoop::isMain());
}

RemoteGraphicsContextGL::~RemoteGraphicsContextGL()
{
    ASSERT(!m_streamConnection);
    ASSERT(!m_context);
}

void RemoteGraphicsContextGL::initialize(GraphicsContextGLAttributes&& attributes)
{
    ASSERT(RunLoop::isMain());
    remoteGraphicsContextGLStreamWorkQueue().dispatch([attributes = WTFMove(attributes), protectedThis = makeRef(*this)]() mutable {
        protectedThis->workQueueInitialize(WTFMove(attributes));
    });
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteGraphicsContextGL::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64());
}

void RemoteGraphicsContextGL::stopListeningForIPC(Ref<RemoteGraphicsContextGL>&& refFromConnection)
{
    ASSERT(RunLoop::isMain());
    m_streamConnection->stopReceivingMessages(Messages::RemoteGraphicsContextGL::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64());
    remoteGraphicsContextGLStreamWorkQueue().dispatch([protectedThis = WTFMove(refFromConnection)]() {
        protectedThis->workQueueUninitialize();
    });
}

void RemoteGraphicsContextGL::workQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    platformWorkQueueInitialize(WTFMove(attributes));
    if (m_context) {
        m_context->addClient(*this);
        String extensions = m_context->getString(GraphicsContextGL::EXTENSIONS);
        String requestableExtensions = m_context->getString(ExtensionsGL::REQUESTABLE_EXTENSIONS_ANGLE);
        send(Messages::RemoteGraphicsContextGLProxy::WasCreated(true, remoteGraphicsContextGLStreamWorkQueue().wakeUpSemaphore(), extensions, requestableExtensions));
    } else
        send(Messages::RemoteGraphicsContextGLProxy::WasCreated(false, IPC::Semaphore { }, "", ""));
}

void RemoteGraphicsContextGL::workQueueUninitialize()
{
    m_context = nullptr;
    m_streamConnection = nullptr;
}

void RemoteGraphicsContextGL::didComposite()
{
}

void RemoteGraphicsContextGL::forceContextLost()
{
    send(Messages::RemoteGraphicsContextGLProxy::WasLost());
}

void RemoteGraphicsContextGL::recycleContext()
{
    ASSERT_NOT_REACHED();
}

void RemoteGraphicsContextGL::dispatchContextChangedNotification()
{
    send(Messages::RemoteGraphicsContextGLProxy::WasChanged());
}

void RemoteGraphicsContextGL::reshape(int32_t width, int32_t height)
{
    if (width && height)
        m_context->reshape(width, height);
    else
        forceContextLost();
}

#if !PLATFORM(COCOA)
void RemoteGraphicsContextGL::prepareForDisplay(CompletionHandler<void()>&& completionHandler)
{
    notImplemented();
    completionHandler();
}
#endif

void RemoteGraphicsContextGL::synthesizeGLError(uint32_t error)
{
    m_context->synthesizeGLError(static_cast<GCGLenum>(error));
}

void RemoteGraphicsContextGL::getError(CompletionHandler<void(uint32_t)>&& completionHandler)
{
    completionHandler(static_cast<uint32_t>(m_context->getError()));
}

void RemoteGraphicsContextGL::ensureExtensionEnabled(String&& extension)
{
    m_context->getExtensions().ensureEnabled(extension);
}

void RemoteGraphicsContextGL::notifyMarkContextChanged()
{
    m_context->markContextChanged();
}

void RemoteGraphicsContextGL::paintRenderingResultsToCanvas(WebCore::RenderingResourceIdentifier imageBuffer, CompletionHandler<void()>&& completionHandler)
{
    paintImageDataToImageBuffer(m_context->readRenderingResultsForPainting(), imageBuffer, WTFMove(completionHandler));
}

void RemoteGraphicsContextGL::paintCompositedResultsToCanvas(WebCore::RenderingResourceIdentifier imageBuffer, CompletionHandler<void()>&& completionHandler)
{
    paintImageDataToImageBuffer(m_context->readCompositedResultsForPainting(), imageBuffer, WTFMove(completionHandler));
}

void RemoteGraphicsContextGL::paintImageDataToImageBuffer(RefPtr<WebCore::ImageData>&& imageData, WebCore::RenderingResourceIdentifier target, CompletionHandler<void()>&& completionHandler)
{
    // FIXME: We do not have functioning read/write fences in RemoteRenderingBackend. Thus this is synchronous,
    // as are the messages that call these.
    Lock mutex;
    Condition conditionVariable;
    bool isFinished = false;
    m_renderingBackend->dispatch([&]() mutable {
        if (auto imageBuffer = m_renderingBackend->remoteResourceCache().cachedImageBuffer(target)) {
            // Here we do not try to play back pending commands for imageBuffer. Currently this call is only made for empty
            // image buffers and there's no good way to add display lists.
            if (imageData)
                GraphicsContextGLOpenGL::paintToCanvas(m_context->contextAttributes(), imageData.releaseNonNull(), imageBuffer->backendSize(), imageBuffer->context());
            else
                imageBuffer->context().clearRect({ IntPoint(), imageBuffer->backendSize() });
            // Unfortunately "flush" implementation in RemoteRenderingBackend overloads ordering and effects.
            imageBuffer->flushContext();
        }
        auto locker = holdLock(mutex);
        isFinished = true;
        conditionVariable.notifyOne();
    });
    std::unique_lock<Lock> lock(mutex);
    conditionVariable.wait(lock, [&] {
        return isFinished;
    });
    completionHandler();
}

void RemoteGraphicsContextGL::copyTextureFromMedia(WebCore::MediaPlayerIdentifier mediaPlayerIdentifier, uint32_t texture, uint32_t target, int32_t level, uint32_t internalFormat, uint32_t format, uint32_t type, bool premultiplyAlpha, bool flipY, CompletionHandler<void(bool)>&& completionHandler)
{
#if USE(AVFOUNDATION)
    UNUSED_VARIABLE(premultiplyAlpha);
    ASSERT_UNUSED(target, target == GraphicsContextGL::TEXTURE_2D);

    RetainPtr<CVPixelBufferRef> pixelBuffer;
    auto getPixelBuffer = [&] {
        if (!m_gpuConnectionToWebProcess)
            return;

        if (auto mediaPlayer = m_gpuConnectionToWebProcess->remoteMediaPlayerManagerProxy().mediaPlayer(mediaPlayerIdentifier))
            pixelBuffer = mediaPlayer->pixelBufferForCurrentTime();
    };

    if (isMainThread())
        getPixelBuffer();
    else
        callOnMainThreadAndWait(WTFMove(getPixelBuffer));

    if (!pixelBuffer) {
        completionHandler(false);
        return;
    }

    auto contextCV = m_context->asCV();
    if (!contextCV) {
        completionHandler(false);
        return;
    }

    completionHandler(contextCV->copyPixelBufferToTexture(pixelBuffer.get(), texture, level, internalFormat, format, type, GraphicsContextGL::FlipY(flipY)));
#else
    UNUSED_VARIABLE(mediaPlayerIdentifier);
    UNUSED_VARIABLE(texture);
    UNUSED_VARIABLE(target);
    UNUSED_VARIABLE(level);
    UNUSED_VARIABLE(internalFormat);
    UNUSED_VARIABLE(format);
    UNUSED_VARIABLE(type);
    UNUSED_VARIABLE(premultiplyAlpha);
    UNUSED_VARIABLE(flipY);

    notImplemented();
    completionHandler(false);
#endif
}

} // namespace WebKit

#endif
