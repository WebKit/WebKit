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

#include "config.h"
#include "RemoteWebLockRegistry.h"

#include "RemoteWebLockRegistryMessages.h"
#include "WebLockRegistryProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/WebLockManagerSnapshot.h>

namespace WebKit {

struct LockInfo {
    Function<void()> lockStolenHandler;
};

struct LockRequest : LockInfo {
    Function<void(bool)> lockGrantedHandler;
};

struct RemoteWebLockRegistry::LocksSnapshot {
    HashMap<WebCore::WebLockIdentifier, LockRequest> pendingRequests;
    HashMap<WebCore::WebLockIdentifier, LockInfo> heldLocks;

    bool isEmpty() const { return pendingRequests.isEmpty() && heldLocks.isEmpty(); }
};

RemoteWebLockRegistry::RemoteWebLockRegistry(WebProcess& process)
{
    process.addMessageReceiver(Messages::RemoteWebLockRegistry::messageReceiverName(), *this);
}

RemoteWebLockRegistry::~RemoteWebLockRegistry()
{
    WebProcess::singleton().removeMessageReceiver(*this);
}

void RemoteWebLockRegistry::requestLock(PAL::SessionID sessionID, const WebCore::ClientOrigin& clientOrigin, WebCore::WebLockIdentifier lockIdentifier, WebCore::ScriptExecutionContextIdentifier clientID, const String& name, WebCore::WebLockMode lockMode, bool steal, bool ifAvailable, Function<void(bool)>&& grantedHandler, Function<void()>&& lockStolenHandler)
{
    ASSERT_UNUSED(sessionID, WebProcess::singleton().sessionID());
    LockRequest request { { WTFMove(lockStolenHandler) }, WTFMove(grantedHandler) };
    auto& snapshot = m_locksSnapshotPerClient.ensure(clientID, [] { return LocksSnapshot { }; }).iterator->value;
    snapshot.pendingRequests.add(lockIdentifier, WTFMove(request));

    WebProcess::singleton().send(Messages::WebLockRegistryProxy::RequestLock(clientOrigin, lockIdentifier, clientID, name, lockMode, steal, ifAvailable), 0);
}

void RemoteWebLockRegistry::releaseLock(PAL::SessionID sessionID, const WebCore::ClientOrigin& clientOrigin, WebCore::WebLockIdentifier lockIdentifier, WebCore::ScriptExecutionContextIdentifier clientID, const String& name)
{
    ASSERT_UNUSED(sessionID, WebProcess::singleton().sessionID());
    auto it = m_locksSnapshotPerClient.find(clientID);
    if (it != m_locksSnapshotPerClient.end()) {
        it->value.heldLocks.remove(lockIdentifier);
        if (it->value.isEmpty())
            m_locksSnapshotPerClient.remove(it);
    }

    WebProcess::singleton().send(Messages::WebLockRegistryProxy::ReleaseLock(clientOrigin, lockIdentifier, clientID, name), 0);
}

void RemoteWebLockRegistry::abortLockRequest(PAL::SessionID sessionID, const WebCore::ClientOrigin& clientOrigin, WebCore::WebLockIdentifier lockIdentifier, WebCore::ScriptExecutionContextIdentifier clientID, const String& name, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT_UNUSED(sessionID, WebProcess::singleton().sessionID());
    WebProcess::singleton().sendWithAsyncReply(Messages::WebLockRegistryProxy::AbortLockRequest(clientOrigin, lockIdentifier, clientID, name), [weakThis = WeakPtr { *this }, lockIdentifier, clientID, completionHandler = WTFMove(completionHandler)](bool didAbort) mutable {
        if (weakThis && didAbort) {
            auto it = weakThis->m_locksSnapshotPerClient.find(clientID);
            if (it != weakThis->m_locksSnapshotPerClient.end()) {
                it->value.pendingRequests.remove(lockIdentifier);
                if (it->value.isEmpty())
                    weakThis->m_locksSnapshotPerClient.remove(it);
            }
        }
        completionHandler(didAbort);
    }, 0);
}

void RemoteWebLockRegistry::snapshot(PAL::SessionID sessionID, const WebCore::ClientOrigin& clientOrigin, CompletionHandler<void(WebCore::WebLockManagerSnapshot&&)>&& completionHandler)
{
    ASSERT_UNUSED(sessionID, WebProcess::singleton().sessionID());
    WebProcess::singleton().sendWithAsyncReply(Messages::WebLockRegistryProxy::Snapshot(clientOrigin), WTFMove(completionHandler), 0);
}

void RemoteWebLockRegistry::clientIsGoingAway(PAL::SessionID sessionID, const WebCore::ClientOrigin& clientOrigin, WebCore::ScriptExecutionContextIdentifier clientID)
{
    ASSERT_UNUSED(sessionID, WebProcess::singleton().sessionID());
    m_locksSnapshotPerClient.remove(clientID);
    WebProcess::singleton().send(Messages::WebLockRegistryProxy::ClientIsGoingAway(clientOrigin, clientID), 0);
}

void RemoteWebLockRegistry::didCompleteLockRequest(WebCore::WebLockIdentifier lockIdentifier, WebCore::ScriptExecutionContextIdentifier clientID, bool success)
{
    auto it = m_locksSnapshotPerClient.find(clientID);
    if (it == m_locksSnapshotPerClient.end())
        return;

    auto pendingRequest = it->value.pendingRequests.take(lockIdentifier);
    if (!pendingRequest.lockGrantedHandler)
        return;

    auto lockGrantedHandler = WTFMove(pendingRequest.lockGrantedHandler);
    if (success)
        it->value.heldLocks.add(lockIdentifier, WTFMove(pendingRequest));
    else {
        if (it->value.isEmpty())
            m_locksSnapshotPerClient.remove(it);
    }

    lockGrantedHandler(success);
}

void RemoteWebLockRegistry::didStealLock(WebCore::WebLockIdentifier lockIdentifier, WebCore::ScriptExecutionContextIdentifier clientID)
{
    auto it = m_locksSnapshotPerClient.find(clientID);
    if (it == m_locksSnapshotPerClient.end())
        return;

    auto lockInfo = it->value.heldLocks.take(lockIdentifier);
    if (!lockInfo.lockStolenHandler)
        return;

    if (it->value.isEmpty())
        m_locksSnapshotPerClient.remove(it);

    lockInfo.lockStolenHandler();
}

} // namespace WebKit
