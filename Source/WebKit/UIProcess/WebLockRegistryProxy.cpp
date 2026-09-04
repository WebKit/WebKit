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

#include "config.h"
#include "WebLockRegistryProxy.h"

#include "Connection.h"
#include "FirstPartyAuthority.h"
#include "RemoteWebLockRegistryMessages.h"
#include "WebLockRegistryProxyMessages.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/WebLock.h>
#include <WebCore/WebLockIdentifier.h>
#include <WebCore/WebLockManagerSnapshot.h>
#include <WebCore/WebLockRegistry.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_process->connection())
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, m_process->connection(), completion)

#define EXTRACT_WITH_MESSAGE_CHECK(name, untrusted, ...) \
    auto name##Validated = WTF::move(untrusted).validate(__VA_ARGS__); \
    MESSAGE_CHECK(IPC::valueMayBeLegitimate(name##Validated)); \
    if (!name##Validated) \
        return; \
    auto name = WTF::move(*name##Validated)

#define EXTRACT_WITH_MESSAGE_CHECK_COMPLETION(name, untrusted, completion, ...) \
    auto name##Validated = WTF::move(untrusted).validate(__VA_ARGS__); \
    MESSAGE_CHECK_COMPLETION(IPC::valueMayBeLegitimate(name##Validated), completion); \
    if (!name##Validated) { \
        { completion; } \
        return; \
    } \
    auto name = WTF::move(*name##Validated)

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebLockRegistryProxy);

WebLockRegistryProxy::WebLockRegistryProxy(WebProcessProxy& process)
    : m_process(process)
{
    process.addMessageReceiver(Messages::WebLockRegistryProxy::messageReceiverName(), *this);
}

WebLockRegistryProxy::~WebLockRegistryProxy()
{
    m_process->removeMessageReceiver(Messages::WebLockRegistryProxy::messageReceiverName());
}

void WebLockRegistryProxy::requestLock(IPC::Untrusted<WebCore::ClientOrigin>&& untrustedClientOrigin, WebCore::WebLockIdentifier lockIdentifier, WebCore::ScriptExecutionContextIdentifier clientID, String&& name, WebCore::WebLockMode lockMode, bool steal, bool ifAvailable)
{
    MESSAGE_CHECK(lockIdentifier.processIdentifier() == m_process->coreProcessIdentifier());
    MESSAGE_CHECK(clientID.processIdentifier() == m_process->coreProcessIdentifier());
    MESSAGE_CHECK(name.length() <= WebCore::WebLock::maxNameLength);
    EXTRACT_WITH_MESSAGE_CHECK(clientOrigin, untrustedClientOrigin, CommittedClientOriginAuthority { m_process.get() });
    m_hasEverRequestedLocks = true;

    RefPtr dataStore = m_process->websiteDataStore();
    if (!dataStore) {
        m_process->send(Messages::RemoteWebLockRegistry::DidCompleteLockRequest(lockIdentifier, clientID, false), 0);
        return;
    }

    dataStore->webLockRegistry().requestLock(m_process->sessionID(), WTF::move(clientOrigin), lockIdentifier, clientID, WTF::move(name), lockMode, steal, ifAvailable, [weakThis = WeakPtr { *this }, lockIdentifier, clientID](bool success) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_process->send(Messages::RemoteWebLockRegistry::DidCompleteLockRequest(lockIdentifier, clientID, success), 0);
    }, [weakThis = WeakPtr { *this }, lockIdentifier, clientID] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_process->send(Messages::RemoteWebLockRegistry::DidStealLock(lockIdentifier, clientID), 0);
    });
}

void WebLockRegistryProxy::releaseLock(IPC::Untrusted<WebCore::ClientOrigin>&& untrustedClientOrigin, WebCore::WebLockIdentifier lockIdentifier, WebCore::ScriptExecutionContextIdentifier clientID, String&& name)
{
    MESSAGE_CHECK(lockIdentifier.processIdentifier() == m_process->coreProcessIdentifier());
    MESSAGE_CHECK(clientID.processIdentifier() == m_process->coreProcessIdentifier());
    EXTRACT_WITH_MESSAGE_CHECK(clientOrigin, untrustedClientOrigin, CommittedClientOriginAuthority { m_process.get() });
    Ref process = m_process.get();
    RefPtr dataStore = process->websiteDataStore();
    if (!dataStore)
        return;

    dataStore->webLockRegistry().releaseLock(process->sessionID(), WTF::move(clientOrigin), lockIdentifier, clientID, WTF::move(name));
}

void WebLockRegistryProxy::abortLockRequest(IPC::Untrusted<WebCore::ClientOrigin>&& untrustedClientOrigin, WebCore::WebLockIdentifier lockIdentifier, WebCore::ScriptExecutionContextIdentifier clientID, String&& name, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(lockIdentifier.processIdentifier() == m_process->coreProcessIdentifier(), completionHandler(false));
    MESSAGE_CHECK_COMPLETION(clientID.processIdentifier() == m_process->coreProcessIdentifier(), completionHandler(false));
    EXTRACT_WITH_MESSAGE_CHECK_COMPLETION(clientOrigin, untrustedClientOrigin, completionHandler(false), CommittedClientOriginAuthority { m_process.get() });
    RefPtr dataStore = m_process->websiteDataStore();
    if (!dataStore) {
        completionHandler(false);
        return;
    }

    dataStore->webLockRegistry().abortLockRequest(m_process->sessionID(), WTF::move(clientOrigin), lockIdentifier, clientID, WTF::move(name), WTF::move(completionHandler));
}

void WebLockRegistryProxy::snapshot(IPC::Untrusted<WebCore::ClientOrigin>&& untrustedClientOrigin, CompletionHandler<void(WebCore::WebLockManagerSnapshot&&)>&& completionHandler)
{
    EXTRACT_WITH_MESSAGE_CHECK_COMPLETION(clientOrigin, untrustedClientOrigin, completionHandler(WebCore::WebLockManagerSnapshot { }), CommittedClientOriginAuthority { m_process.get() });

    RefPtr dataStore = m_process->websiteDataStore();
    if (!dataStore) {
        completionHandler(WebCore::WebLockManagerSnapshot { });
        return;
    }

    dataStore->webLockRegistry().snapshot(m_process->sessionID(), WTF::move(clientOrigin), WTF::move(completionHandler));
}

void WebLockRegistryProxy::clientIsGoingAway(IPC::Untrusted<WebCore::ClientOrigin>&& untrustedClientOrigin, WebCore::ScriptExecutionContextIdentifier clientID)
{
    MESSAGE_CHECK(clientID.processIdentifier() == m_process->coreProcessIdentifier());
    EXTRACT_WITH_MESSAGE_CHECK(clientOrigin, untrustedClientOrigin, CommittedClientOriginAuthority { m_process.get() });
    if (RefPtr dataStore = WebsiteDataStore::existingDataStoreForSessionID(m_process->sessionID()))
        dataStore->webLockRegistry().clientIsGoingAway(m_process->sessionID(), WTF::move(clientOrigin), clientID);
}

void WebLockRegistryProxy::processDidExit()
{
    if (!m_hasEverRequestedLocks)
        return;

    if (RefPtr dataStore = WebsiteDataStore::existingDataStoreForSessionID(m_process->sessionID()))
        dataStore->webLockRegistry().clientsAreGoingAway(m_process->coreProcessIdentifier());
}

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_COMPLETION
#undef EXTRACT_WITH_MESSAGE_CHECK
#undef EXTRACT_WITH_MESSAGE_CHECK_COMPLETION

} // namespace WebKit
