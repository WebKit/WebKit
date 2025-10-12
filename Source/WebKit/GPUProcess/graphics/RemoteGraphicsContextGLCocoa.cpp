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
#include "RemoteSharedResourceCache.h"
#include <WebCore/ProcessIdentity.h>
#include <wtf/MachSendRight.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeap.h"
#include <WebCore/GraphicsContextGLCV.h>
#include <WebCore/MediaSampleAVFObjC.h>
#include <WebCore/VideoFrameCV.h>
#endif

namespace WebKit {

#if ENABLE(VIDEO)
void RemoteGraphicsContextGL::copyTextureFromVideoFrame(WebKit::SharedVideoFrame&& frame, PlatformGLObject texture, uint32_t target, int32_t level, uint32_t internalFormat, uint32_t format, uint32_t type, bool premultiplyAlpha, bool flipY, CompletionHandler<void(bool)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    RefPtr videoFrame = m_sharedVideoFrameReader.read(WTFMove(frame));
    if (!videoFrame) {
        ASSERT_IS_TESTING_IPC();
        completionHandler(false);
        return;
    }
    if (!m_objectNames.isValidKey(texture)) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    texture = m_objectNames.get(texture);
    bool result = protectedContext()->copyTextureFromVideoFrame(*videoFrame, texture, target, level, internalFormat, format, type, premultiplyAlpha, flipY);
    completionHandler(result);
}

void RemoteGraphicsContextGL::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTFMove(semaphore));
}

void RemoteGraphicsContextGL::setSharedVideoFrameMemory(SharedMemory::Handle&& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(WTFMove(handle));
}
#endif

namespace {

class RemoteGraphicsContextGLCocoa final : public RemoteGraphicsContextGL {
    WTF_MAKE_TZONE_ALLOCATED(RemoteGraphicsContextGLCocoa);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteGraphicsContextGLCocoa);

public:
    RemoteGraphicsContextGLCocoa(GPUConnectionToWebProcess&, RemoteGraphicsContextGLIdentifier, RemoteRenderingBackend&, Ref<IPC::StreamServerConnection>&&);
    ~RemoteGraphicsContextGLCocoa() final = default;

    // RemoteGraphicsContextGL overrides.
    void platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&&) final;
    void prepareForDisplay(IPC::Semaphore&&, CompletionHandler<void(WTF::MachSendRight&&)>&&) final;
private:
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteGraphicsContextGLCocoa);

}

Ref<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, WebCore::GraphicsContextGLAttributes&& attributes, RemoteGraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
{
    auto instance = adoptRef(*new RemoteGraphicsContextGLCocoa(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(streamConnection)));
    instance->initialize(WTFMove(attributes));
    return instance;
}

RemoteGraphicsContextGLCocoa::RemoteGraphicsContextGLCocoa(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
    : RemoteGraphicsContextGL(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(streamConnection))
{
}

void RemoteGraphicsContextGLCocoa::platformWorkQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    assertIsCurrent(workQueue());
    m_context = WebCore::GraphicsContextGLCocoa::create(WTFMove(attributes), WebCore::ProcessIdentity { m_sharedResourceCache->resourceOwner() });
}

void RemoteGraphicsContextGLCocoa::prepareForDisplay(IPC::Semaphore&& finishedSemaphore, CompletionHandler<void(WTF::MachSendRight&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    RefPtr context = m_context;

    context->prepareForDisplayWithFinishedSignal([finishedSemaphore = WTFMove(finishedSemaphore)]() mutable {
        finishedSemaphore.signal();
    });
    MachSendRight sendRight;
    if (WebCore::IOSurface* surface = context->displayBufferSurface())
        sendRight = surface->createSendRight();
    completionHandler(WTFMove(sendRight));
}

}

#endif
