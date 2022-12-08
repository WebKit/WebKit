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
#include <WebCore/WorkerClient.h>

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
    WebWorkerClient(WebPage*, SerialFunctionDispatcher&);

    // Used for constructing clients for nested workers. Created on the
    // worker thread of the outer worker, and then transferred to the
    // nested worker.
#if ENABLE(GPU_PROCESS)
    WebWorkerClient(IPC::Connection&, SerialFunctionDispatcher&, RemoteRenderingBackendCreationParameters&, WebCore::PlatformDisplayID&);
#else
    WebWorkerClient(SerialFunctionDispatcher&, WebCore::PlatformDisplayID&);
#endif

    std::unique_ptr<WorkerClient> clone(SerialFunctionDispatcher&) final;

    WebCore::PlatformDisplayID displayID() const final;

    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::PixelFormat, bool avoidBackendSizeCheck = false) const final;
#if ENABLE(WEBGL)
    RefPtr<WebCore::GraphicsContextGL> createGraphicsContextGL(const WebCore::GraphicsContextGLAttributes&) const final;
#endif

private:
#if ENABLE(GPU_PROCESS)
    RemoteRenderingBackendProxy& ensureRenderingBackend() const;
#endif

    SerialFunctionDispatcher& m_dispatcher;
#if ENABLE(GPU_PROCESS)
    Ref<IPC::Connection> m_connection;
    mutable std::unique_ptr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
    RemoteRenderingBackendCreationParameters m_creationParameters;
#endif
    WebCore::PlatformDisplayID m_displayID;
};

} // namespace WebKit
