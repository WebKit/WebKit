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
std::unique_ptr<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(const WebCore::GraphicsContextGLAttributes& attributes, GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLIdentifier graphicsContextGLIdentifier)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}
#endif

RemoteGraphicsContextGL::RemoteGraphicsContextGL(const WebCore::GraphicsContextGLAttributes& attributes, GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLIdentifier graphicsContextGLIdentifier, Ref<GraphicsContextGLOpenGL> context)
    : m_context(WTFMove(context))
    , m_gpuConnectionToWebProcess(makeWeakPtr(gpuConnectionToWebProcess))
    , m_graphicsContextGLIdentifier(graphicsContextGLIdentifier)
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        gpuConnectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteGraphicsContextGL::messageReceiverName(), graphicsContextGLIdentifier.toUInt64(), *this);
    m_context->addClient(*this);
    String extensions = m_context->getString(GraphicsContextGL::EXTENSIONS);
    String requestableExtensions = m_context->getString(ExtensionsGL::REQUESTABLE_EXTENSIONS_ANGLE);
    send(Messages::RemoteGraphicsContextGLProxy::WasCreated(extensions, requestableExtensions), m_graphicsContextGLIdentifier);
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

void RemoteGraphicsContextGL::ensureExtensionEnabled(String&& extension)
{
    m_context->getExtensions().ensureEnabled(extension);
}

void RemoteGraphicsContextGL::notifyMarkContextChanged()
{
    m_context->markContextChanged();
}

} // namespace WebKit

#endif
