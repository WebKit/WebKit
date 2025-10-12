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

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)

#include <wtf/TZoneMalloc.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

class RemoteGraphicsContextGLGBM final : public RemoteGraphicsContextGL {
    WTF_MAKE_TZONE_ALLOCATED(RemoteGraphicsContextGLGBM);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteGraphicsContextGLGBM);

public:
    RemoteGraphicsContextGLGBM(GPUConnectionToWebProcess&, RemoteGraphicsContextGLIdentifier, RemoteRenderingBackend&, Ref<IPC::StreamServerConnection>&&);

private:
    void platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&&) final;
    void prepareForDisplay(CompletionHandler<void(uint64_t, std::optional<WebCore::DMABufBuffer::Attributes>&&, UnixFileDescriptor&&)>&&) final;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteGraphicsContextGLGBM);

RemoteGraphicsContextGLGBM::RemoteGraphicsContextGLGBM(GPUConnectionToWebProcess& connection, RemoteGraphicsContextGLIdentifier identifier, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
    : RemoteGraphicsContextGL(connection, identifier, renderingBackend, WTFMove(streamConnection))
{ }

void RemoteGraphicsContextGLGBM::platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    assertIsCurrent(workQueue());
    m_context = WebCore::GraphicsContextGLTextureMapperGBM::create(WTFMove(attributes));
}

void RemoteGraphicsContextGLGBM::prepareForDisplay(CompletionHandler<void(uint64_t, std::optional<WebCore::DMABufBuffer::Attributes>&&, UnixFileDescriptor&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());

    UnixFileDescriptor fenceFD;
    m_context->prepareForDisplayWithFinishedSignal([this, &fenceFD] {
        fenceFD = m_context->createExportedFence();
        if (!fenceFD)
            m_context->flush();
    });

    auto* buffer = m_context->displayBuffer();
    if (!buffer) {
        completionHandler(0, std::nullopt, { });
        return;
    }

    completionHandler(buffer->id(), buffer->takeAttributes(), WTFMove(fenceFD));
}

Ref<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(GPUConnectionToWebProcess& connection, WebCore::GraphicsContextGLAttributes&& attributes, RemoteGraphicsContextGLIdentifier identifier, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
{
    auto instance = adoptRef(*new RemoteGraphicsContextGLGBM(connection, identifier, renderingBackend, WTFMove(streamConnection)));
    instance->initialize(WTFMove(attributes));
    return instance;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
