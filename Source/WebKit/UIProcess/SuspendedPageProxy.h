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
#include "ProcessThrottler.h"
#include "WebBackForwardListItem.h"
#include <WebCore/SecurityOriginData.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class WebPageProxy;
class WebProcessProxy;

class SuspendedPageProxy final: public IPC::MessageReceiver, public CanMakeWeakPtr<SuspendedPageProxy> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SuspendedPageProxy(WebPageProxy&, Ref<WebProcessProxy>&&, WebBackForwardListItem&, uint64_t mainFrameID);
    ~SuspendedPageProxy();

    WebPageProxy& page() const { return m_page; }
    WebProcessProxy& process() { return m_process.get(); }
    uint64_t mainFrameID() const { return m_mainFrameID; }
    const String& registrableDomain() const { return m_registrableDomain; }

    bool failedToSuspend() const { return m_suspensionState == SuspensionState::FailedToSuspend; }

    void waitUntilReadyToUnsuspend(CompletionHandler<void(SuspendedPageProxy*)>&&);
    void unsuspend();
    void close();

#if !LOG_DISABLED
    const char* loggingString() const;
#endif

private:
    enum class SuspensionState : uint8_t { Suspending, FailedToSuspend, Suspended, Resumed };
    void didProcessRequestToSuspend(SuspensionState);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

    WebPageProxy& m_page;
    Ref<WebProcessProxy> m_process;
    uint64_t m_mainFrameID;
    String m_registrableDomain;
    bool m_isClosed { false };

    SuspensionState m_suspensionState { SuspensionState::Suspending };
    CompletionHandler<void(SuspendedPageProxy*)> m_readyToUnsuspendHandler;
#if PLATFORM(IOS_FAMILY)
    ProcessThrottler::BackgroundActivityToken m_suspensionToken;
#endif
};

} // namespace WebKit
