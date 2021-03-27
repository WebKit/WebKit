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

#pragma once

#if ENABLE(WEB_AUTHN)

#include "AuxiliaryProcessProxy.h"
#include "ProcessLauncher.h"
#include "ProcessThrottler.h"
#include "ProcessThrottlerClient.h"
#include "WebPageProxyIdentifier.h"
#include "WebProcessProxyMessagesReplies.h"
#include <memory>

namespace WebKit {

class WebProcessProxy;
class WebsiteDataStore;
struct WebAuthnProcessCreationParameters;

class WebAuthnProcessProxy final : public AuxiliaryProcessProxy, private ProcessThrottlerClient {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WebAuthnProcessProxy);
    friend LazyNeverDestroyed<WebAuthnProcessProxy>;
public:
    static WebAuthnProcessProxy& singleton();

    void getWebAuthnProcessConnection(WebProcessProxy&, Messages::WebProcessProxy::GetWebAuthnProcessConnectionDelayedReply&&);

    ProcessThrottler& throttler() final { return m_throttler; }
    void updateProcessAssertion();

    // ProcessThrottlerClient
    void sendProcessDidResume() final { }
    ASCIILiteral clientName() const final { return "WebAuthnProcess"_s; }

private:
    explicit WebAuthnProcessProxy();
    ~WebAuthnProcessProxy();

    // AuxiliaryProcessProxy
    ASCIILiteral processName() const final { return "WebAuthn"_s; }

    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;

    void webAuthnProcessCrashed();

    // ProcessThrottlerClient
    void sendPrepareToSuspend(IsSuspensionImminent, CompletionHandler<void()>&& completionHandler) final { completionHandler(); }

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    // IPC::Connection::Client
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override;

    ProcessThrottler m_throttler;
    ProcessThrottler::ActivityVariant m_activityFromWebProcesses;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
