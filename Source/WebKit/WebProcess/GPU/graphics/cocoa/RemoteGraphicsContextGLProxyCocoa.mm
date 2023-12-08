/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "GPUConnectionToWebProcess.h"
#import "GPUProcessConnection.h"
#import "RemoteGraphicsContextGLMessages.h"
#import "WebProcess.h"
#import <WebCore/CVUtilities.h>
#import <WebCore/GraphicsLayerContentsDisplayDelegate.h>
#import <WebCore/IOSurface.h>
#import <WebCore/PlatformCALayer.h>
#import <WebCore/PlatformCALayerDelegatedContents.h>

namespace WebKit {

namespace {

class DisplayBufferFence final : public WebCore::PlatformCALayerDelegatedContentsFence {
public:
    static Ref<DisplayBufferFence> create(IPC::Semaphore&& finishedFenceSemaphore)
    {
        return adoptRef(*new DisplayBufferFence(WTFMove(finishedFenceSemaphore)));
    }

    bool waitFor(Seconds timeout) final
    {
        Locker locker { m_lock };
        if (m_signaled)
            return true;
        m_signaled = m_semaphore.waitFor(timeout);
        return m_signaled;
    }

    void forceSignal()
    {
        Locker locker { m_lock };
        if (m_signaled)
            return;
        m_signaled = true;
        m_semaphore.signal();
    }

private:
    DisplayBufferFence(IPC::Semaphore&& finishedFenceSemaphore)
        : m_semaphore(WTFMove(finishedFenceSemaphore))
    {
    }

    Lock m_lock;
    bool m_signaled WTF_GUARDED_BY_LOCK(m_lock) { false };
    IPC::Semaphore m_semaphore;
};

class DisplayBufferDisplayDelegate final : public WebCore::GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<DisplayBufferDisplayDelegate> create(bool isOpaque)
    {
        return adoptRef(*new DisplayBufferDisplayDelegate(isOpaque));
    }

    // WebCore::GraphicsLayerContentsDisplayDelegate overrides.
    void prepareToDelegateDisplay(WebCore::PlatformCALayer& layer) final
    {
        layer.setOpaque(m_isOpaque);
    }

    void display(WebCore::PlatformCALayer& layer) final
    {
        if (m_displayBuffer)
            layer.setDelegatedContents({ MachSendRight { m_displayBuffer }, m_finishedFence, std::nullopt });
        else
            layer.clearContents();
    }

    WebCore::GraphicsLayer::CompositingCoordinatesOrientation orientation() const final
    {
        return WebCore::GraphicsLayer::CompositingCoordinatesOrientation::BottomUp;
    }

    void setDisplayBuffer(MachSendRight&& displayBuffer, RefPtr<DisplayBufferFence> finishedFence)
    {
        if (!displayBuffer) {
            m_finishedFence = nullptr;
            m_displayBuffer = { };
            return;
        }
        if (m_displayBuffer && displayBuffer.sendRight() == m_displayBuffer.sendRight())
            return;
        m_finishedFence = WTFMove(finishedFence);
        m_displayBuffer = WTFMove(displayBuffer);
    }

private:
    DisplayBufferDisplayDelegate(bool isOpaque)
        : m_isOpaque(isOpaque)
    {
    }

    MachSendRight m_displayBuffer;
    RefPtr<DisplayBufferFence> m_finishedFence;
    const bool m_isOpaque;
};

class RemoteGraphicsContextGLProxyCocoa final : public RemoteGraphicsContextGLProxy {
public:
    // GraphicsContextGL override.
    std::optional<WebCore::GraphicsContextGL::EGLImageAttachResult> createAndBindEGLImage(GCGLenum, WebCore::GraphicsContextGL::EGLImageSource) final;
    GCEGLSync createEGLSync(ExternalEGLSyncEvent) final;

    // RemoteGraphicsContextGLProxy overrides.
    RefPtr<WebCore::GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final { return m_layerContentsDisplayDelegate.ptr(); }
    void prepareForDisplay() final;
    void forceContextLost() final;
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    WebCore::GraphicsContextGLCV* asCV() final { return nullptr; }
#endif
private:
    RemoteGraphicsContextGLProxyCocoa(IPC::Connection& connection,  Ref<IPC::StreamClientConnection> streamConnection, const WebCore::GraphicsContextGLAttributes& attributes, Ref<RemoteVideoFrameObjectHeapProxy>&& videoFrameObjectHeapProxy)
        : RemoteGraphicsContextGLProxy(connection, WTFMove(streamConnection), attributes, WTFMove(videoFrameObjectHeapProxy))
        , m_layerContentsDisplayDelegate(DisplayBufferDisplayDelegate::create(!attributes.alpha))
    {
    }
    void addNewFence(Ref<DisplayBufferFence> newFence);
    static constexpr size_t maxPendingFences = 3;
    size_t m_oldestFenceIndex { 0 };
    RefPtr<DisplayBufferFence> m_frameCompletionFences[maxPendingFences];

    Ref<DisplayBufferDisplayDelegate> m_layerContentsDisplayDelegate;
    friend class RemoteGraphicsContextGLProxy;
};

std::optional<WebCore::GraphicsContextGL::EGLImageAttachResult> RemoteGraphicsContextGLProxyCocoa::createAndBindEGLImage(GCGLenum target, WebCore::GraphicsContextGL::EGLImageSource source)
{
    if (isContextLost())
        return std::nullopt;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::CreateAndBindEGLImage(target, WTFMove(source)));
    if (!sendResult.succeeded()) {
        markContextLost();
        return std::nullopt;
    }
    auto [handle, size] = sendResult.takeReply();
    if (!handle)
        return std::nullopt;
    return std::make_tuple(reinterpret_cast<GCEGLImage>(static_cast<intptr_t>(handle)), size);
}

GCEGLSync RemoteGraphicsContextGLProxyCocoa::createEGLSync(ExternalEGLSyncEvent syncEvent)
{
    if (isContextLost())
        return { };
    auto [eventHandle, signalValue] = syncEvent;
    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::CreateEGLSync(WTFMove(eventHandle), signalValue));
    if (!sendResult.succeeded()) {
        markContextLost();
        return { };
    }
    auto& [returnValue] = sendResult.reply();
    return reinterpret_cast<GCEGLSync>(static_cast<intptr_t>(returnValue));
}

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
    auto finishedFence = DisplayBufferFence::create(WTFMove(finishedSignaller));
    addNewFence(finishedFence);
    m_layerContentsDisplayDelegate->setDisplayBuffer(WTFMove(displayBufferSendRight), WTFMove(finishedFence));
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
    std::exchange(m_frameCompletionFences[oldestFenceIndex], WTFMove(newFence));
    // Due to the fence being IPC::Semaphore, we do not have very good way of waiting the fence in two places, compositor and here.
    // Thus we just record maxPendingFences and trust that compositor does not advance too far with multiple frames.
}

}

Ref<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::platformCreate(IPC::Connection& connection,  Ref<IPC::StreamClientConnection> streamConnection, const WebCore::GraphicsContextGLAttributes& attributes, Ref<RemoteVideoFrameObjectHeapProxy>&& videoFrameObjectHeapProxy)
{
    return adoptRef(*new RemoteGraphicsContextGLProxyCocoa(connection, WTFMove(streamConnection), attributes, WTFMove(videoFrameObjectHeapProxy)));
}

}

#endif
