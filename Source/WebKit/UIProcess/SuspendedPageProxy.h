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
#include "WebPageProxyMessagesReplies.h"
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
class WebProcessProxy;
class WebsiteDataStore;

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
using LayerHostingContextID = uint32_t;
#endif

enum class ShouldDelayClosingUntilEnteringAcceleratedCompositingMode : bool { No, Yes };

class SuspendedPageProxy final: public IPC::MessageReceiver, public IPC::MessageSender, public CanMakeWeakPtr<SuspendedPageProxy> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SuspendedPageProxy(WebPageProxy&, Ref<WebProcessProxy>&&, WebCore::FrameIdentifier mainFrameID, ShouldDelayClosingUntilEnteringAcceleratedCompositingMode);
    ~SuspendedPageProxy();

    static RefPtr<WebProcessProxy> findReusableSuspendedPageProcess(WebProcessPool&, const WebCore::RegistrableDomain&, WebsiteDataStore&);

    WebPageProxy& page() const { return m_page; }
    WebCore::PageIdentifier webPageID() const { return m_webPageID; }
    WebProcessProxy& process() const { return m_process.get(); }
    WebCore::FrameIdentifier mainFrameID() const { return m_mainFrameID; }

    WebBackForwardCache& backForwardCache() const;

    bool pageIsClosedOrClosing() const;

    void waitUntilReadyToUnsuspend(CompletionHandler<void(SuspendedPageProxy*)>&&);
    void unsuspend();

    void pageEnteredAcceleratedCompositingMode();
    void closeWithoutFlashing();

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    LayerHostingContextID contextIDForVisibilityPropagation() const { return m_contextIDForVisibilityPropagation; }
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
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;
    bool sendMessage(std::unique_ptr<IPC::Encoder>, OptionSet<IPC::SendOption>, Optional<std::pair<CompletionHandler<void(IPC::Decoder*)>, uint64_t>>&&) final;

    WebPageProxy& m_page;
    WebCore::PageIdentifier m_webPageID;
    Ref<WebProcessProxy> m_process;
    WebCore::FrameIdentifier m_mainFrameID;
    bool m_isClosed { false };
    ShouldDelayClosingUntilEnteringAcceleratedCompositingMode m_shouldDelayClosingUntilEnteringAcceleratedCompositingMode { ShouldDelayClosingUntilEnteringAcceleratedCompositingMode::No };
    bool m_shouldCloseWhenEnteringAcceleratedCompositingMode { false };

    SuspensionState m_suspensionState { SuspensionState::Suspending };
    CompletionHandler<void(SuspendedPageProxy*)> m_readyToUnsuspendHandler;
    RunLoop::Timer<SuspendedPageProxy> m_suspensionTimeoutTimer;
#if PLATFORM(IOS_FAMILY)
    std::unique_ptr<ProcessThrottler::BackgroundActivity> m_suspensionActivity;
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    LayerHostingContextID m_contextIDForVisibilityPropagation { 0 };
#endif
};

} // namespace WebKit
