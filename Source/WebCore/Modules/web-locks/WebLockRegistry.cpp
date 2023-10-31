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
#include "ScriptExecutionContext.h"
#include "WebLockManager.h"
#include "WebLockManagerSnapshot.h"
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

static RefPtr<WebLockRegistry>& sharedRegistry()
{
    static MainThreadNeverDestroyed<RefPtr<WebLockRegistry>> registry;
    return registry;
}

WebLockRegistry& WebLockRegistry::shared()
{
    auto& registry = sharedRegistry();
    if (!registry)
        registry= LocalWebLockRegistry::create();
    return *registry;
}

void WebLockRegistry::setSharedRegistry(Ref<WebLockRegistry>&& registry)
{
    ASSERT(!sharedRegistry());
    sharedRegistry() = WTFMove(registry);
}

class LocalWebLockRegistry::PerOriginRegistry : public RefCounted<PerOriginRegistry>, public CanMakeWeakPtr<PerOriginRegistry> {
public:
    static Ref<PerOriginRegistry> create(LocalWebLockRegistry&, PAL::SessionID, const ClientOrigin&);
    ~PerOriginRegistry();

    struct LockInfo {
        Ref<PerOriginRegistry> registry;
        WebLockIdentifier lockIdentifier;
        ScriptExecutionContextIdentifier clientID;
        WebLockMode mode;
        Function<void()> lockStolenHandler;
    };

    void requestLock(WebLockIdentifier, ScriptExecutionContextIdentifier, const String& name, WebLockMode, bool steal, bool ifAvailable, Function<void(bool)>&& grantedHandler, Function<void()>&& lockStolenHandler);
    void releaseLock(WebLockIdentifier, const String& name);
    void abortLockRequest(WebLockIdentifier, const String& name, CompletionHandler<void(bool)>&&);
    void snapshot(CompletionHandler<void(WebLockManagerSnapshot&&)>&&);
    void clientsAreGoingAway(const Function<bool(const LockInfo&)>& matchClient);

private:
    PerOriginRegistry(LocalWebLockRegistry&, PAL::SessionID, const ClientOrigin&);

    struct LockRequest : LockInfo {
        String name;
        Function<void(bool)> grantedHandler;
    };

    void processLockRequestQueue(const String& name, Deque<LockRequest>&);
    bool isGrantable(const LockRequest&) const;

    WeakPtr<LocalWebLockRegistry> m_globalRegistry;
    PAL::SessionID m_sessionID;
    ClientOrigin m_clientOrigin;
    FastRobinHoodHashMap<String, Deque<LockRequest>> m_lockRequestQueueMap;
    FastRobinHoodHashMap<String, Vector<LockInfo>> m_heldLocks;
};

LocalWebLockRegistry::LocalWebLockRegistry() = default;

LocalWebLockRegistry::~LocalWebLockRegistry() = default;

auto LocalWebLockRegistry::ensureRegistryForOrigin(PAL::SessionID sessionID, const ClientOrigin& clientOrigin) -> Ref<PerOriginRegistry>
{
    if (auto existingRegistry = m_perOriginRegistries.get({ sessionID, clientOrigin }))
        return *existingRegistry;

    return PerOriginRegistry::create(*this, sessionID, clientOrigin);
}

auto LocalWebLockRegistry::existingRegistryForOrigin(PAL::SessionID sessionID, const ClientOrigin& clientOrigin) const -> RefPtr<PerOriginRegistry>
{
    return m_perOriginRegistries.get({ sessionID, clientOrigin }).get();
}

Ref<LocalWebLockRegistry::PerOriginRegistry> LocalWebLockRegistry::PerOriginRegistry::create(LocalWebLockRegistry& globalRegistry, PAL::SessionID sessionID, const ClientOrigin& clientOrigin)
{
    return adoptRef(*new PerOriginRegistry(globalRegistry, sessionID, clientOrigin));
}

LocalWebLockRegistry::PerOriginRegistry::PerOriginRegistry(LocalWebLockRegistry& globalRegistry, PAL::SessionID sessionID, const ClientOrigin& clientOrigin)
    : m_globalRegistry(globalRegistry)
    , m_sessionID(sessionID)
    , m_clientOrigin(clientOrigin)
{
    globalRegistry.m_perOriginRegistries.add({ sessionID, clientOrigin }, WeakPtr { * this });
}

LocalWebLockRegistry::PerOriginRegistry::~PerOriginRegistry()
{
    if (m_globalRegistry)
        m_globalRegistry->m_perOriginRegistries.remove({ m_sessionID, m_clientOrigin });
}

