/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(LIBGBM)

namespace WebKit {

class RemoteGraphicsContextGLGBM final : public RemoteGraphicsContextGL {
public:
    RemoteGraphicsContextGLGBM(GPUConnectionToWebProcess&, GraphicsContextGLIdentifier, RemoteRenderingBackend&, IPC::StreamServerConnection::Handle&&);

private:
    void platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&&) final;
    void prepareForDisplay(CompletionHandler<void(WebCore::DMABufObject&&)>&&) final;
};

RemoteGraphicsContextGLGBM::RemoteGraphicsContextGLGBM(GPUConnectionToWebProcess& connection, GraphicsContextGLIdentifier identifier, RemoteRenderingBackend& renderingBackend, IPC::StreamServerConnection::Handle&& connectionHandle)
    : RemoteGraphicsContextGL(connection, identifier, renderingBackend, WTFMove(connectionHandle))
{ }

void RemoteGraphicsContextGLGBM::platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    assertIsCurrent(workQueue());
    m_context = WebCore::GraphicsContextGLGBM::create(WTFMove(attributes));
}

void RemoteGraphicsContextGLGBM::prepareForDisplay(CompletionHandler<void(WebCore::DMABufObject&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    m_context->prepareForDisplay();

    WebCore::DMABufObject dmabufObject(0);
    auto& swapchain = m_context->swapchain();
    if (swapchain.displayBO) {
        uintptr_t handle = reinterpret_cast<uintptr_t>(swapchain.swapchain.get()) + swapchain.displayBO->handle();
        dmabufObject = swapchain.displayBO->createDMABufObject(handle);
        swapchain.displayBO = nullptr;
    }

    completionHandler(WTFMove(dmabufObject));
}

Ref<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(GPUConnectionToWebProcess& connection, WebCore::GraphicsContextGLAttributes&& attributes, GraphicsContextGLIdentifier identifier, RemoteRenderingBackend& renderingBackend, IPC::StreamServerConnection::Handle&& connectionHandle)
{
    auto instance = adoptRef(*new RemoteGraphicsContextGLGBM(connection, identifier, renderingBackend, WTFMove(connectionHandle)));
    instance->initialize(WTFMove(attributes));
    return instance;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(LIBGBM)
