/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "MessageSender.h"
#include "ProcessThrottler.h"
#include "WebBackForwardListItem.h"
#include "WebProcessProxy.h"
#include <WebCore/FrameIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class RegistrableDomain;
}

namespace WebKit {

class WebBackForwardCache;
class WebPageProxy;
class WebProcessPool;
class WebsiteDataStore;

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
using LayerHostingContextID = uint32_t;
#endif

enum class ShouldDelayClosingUntilFirstLayerFlush : bool { No, Yes };

class SuspendedPageProxy final: public IPC::MessageReceiver, public IPC::MessageSender {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SuspendedPageProxy(WebPageProxy&, Ref<WebProcessProxy>&&, Ref<WebFrameProxy>&& mainFrame, ShouldDelayClosingUntilFirstLayerFlush);
    ~SuspendedPageProxy();

    static RefPtr<WebProcessProxy> findReusableSuspendedPageProcess(WebProcessPool&, const WebCore::RegistrableDomain&, WebsiteDataStore&, WebProcessProxy::LockdownMode);

    WebPageProxy& page() const { return m_page; }
    WebCore::PageIdentifier webPageID() const { return m_webPageID; }
    WebProcessProxy& process() const { return m_process.get(); }
    WebFrameProxy& mainFrame() { return m_mainFrame.get(); }

    WebBackForwardCache& backForwardCache() const;

    bool pageIsClosedOrClosing() const;

    void waitUntilReadyToUnsuspend(CompletionHandler<void(SuspendedPageProxy*)>&&);
    void unsuspend();

    void pageDidFirstLayerFlush();
    void closeWithoutFlashing();

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    LayerHostingContextID contextIDForVisibilityPropagationInWebProcess() const { return m_contextIDForVisibilityPropagationInWebProcess; }
#if ENABLE(GPU_PROCESS)
    LayerHostingContextID contextIDForVisibilityPropagationInGPUProcess() const { return m_contextIDForVisibilityPropagationInGPUProcess; }
#endif
#endif

#if !LOG_DISABLED
    const char* loggingString() const;
#endif

private:
    enum class SuspensionState : uint8_t { Suspending, FailedToSuspend, Suspended, Resumed };
    void didProcessRequestToSuspend(SuspensionState);
    void suspensionTimedOut();

    void close();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;
    bool sendMessage(UniqueRef<IPC::Encoder>&&, OptionSet<IPC::SendOption>) final;
    bool sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&&, AsyncReplyHandler, OptionSet<IPC::SendOption>) final;

    WebPageProxy& m_page;
    WebCore::PageIdentifier m_webPageID;
    Ref<WebProcessProxy> m_process;
    Ref<WebFrameProxy> m_mainFrame;
    bool m_isClosed { false };
    ShouldDelayClosingUntilFirstLayerFlush m_shouldDelayClosingUntilFirstLayerFlush { ShouldDelayClosingUntilFirstLayerFlush::No };
    bool m_shouldCloseWhenEnteringAcceleratedCompositingMode { false };

    SuspensionState m_suspensionState { SuspensionState::Suspending };
    CompletionHandler<void(SuspendedPageProxy*)> m_readyToUnsuspendHandler;
    RunLoop::Timer m_suspensionTimeoutTimer;
#if USE(RUNNINGBOARD)
    std::unique_ptr<ProcessThrottler::BackgroundActivity> m_suspensionActivity;
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    LayerHostingContextID m_contextIDForVisibilityPropagationInWebProcess { 0 };
#if ENABLE(GPU_PROCESS)
    LayerHostingContextID m_contextIDForVisibilityPropagationInGPUProcess { 0 };
#endif
#endif
};

} // namespace WebKit
