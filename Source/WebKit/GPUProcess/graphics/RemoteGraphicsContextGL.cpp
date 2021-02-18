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
#include <WebCore/GraphicsContextGLOpenGL.h>
#include <WebCore/NotImplemented.h>


namespace WebKit {
using namespace WebCore;

#if !PLATFORM(COCOA)
std::unique_ptr<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(const WebCore::GraphicsContextGLAttributes&, GPUConnectionToWebProcess&, GraphicsContextGLIdentifier, RemoteRenderingBackend&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}
#endif

RemoteGraphicsContextGL::RemoteGraphicsContextGL(const WebCore::GraphicsContextGLAttributes& attributes, GPUConnectionToWebProcess& connection, GraphicsContextGLIdentifier identifier, Ref<GraphicsContextGLOpenGL> context, RemoteRenderingBackend& renderingBackend)
    : m_context(WTFMove(context))
    , m_gpuConnectionToWebProcess(makeWeakPtr(connection))
    , m_graphicsContextGLIdentifier(identifier)
    , m_renderingBackend(makeRef(renderingBackend))
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        gpuConnectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteGraphicsContextGL::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64(), *this);
    m_context->addClient(*this);
    String extensions = m_context->getString(GraphicsContextGL::EXTENSIONS);
    String requestableExtensions = m_context->getString(ExtensionsGL::REQUESTABLE_EXTENSIONS_ANGLE);
    send(Messages::RemoteGraphicsContextGLProxy::WasCreated(true, extensions, requestableExtensions), m_graphicsContextGLIdentifier);
}

RemoteGraphicsContextGL::~RemoteGraphicsContextGL()
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        gpuConnectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteGraphicsContextGL::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64());
}

GPUConnectionToWebProcess* RemoteGraphicsContextGL::gpuConnectionToWebProcess() const
{
    return m_gpuConnectionToWebProcess.get();
}

IPC::Connection* RemoteGraphicsContextGL::messageSenderConnection() const
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        return &gpuConnectionToWebProcess->connection();
    return nullptr;
}

uint64_t RemoteGraphicsContextGL::messageSenderDestinationID() const
{
    return m_graphicsContextGLIdentifier.toUInt64();
}

void RemoteGraphicsContextGL::didComposite()
{
}

void RemoteGraphicsContextGL::forceContextLost()
{
    send(Messages::RemoteGraphicsContextGLProxy::WasLost(), m_graphicsContextGLIdentifier);
}

void RemoteGraphicsContextGL::recycleContext()
{
    ASSERT_NOT_REACHED();
}

void RemoteGraphicsContextGL::dispatchContextChangedNotification()
{
    send(Messages::RemoteGraphicsContextGLProxy::WasChanged(), m_graphicsContextGLIdentifier);
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

} // namespace WebKit

#endif
