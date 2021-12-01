/*
 * Copyright (C) 2021, Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebLockRegistry.h"

#include "Exception.h"
#include "WebLockManagerSnapshot.h"
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

struct WebLockRegistry::LockInfo {
    Ref<WebLockRegistry> registry;
    WebLockIdentifier lockIdentifier;
    ScriptExecutionContextIdentifier clientID;
    WebLockMode mode;
    Function<void(Exception&&)> releaseHandler;
};

struct WebLockRegistry::LockRequest : LockInfo {
    String name;
    Function<void(bool)> grantedHandler;
};

static HashMap<ClientOrigin, WebLockRegistry*>& registryMap()
{
    ASSERT(isMainRunLoop());
    static NeverDestroyed<HashMap<ClientOrigin, WebLockRegistry*>> registryMap;
    return registryMap;
}

Ref<WebLockRegistry> WebLockRegistry::registryForOrigin(const ClientOrigin& origin)
{
    if (auto existingRegistry = registryMap().get(origin))
        return *existingRegistry;

    return adoptRef(*new WebLockRegistry(origin));
}

WebLockRegistry::WebLockRegistry(const ClientOrigin& origin)
    : m_origin(origin)
{
    registryMap().add(m_origin, this);
}

WebLockRegistry::~WebLockRegistry()
{
    auto* registry = registryMap().take(m_origin);
    ASSERT_UNUSED(registry, registry == this);
}

// https://wicg.github.io/web-locks/#request-a-lock
void WebLockRegistry::requestLock(WebLockIdentifier lockIdentifier, ScriptExecutionContextIdentifier clientID, const String& name, WebLockMode mode, bool steal, bool ifAvailable, Function<void(bool)>&& grantedHandler, Function<void(Exception&&)>&& releaseHandler)
{
    LockRequest request { { *this, lockIdentifier, clientID, mode, WTFMove(releaseHandler) }, name, WTFMove(grantedHandler) };

    if (steal) {
        auto it = m_heldLocks.find(name);
        if (it != m_heldLocks.end()) {
            for (auto& lockInfo : it->value)
                lockInfo.releaseHandler(Exception { AbortError, "Lock was stolen by another request" });
            m_heldLocks.remove(it);
        }
    } else if (ifAvailable && !isGrantable(request)) {
        request.grantedHandler(false);
        return;
    }
    auto& queue = m_lockRequestQueueMap.ensure(name, [] { return Deque<LockRequest> { }; }).iterator->value;
    if (steal)
        queue.prepend(WTFMove(request));
    else
        queue.append(WTFMove(request));
    processLockRequestQueue(name, queue);
}

// https://wicg.github.io/web-locks/#algorithm-release-lock
void WebLockRegistry::releaseLock(WebLockIdentifier lockIdentifier, const String& name)
{
    auto it = m_heldLocks.find(name);
    if (it == m_heldLocks.end())
        return;

    auto& locksWithName = it->value;
    locksWithName.removeFirstMatching([lockIdentifier](auto& lockInfo) { return lockIdentifier == lockInfo.lockIdentifier; });
    if (locksWithName.isEmpty())
        m_heldLocks.remove(it);

    auto queueIterator = m_lockRequestQueueMap.find(name);
    if (queueIterator != m_lockRequestQueueMap.end())
        processLockRequestQueue(name, queueIterator->value);
}

// https://wicg.github.io/web-locks/#abort-the-request
void WebLockRegistry::abortLockRequest(WebLockIdentifier lockIdentifier, const String& name, CompletionHandler<void(bool)>&& completionHandler)
{
    auto queueIterator = m_lockRequestQueueMap.find(name);
    if (queueIterator == m_lockRequestQueueMap.end())
        return completionHandler(false);

    auto& queue = queueIterator->value;
    auto requestIterator = queue.findIf([lockIdentifier](auto& request) { return request.lockIdentifier == lockIdentifier; });
    if (requestIterator == queue.end())
        return completionHandler(false);

    queue.remove(requestIterator);
    if (queue.isEmpty()) {
        m_lockRequestQueueMap.remove(queueIterator);
        return completionHandler(true);
    }

    processLockRequestQueue(name, queue);
    completionHandler(true);
}

// https://wicg.github.io/web-locks/#grantable
bool WebLockRegistry::isGrantable(const LockRequest& request) const
{
    auto queueIterator = m_lockRequestQueueMap.find(request.name);
    if (queueIterator != m_lockRequestQueueMap.end() && &queueIterator->value.first() != &request)
        return false;

    switch (request.mode) {
    case WebLockMode::Exclusive:
        return !m_heldLocks.contains(request.name);
    case WebLockMode::Shared:
        break;
    }
    auto it = m_heldLocks.find(request.name);
    return it == m_heldLocks.end() || it->value.first().mode != WebLockMode::Exclusive;
}

// https://wicg.github.io/web-locks/#process-the-lock-request-queue
void WebLockRegistry::processLockRequestQueue(const String& name, Deque<LockRequest>& queue)
{
    while (!queue.isEmpty()) {
        if (!isGrantable(queue.first()))
            return;
        auto request = queue.takeFirst();
        auto& locksForName = m_heldLocks.ensure(request.name, [] { return Vector<LockInfo> { }; }).iterator->value;
        auto grantedHandler = WTFMove(request.grantedHandler);
        locksForName.append(WTFMove(request));
        grantedHandler(true);
    }
    auto removedQueue = m_lockRequestQueueMap.take(name);
    ASSERT_UNUSED(removedQueue, removedQueue.isEmpty());
}

// https://wicg.github.io/web-locks/#snapshot-the-lock-state
void WebLockRegistry::snapshot(CompletionHandler<void(WebLockManager::Snapshot&&)>&& completionHandler)
{
    WebLockManager::Snapshot snapshot;
    for (auto& pair : m_lockRequestQueueMap) {
        for (auto& request : pair.value)
            snapshot.pending.append({ pair.key, request.mode, request.clientID.toString() });
    }
    for (auto& pair : m_heldLocks) {
        for (auto& lockInfo : pair.value)
            snapshot.held.append({ pair.key, lockInfo.mode, lockInfo.clientID.toString() });
    }

    completionHandler(WTFMove(snapshot));
}

// https://wicg.github.io/web-locks/#agent-integration
void WebLockRegistry::clientIsGoingAway(ScriptExecutionContextIdentifier clientID)
{
    HashSet<String> namesOfQueuesToProcess;

    Vector<String> namesWithoutRequests;
    for (auto& pair : m_lockRequestQueueMap) {
        if (!pair.value.removeAllMatching([clientID](auto& request) { return request.clientID == clientID; }))
            continue;
        if (pair.value.isEmpty())
            namesWithoutRequests.append(pair.key);
        else
            namesOfQueuesToProcess.add(pair.key);
    }
    for (auto& name : namesWithoutRequests)
        m_lockRequestQueueMap.remove(name);

    Vector<String> namesWithoutLocks;
    for (auto& pair : m_heldLocks) {
        if (!pair.value.removeAllMatching([clientID](auto& lockInfo) { return lockInfo.clientID == clientID; }))
            continue;
        if (pair.value.isEmpty())
            namesWithoutLocks.append(pair.key);
        namesOfQueuesToProcess.add(pair.key);
    }
    for (auto& name : namesWithoutLocks)
        m_heldLocks.remove(name);

    for (auto& name : namesOfQueuesToProcess) {
        auto queueIterator = m_lockRequestQueueMap.find(name);
        if (queueIterator != m_lockRequestQueueMap.end())
            processLockRequestQueue(name, queueIterator->value);
    }
}

} // namespace WebCore