void LocalWebLockRegistry::requestLock(PAL::SessionID sessionID, const ClientOrigin& clientOrigin, WebLockIdentifier lockIdentifier, ScriptExecutionContextIdentifier clientID, const String& name, WebLockMode mode, bool steal, bool ifAvailable, Function<void(bool)>&& grantedHandler, Function<void()>&& lockStolenHandler)
{
    ensureRegistryForOrigin(sessionID, clientOrigin)->requestLock(lockIdentifier, clientID, name, mode, steal, ifAvailable, WTFMove(grantedHandler), WTFMove(lockStolenHandler));
}

// https://wicg.github.io/web-locks/#request-a-lock
void LocalWebLockRegistry::PerOriginRegistry::requestLock(WebLockIdentifier lockIdentifier, ScriptExecutionContextIdentifier clientID, const String& name, WebLockMode mode, bool steal, bool ifAvailable, Function<void(bool)>&& grantedHandler, Function<void()>&& lockStolenHandler)
{
    LockRequest request { { *this, lockIdentifier, clientID, mode, WTFMove(lockStolenHandler) }, name, WTFMove(grantedHandler) };

    if (steal) {
        auto it = m_heldLocks.find(name);
        if (it != m_heldLocks.end()) {
            for (auto& lockInfo : it->value)
                lockInfo.lockStolenHandler();
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

void LocalWebLockRegistry::releaseLock(PAL::SessionID sessionID, const ClientOrigin& clientOrigin, WebLockIdentifier lockIdentifier, ScriptExecutionContextIdentifier, const String& name)
{
    if (auto registry = existingRegistryForOrigin(sessionID, clientOrigin))
        registry->releaseLock(lockIdentifier, name);
}

// https://wicg.github.io/web-locks/#algorithm-release-lock
void LocalWebLockRegistry::PerOriginRegistry::releaseLock(WebLockIdentifier lockIdentifier, const String& name)
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

void LocalWebLockRegistry::abortLockRequest(PAL::SessionID sessionID, const ClientOrigin& clientOrigin, WebLockIdentifier lockIdentifier, ScriptExecutionContextIdentifier, const String& name, CompletionHandler<void(bool)>&& completionHandler)
{
    auto registry = existingRegistryForOrigin(sessionID, clientOrigin);
    if (!registry)
        return completionHandler(false);

    registry->abortLockRequest(lockIdentifier, name, WTFMove(completionHandler));
}

// https://wicg.github.io/web-locks/#abort-the-request
void LocalWebLockRegistry::PerOriginRegistry::abortLockRequest(WebLockIdentifier lockIdentifier, const String& name, CompletionHandler<void(bool)>&& completionHandler)
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
bool LocalWebLockRegistry::PerOriginRegistry::isGrantable(const LockRequest& request) const
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
void LocalWebLockRegistry::PerOriginRegistry::processLockRequestQueue(const String& name, Deque<LockRequest>& queue)
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

void LocalWebLockRegistry::snapshot(PAL::SessionID sessionID, const ClientOrigin& clientOrigin, CompletionHandler<void(WebLockManager::Snapshot&&)>&& completionHandler)
{
    auto registry = existingRegistryForOrigin(sessionID, clientOrigin);
    if (!registry)
        return completionHandler({ });

    registry->snapshot(WTFMove(completionHandler));
}

// https://wicg.github.io/web-locks/#snapshot-the-lock-state
void LocalWebLockRegistry::PerOriginRegistry::snapshot(CompletionHandler<void(WebLockManager::Snapshot&&)>&& completionHandler)
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

void LocalWebLockRegistry::clientIsGoingAway(PAL::SessionID sessionID, const ClientOrigin& clientOrigin, ScriptExecutionContextIdentifier clientID)
{
    if (auto registry = existingRegistryForOrigin(sessionID, clientOrigin))
        registry->clientsAreGoingAway([clientID](auto& lockInfo) { return lockInfo.clientID == clientID; });
}

// https://wicg.github.io/web-locks/#agent-integration
void LocalWebLockRegistry::PerOriginRegistry::clientsAreGoingAway(const Function<bool(const LockInfo&)>& matchClient)
{
    // FIXME: This is inefficient. We could optimize this by keeping track of which locks map to which clients.
    HashSet<String> namesOfQueuesToProcess;

    Vector<String> namesWithoutRequests;
    for (auto& pair : m_lockRequestQueueMap) {
        if (!pair.value.removeAllMatching(matchClient))
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
        if (!pair.value.removeAllMatching(matchClient))
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

void LocalWebLockRegistry::clientsAreGoingAway(ProcessIdentifier processIdentifier)
{
    Vector<std::pair<PAL::SessionID, ClientOrigin>> clientOrigins = copyToVector(m_perOriginRegistries.keys());
    for (auto& [sessionID, clientOrigin] : clientOrigins) {
        if (auto registry = existingRegistryForOrigin(sessionID, clientOrigin))
            registry->clientsAreGoingAway([processIdentifier](auto& lockInfo) { return lockInfo.clientID.processIdentifier() == processIdentifier; });
    }
}

} // namespace WebCore
