/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#include "Connection.h"
#include "RemoteRenderingBackendCreationParameters.h"
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WorkerClient.h>

namespace WebCore::WebGPU {
class GPU;
}

namespace WebKit {

class WebPage;
class RemoteRenderingBackendProxy;

class WebWorkerClient : public WebCore::WorkerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // Constructed on the main thread, and then transferred to the
    // worker thread. All further operations on this object will
    // happen on the worker.
    // Any details needed from the page must be copied at this
    // point, but can't hold references to any main-thread objects.
    static UniqueRef<WebWorkerClient> create(WebPage&, SerialFunctionDispatcher&);

    UniqueRef<WorkerClient> createNestedWorkerClient(SerialFunctionDispatcher&) final;

    WebCore::PlatformDisplayID displayID() const final;

    RefPtr<WebCore::ImageBuffer> sinkIntoImageBuffer(std::unique_ptr<WebCore::SerializedImageBuffer>) final;
    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::PixelFormat, OptionSet<WebCore::ImageBufferOptions>) const final;
#if ENABLE(WEBGL)
    RefPtr<WebCore::GraphicsContextGL> createGraphicsContextGL(const WebCore::GraphicsContextGLAttributes&) const final;
#endif

#if HAVE(WEBGPU_IMPLEMENTATION)
    RefPtr<WebCore::WebGPU::GPU> createGPUForWebGPU() const final;
#endif

private:
    WebWorkerClient(SerialFunctionDispatcher&, WebCore::PlatformDisplayID
#if ENABLE(GPU_PROCESS)
        , Ref<IPC::Connection>, WebPageProxyIdentifier, WebCore::PageIdentifier
#if ENABLE(VIDEO)
        , Ref<RemoteVideoFrameObjectHeapProxy>
#endif
#endif
        ); //  NOLINT

#if ENABLE(GPU_PROCESS)
    RemoteRenderingBackendProxy& ensureRenderingBackend() const;
#endif

    SerialFunctionDispatcher& m_dispatcher;
    const WebCore::PlatformDisplayID m_displayID;
#if ENABLE(GPU_PROCESS)
    Ref<IPC::Connection> m_connection;
    mutable std::unique_ptr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
    const WebPageProxyIdentifier m_pageProxyID;
    const WebCore::PageIdentifier m_pageID;
#if ENABLE(VIDEO)
    Ref<RemoteVideoFrameObjectHeapProxy> m_videoFrameObjectHeapProxy;
#endif
#endif
};

} // namespace WebKit
