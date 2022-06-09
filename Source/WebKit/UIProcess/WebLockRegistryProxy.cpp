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
#include "WebLockRegistryProxy.h"

#include "Connection.h"
#include "RemoteWebLockRegistryMessages.h"
#include "WebLockRegistryProxyMessages.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/WebLockIdentifier.h>
#include <WebCore/WebLockRegistry.h>

namespace WebKit {

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_process.connection())

WebLockRegistryProxy::WebLockRegistryProxy(WebProcessProxy& process)
    : m_process(process)
{
    m_process.addMessageReceiver(Messages::WebLockRegistryProxy::messageReceiverName(), *this);
}

WebLockRegistryProxy::~WebLockRegistryProxy()
{
    m_process.removeMessageReceiver(Messages::WebLockRegistryProxy::messageReceiverName());
}

void WebLockRegistryProxy::requestLock(WebCore::ClientOrigin&& clientOrigin, WebCore::WebLockIdentifier lockIdentifier, WebCore::ScriptExecutionContextIdentifier clientID, String&& name, WebCore::WebLockMode lockMode, bool steal, bool ifAvailable)
{
    MESSAGE_CHECK(lockIdentifier.processIdentifier() == m_process.coreProcessIdentifier());
    MESSAGE_CHECK(clientID.processIdentifier() == m_process.coreProcessIdentifier());
    m_hasEverRequestedLocks = true;

    m_process.websiteDataStore().webLockRegistry().requestLock(WTFMove(clientOrigin), lockIdentifier, clientID, WTFMove(name), lockMode, steal, ifAvailable, [weakThis = WeakPtr { *this }, lockIdentifier, clientID](bool success) {
        if (weakThis)
            weakThis->m_process.send(Messages::RemoteWebLockRegistry::DidCompleteLockRequest(lockIdentifier, clientID, success), 0);
    }, [weakThis = WeakPtr { *this }, lockIdentifier, clientID] {
        if (weakThis)
            weakThis->m_process.send(Messages::RemoteWebLockRegistry::DidStealLock(lockIdentifier, clientID), 0);
    });
}

void WebLockRegistryProxy::releaseLock(WebCore::ClientOrigin&& clientOrigin, WebCore::WebLockIdentifier lockIdentifier, WebCore::ScriptExecutionContextIdentifier clientID, String&& name)
{
    MESSAGE_CHECK(lockIdentifier.processIdentifier() == m_process.coreProcessIdentifier());
    MESSAGE_CHECK(clientID.processIdentifier() == m_process.coreProcessIdentifier());
    m_process.websiteDataStore().webLockRegistry().releaseLock(WTFMove(clientOrigin), lockIdentifier, clientID, WTFMove(name));
}

void WebLockRegistryProxy::abortLockRequest(WebCore::ClientOrigin&& clientOrigin, WebCore::WebLockIdentifier lockIdentifier, WebCore::ScriptExecutionContextIdentifier clientID, String&& name, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK(lockIdentifier.processIdentifier() == m_process.coreProcessIdentifier());
    MESSAGE_CHECK(clientID.processIdentifier() == m_process.coreProcessIdentifier());
    m_process.websiteDataStore().webLockRegistry().abortLockRequest(WTFMove(clientOrigin), lockIdentifier, clientID, WTFMove(name), WTFMove(completionHandler));
}

void WebLockRegistryProxy::snapshot(WebCore::ClientOrigin&& clientOrigin, CompletionHandler<void(WebCore::WebLockManagerSnapshot&&)>&& completionHandler)
{
    m_process.websiteDataStore().webLockRegistry().snapshot(WTFMove(clientOrigin), WTFMove(completionHandler));
}

void WebLockRegistryProxy::clientIsGoingAway(WebCore::ClientOrigin&& clientOrigin, WebCore::ScriptExecutionContextIdentifier clientID)
{
    MESSAGE_CHECK(clientID.processIdentifier() == m_process.coreProcessIdentifier());
    m_process.websiteDataStore().webLockRegistry().clientIsGoingAway(WTFMove(clientOrigin), clientID);
}

void WebLockRegistryProxy::processDidExit()
{
    if (m_hasEverRequestedLocks)
        m_process.websiteDataStore().webLockRegistry().clientsAreGoingAway(m_process.coreProcessIdentifier());
}

#undef MESSAGE_CHECK

} // namespace WebKit
