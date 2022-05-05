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

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && PLATFORM(COCOA)

#include "GPUConnectionToWebProcess.h"
#include <WebCore/ProcessIdentity.h>
#include <wtf/MachSendRight.h>

namespace WebKit {
using namespace WebCore;

namespace {

class RemoteGraphicsContextGLCocoa final : public RemoteGraphicsContextGL {
public:
    RemoteGraphicsContextGLCocoa(GPUConnectionToWebProcess&, GraphicsContextGLIdentifier, RemoteRenderingBackend&, IPC::StreamConnectionBuffer&&);
    ~RemoteGraphicsContextGLCocoa() final = default;

    // RemoteGraphicsContextGL overrides.
    void platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&&) final;
    void prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&&) final;
private:
    const ProcessIdentity m_resourceOwner;
};

}

Ref<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLAttributes&& attributes, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, IPC::StreamConnectionBuffer&& stream)
{
    auto instance = adoptRef(*new RemoteGraphicsContextGLCocoa(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(stream)));
    instance->initialize(WTFMove(attributes));
    return instance;
}

RemoteGraphicsContextGLCocoa::RemoteGraphicsContextGLCocoa(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, IPC::StreamConnectionBuffer&& stream)
    : RemoteGraphicsContextGL(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(stream))
    , m_resourceOwner(gpuConnectionToWebProcess.webProcessIdentity())
{

}

void RemoteGraphicsContextGLCocoa::platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    assertIsCurrent(m_streamThread);
    m_context = GraphicsContextGLCocoa::create(WTFMove(attributes), ProcessIdentity { m_resourceOwner });
}

void RemoteGraphicsContextGLCocoa::prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&& completionHandler)
{
    assertIsCurrent(m_streamThread);
    m_context->prepareForDisplay();
    MachSendRight sendRight;
    IOSurface* displayBuffer = m_context->displayBuffer();
    if (displayBuffer) {
        m_context->markDisplayBufferInUse();
        sendRight = displayBuffer->createSendRight();
    }
    completionHandler(WTFMove(sendRight));
}

}

#endif
