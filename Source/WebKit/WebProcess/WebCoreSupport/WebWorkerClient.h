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
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WorkerClient.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class Page;
}

namespace WebCore::WebGPU {
class GPU;
}

namespace WebKit {

class WebPage;

class WebWorkerClient : public WebCore::WorkerClient {
    WTF_MAKE_TZONE_ALLOCATED(WebWorkerClient);
public:
    ~WebWorkerClient();
    // Constructed on the main thread, and then transferred to the
    // worker thread. All further operations on this object will
    // happen on the worker.
    // Any details needed from the page must be copied at this
    // point, but can't hold references to any main-thread objects.
    static UniqueRef<WebWorkerClient> create(WebCore::Page&, SerialFunctionDispatcher&);

    UniqueRef<WorkerClient> createNestedWorkerClient(SerialFunctionDispatcher&) override;

    WebCore::PlatformDisplayID displayID() const final;

    RefPtr<WebCore::ImageBuffer> sinkIntoImageBuffer(std::unique_ptr<WebCore::SerializedImageBuffer>) override;
    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::ImageBufferPixelFormat) const override;
#if ENABLE(WEBGL)
    RefPtr<WebCore::GraphicsContextGL> createGraphicsContextGL(const WebCore::GraphicsContextGLAttributes&) const override;
#endif

#if HAVE(WEBGPU_IMPLEMENTATION)
    RefPtr<WebCore::WebGPU::GPU> createGPUForWebGPU() const override;
#endif

protected:
    WebWorkerClient(SerialFunctionDispatcher&, WebCore::PlatformDisplayID);

    // m_dispatcher should stay alive as long as WebWorkerClient is alive.
    RefPtr<SerialFunctionDispatcher> dispatcher() const { return m_dispatcher.get(); }

    ThreadSafeWeakPtr<SerialFunctionDispatcher> m_dispatcher;
    const WebCore::PlatformDisplayID m_displayID;
};

} // namespace WebKit
