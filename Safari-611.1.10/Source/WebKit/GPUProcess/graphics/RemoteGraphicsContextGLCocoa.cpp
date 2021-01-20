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

class RemoteGraphicsContextGLCocoa final : public RemoteGraphicsContextGL, private WebCore::GraphicsContextGLIOSurfaceSwapChain {
public:
    RemoteGraphicsContextGLCocoa(const WebCore::GraphicsContextGLAttributes&, GPUConnectionToWebProcess&, GraphicsContextGLIdentifier);
    ~RemoteGraphicsContextGLCocoa() final = default;

    // RemoteGraphicsContextGL overrides.
    void prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&&) final;

    // GraphicsContextGLIOSurfaceSwapChain overrides.
    Buffer recycleBuffer() final;
    void present(Buffer) final;
    const Buffer& displayBuffer() const final;
    void* detachClient() final;
private:
    GraphicsContextGLIOSurfaceSwapChain::Buffer m_displayBuffer;
};

}

std::unique_ptr<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(const WebCore::GraphicsContextGLAttributes& attributes, GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLIdentifier graphicsContextGLIdentifier)
{
    return std::unique_ptr<RemoteGraphicsContextGL>(new RemoteGraphicsContextGLCocoa(attributes, gpuConnectionToWebProcess, graphicsContextGLIdentifier));
}

RemoteGraphicsContextGLCocoa::RemoteGraphicsContextGLCocoa(const WebCore::GraphicsContextGLAttributes& attributes, GPUConnectionToWebProcess& connection, GraphicsContextGLIdentifier identifier)
    : RemoteGraphicsContextGL(attributes, connection, identifier, GraphicsContextGLOpenGL::createForGPUProcess(attributes, this))
{
}

GraphicsContextGLIOSurfaceSwapChain::Buffer RemoteGraphicsContextGLCocoa::recycleBuffer()
{
    return WTFMove(m_displayBuffer);
}

void RemoteGraphicsContextGLCocoa::present(Buffer buffer)
{
    ASSERT(!m_displayBuffer.surface);
    m_displayBuffer = WTFMove(buffer);
}

const GraphicsContextGLIOSurfaceSwapChain::Buffer& RemoteGraphicsContextGLCocoa::displayBuffer() const
{
    return m_displayBuffer;
}

void* RemoteGraphicsContextGLCocoa::detachClient()
{
    return std::exchange(m_displayBuffer.handle, nullptr);
}

void RemoteGraphicsContextGLCocoa::prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&& completionHandler)
{
    m_context->prepareForDisplay();
    auto displayBuffer = WTFMove(m_displayBuffer.surface);
    MachSendRight sendRight;
    if (displayBuffer)
        sendRight = displayBuffer->createSendRight();
    completionHandler(WTFMove(sendRight));
}

}

#endif
