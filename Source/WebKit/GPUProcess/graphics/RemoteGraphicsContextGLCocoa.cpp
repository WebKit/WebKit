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
#include <WebCore/GraphicsContextGLIOSurfaceSwapChain.h>
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
    WebCore::GraphicsContextGLIOSurfaceSwapChain m_swapChain WTF_GUARDED_BY_LOCK(m_streamThread);
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY)
    task_id_token_t m_webProcessIdentityToken;
#endif
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
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY)
    , m_webProcessIdentityToken(gpuConnectionToWebProcess.webProcessIdentityToken())
#endif
{

}

void RemoteGraphicsContextGLCocoa::platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    assertIsCurrent(m_streamThread);
    m_context = GraphicsContextGLOpenGL::createForGPUProcess(WTFMove(attributes), &m_swapChain);
}

void RemoteGraphicsContextGLCocoa::prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&& completionHandler)
{
    assertIsCurrent(m_streamThread);
    m_context->prepareForDisplay();
    MachSendRight sendRight;
    if (auto* surface = m_swapChain.displayBuffer().surface.get()) {
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY)
        // Mark the IOSurface as being owned by the WebProcess even though it was constructed by the GPUProcess so that Jetsam knows which process to kill.
        surface->setOwnershipIdentity(m_webProcessIdentityToken);
#endif
        sendRight = surface->createSendRight();
    }
    completionHandler(WTFMove(sendRight));
}

}

#endif
