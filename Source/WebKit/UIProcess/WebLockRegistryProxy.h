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

#pragma once

#include "MessageReceiver.h"
#include "WebProcessProxy.h"
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/WebLockIdentifier.h>
#include <WebCore/WebLockMode.h>
#include <wtf/TZoneMalloc.h>

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::WebLockRegistryProxy> : std::true_type { };
}

namespace WebCore {
struct ClientOrigin;
struct WebLockManagerSnapshot;
}

namespace WebKit {

class WebLockRegistryProxy;
struct SharedPreferencesForWebProcess;

class WebLockRegistryProxy final : public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(WebLockRegistryProxy);
public:
    explicit WebLockRegistryProxy(WebProcessProxy&);
    ~WebLockRegistryProxy();

    const SharedPreferencesForWebProcess& sharedPreferencesForWebProcess() { return m_process.sharedPreferencesForWebProcess(); }

    void processDidExit();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    // IPC Message handlers.
    void requestLock(WebCore::ClientOrigin&&, WebCore::WebLockIdentifier, WebCore::ScriptExecutionContextIdentifier, String&& name, WebCore::WebLockMode, bool steal, bool ifAvailable);
    void releaseLock(WebCore::ClientOrigin&&, WebCore::WebLockIdentifier, WebCore::ScriptExecutionContextIdentifier, String&& name);
    void abortLockRequest(WebCore::ClientOrigin&&, WebCore::WebLockIdentifier, WebCore::ScriptExecutionContextIdentifier, String&& name, CompletionHandler<void(bool)>&&);
    void snapshot(WebCore::ClientOrigin&&, CompletionHandler<void(WebCore::WebLockManagerSnapshot&&)>&&);
    void clientIsGoingAway(WebCore::ClientOrigin&&, WebCore::ScriptExecutionContextIdentifier);

    WebProcessProxy& m_process;
    bool m_hasEverRequestedLocks { false };
};

} // namespace WebKit

