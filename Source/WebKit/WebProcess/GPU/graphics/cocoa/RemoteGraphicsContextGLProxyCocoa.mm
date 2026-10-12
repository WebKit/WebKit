/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteGraphicsContextGLProxy.h"

#import "RemoteRenderingBackendProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)
#import "DisplayBufferDisplayDelegate.h"
#import "GPUConnectionToWebProcess.h"
#import "GPUProcessConnection.h"
#import "RemoteGraphicsContextGLMessages.h"
#import "WebProcess.h"
#import <WebCore/CVUtilities.h>
#import <WebCore/GraphicsLayerContentsDisplayDelegate.h>
#import <WebCore/IOSurface.h>

namespace WebKit {

namespace {

class RemoteGraphicsContextGLProxyCocoa final : public RemoteGraphicsContextGLProxy {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(RemoteGraphicsContextGLProxyCocoa);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteGraphicsContextGLProxyCocoa);
public:
    // RemoteGraphicsContextGLProxy overrides.
    RefPtr<WebCore::GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final { return m_layerContentsDisplayDelegate.ptr(); }
    void prepareForDisplay() final;
    void forceContextLost() final;

private:
    explicit RemoteGraphicsContextGLProxyCocoa(const WebCore::GraphicsContextGLAttributes& attributes, RemoteRenderingBackendProxy& renderingBackend)
        : RemoteGraphicsContextGLProxy(attributes, renderingBackend)
        , m_layerContentsDisplayDelegate(DisplayBufferDisplayDelegate::create(!attributes.alpha, WebCore::GraphicsLayerCompositingCoordinatesOrientation::BottomUp))
    {
    }

    void addNewFence(Ref<DisplayBufferFence> newFence);
    static constexpr size_t maxPendingFences = 3;
    size_t m_oldestFenceIndex { 0 };
    std::array<RefPtr<DisplayBufferFence>, maxPendingFences> m_frameCompletionFences;

    const Ref<DisplayBufferDisplayDelegate> m_layerContentsDisplayDelegate;
    friend class RemoteGraphicsContextGLProxy;
};

void RemoteGraphicsContextGLProxyCocoa::prepareForDisplay()
{
    if (isContextLost())
        return;
    IPC::Semaphore finishedSignaller;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PrepareForDisplay(finishedSignaller));
    if (!sendResult.succeeded()) {
        markContextLost();
        return;
    }
    auto [displayBufferSendRight] = sendResult.takeReply();
    if (!displayBufferSendRight)
        return;
    m_hasPreparedForDisplay = true;
    auto finishedFence = DisplayBufferFence::create(WTF::move(finishedSignaller));
    addNewFence(finishedFence);
    m_layerContentsDisplayDelegate->setDisplayBuffer(WTF::move(displayBufferSendRight), WTF::move(finishedFence));
}

void RemoteGraphicsContextGLProxyCocoa::forceContextLost()
{
    for (auto fence : m_frameCompletionFences) {
        if (fence)
            fence->forceSignal();
    }
    RemoteGraphicsContextGLProxy::forceContextLost();
}

void RemoteGraphicsContextGLProxyCocoa::addNewFence(Ref<DisplayBufferFence> newFence)
{
    // Record the pending fences so that they can be force signaled when context is lost.
    size_t oldestFenceIndex = m_oldestFenceIndex++ % maxPendingFences;
    std::exchange(m_frameCompletionFences[oldestFenceIndex], WTF::move(newFence));
    // Due to the fence being IPC::Semaphore, we do not have very good way of waiting the fence in two places, compositor and here.
    // Thus we just record maxPendingFences and trust that compositor does not advance too far with multiple frames.
}

}

Ref<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::platformCreate(const WebCore::GraphicsContextGLAttributes& attributes, RemoteRenderingBackendProxy& renderingBackend)
{
    return adoptRef(*new RemoteGraphicsContextGLProxyCocoa(attributes, renderingBackend));
}

}

#endif
