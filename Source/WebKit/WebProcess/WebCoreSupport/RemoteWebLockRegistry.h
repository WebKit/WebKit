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
#include <WebCore/ProcessQualified.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/WebLockRegistry.h>
#include <wtf/HashMap.h>

namespace WebKit {

class WebProcess;

class RemoteWebLockRegistry final : public WebCore::WebLockRegistry, public IPC::MessageReceiver {
public:
    static Ref<RemoteWebLockRegistry> create(WebProcess& process) { return adoptRef(*new RemoteWebLockRegistry(process)); }
    ~RemoteWebLockRegistry();

    // WebCore::WebLockRegistry.
    void requestLock(PAL::SessionID, const WebCore::ClientOrigin&, WebCore::WebLockIdentifier, WebCore::ScriptExecutionContextIdentifier, const String& name, WebCore::WebLockMode, bool steal, bool ifAvailable, Function<void(bool)>&& grantedHandler, Function<void()>&& lockStolenHandler) final;
    void releaseLock(PAL::SessionID, const WebCore::ClientOrigin&, WebCore::WebLockIdentifier, WebCore::ScriptExecutionContextIdentifier, const String& name) final;
    void abortLockRequest(PAL::SessionID, const WebCore::ClientOrigin&, WebCore::WebLockIdentifier, WebCore::ScriptExecutionContextIdentifier, const String& name, CompletionHandler<void(bool)>&&) final;
    void snapshot(PAL::SessionID, const WebCore::ClientOrigin&, CompletionHandler<void(WebCore::WebLockManagerSnapshot&&)>&&) final;
    void clientIsGoingAway(PAL::SessionID, const WebCore::ClientOrigin&, WebCore::ScriptExecutionContextIdentifier) final;

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    explicit RemoteWebLockRegistry(WebProcess&);

    // IPC Message handlers.
    void didCompleteLockRequest(WebCore::WebLockIdentifier, WebCore::ScriptExecutionContextIdentifier, bool success);
    void didStealLock(WebCore::WebLockIdentifier, WebCore::ScriptExecutionContextIdentifier);

    struct LocksSnapshot;
    HashMap<WebCore::ScriptExecutionContextIdentifier, LocksSnapshot> m_locksSnapshotPerClient;
};

} // namespace WebKit
