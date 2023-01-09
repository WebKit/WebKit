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
#include "IPCUtilities.h"
#include <wtf/MachSendRight.h>


#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeap.h"
#include <WebCore/GraphicsContextGLCV.h>
#include <WebCore/MediaSampleAVFObjC.h>
#include <WebCore/VideoFrameCV.h>
#endif

namespace WebKit {

#if ENABLE(VIDEO)
void RemoteGraphicsContextGL::copyTextureFromVideoFrame(WebKit::SharedVideoFrame&& frame, uint32_t texture, uint32_t target, int32_t level, uint32_t internalFormat, uint32_t format, uint32_t type, bool premultiplyAlpha, bool flipY, CompletionHandler<void(bool)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    UNUSED_VARIABLE(premultiplyAlpha);
    ASSERT_UNUSED(target, target == WebCore::GraphicsContextGL::TEXTURE_2D);

    auto videoFrame = m_sharedVideoFrameReader.read(WTFMove(frame));
    if (!videoFrame) {
        ASSERT_IS_TESTING_IPC();
        completionHandler(false);
        return;
    }

    auto videoFrameCV = videoFrame->asVideoFrameCV();
    if (!videoFrameCV) {
        ASSERT_NOT_REACHED(); // Programming error, not a IPC attack.
        completionHandler(false);
        return;
    }

    auto contextCV = m_context->asCV();
    if (!contextCV) {
        ASSERT_NOT_REACHED();
        completionHandler(false);
        return;
    }

    completionHandler(contextCV->copyVideoSampleToTexture(*videoFrameCV, texture, level, internalFormat, format, type, WebCore::GraphicsContextGL::FlipY(flipY)));
}

void RemoteGraphicsContextGL::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTFMove(semaphore));
}

void RemoteGraphicsContextGL::setSharedVideoFrameMemory(const SharedMemory::Handle& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(handle);
}
#endif

namespace {

class RemoteGraphicsContextGLCocoa final : public RemoteGraphicsContextGL {
public:
    RemoteGraphicsContextGLCocoa(GPUConnectionToWebProcess&, GraphicsContextGLIdentifier, RemoteRenderingBackend&, IPC::StreamServerConnection::Handle&&);
    ~RemoteGraphicsContextGLCocoa() final = default;

    // RemoteGraphicsContextGL overrides.
    void platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&&) final;
    void prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&&) final;
private:
};

}

Ref<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, WebCore::GraphicsContextGLAttributes&& attributes, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, IPC::StreamServerConnection::Handle&& connectionHandle)
{
    auto instance = adoptRef(*new RemoteGraphicsContextGLCocoa(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(connectionHandle)));
    instance->initialize(WTFMove(attributes));
    return instance;
}

RemoteGraphicsContextGLCocoa::RemoteGraphicsContextGLCocoa(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, IPC::StreamServerConnection::Handle&& connectionHandle)
    : RemoteGraphicsContextGL(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(connectionHandle))
{
}

void RemoteGraphicsContextGLCocoa::platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    assertIsCurrent(workQueue());
    m_context = WebCore::GraphicsContextGLCocoa::create(WTFMove(attributes), WebCore::ProcessIdentity { m_resourceOwner });
}

void RemoteGraphicsContextGLCocoa::prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    m_context->prepareForDisplay();
    MachSendRight sendRight;
    WebCore::IOSurface* displayBuffer = m_context->displayBuffer();
    if (displayBuffer) {
        m_context->markDisplayBufferInUse();
        sendRight = displayBuffer->createSendRight();
    }
    completionHandler(WTFMove(sendRight));
}

}

#endif
