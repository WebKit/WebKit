/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "StorageManager.h"

#include "LocalStorageDatabase.h"
#include "LocalStorageDatabaseTracker.h"
#include "LocalStorageNamespace.h"
#include "SessionStorageNamespace.h"
#include "StorageArea.h"
#include "StorageAreaMapMessages.h"
#include "TransientLocalStorageNamespace.h"
#include "WebProcessProxy.h"
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/StorageMap.h>
#include <WebCore/TextEncoding.h>
#include <memory>
#include <wtf/CrossThreadCopier.h>
#include <wtf/WorkQueue.h>

namespace WebKit {
using namespace WebCore;

// Suggested by https://www.w3.org/TR/webstorage/#disk-space
const unsigned StorageManager::localStorageDatabaseQuotaInBytes = 5 * 1024 * 1024;

StorageManager::StorageManager(String&& localStorageDirectory)
{
    ASSERT(!RunLoop::isMain());

    if (!localStorageDirectory.isNull())
        m_localStorageDatabaseTracker = LocalStorageDatabaseTracker::create(WTFMove(localStorageDirectory));
}

StorageManager::~StorageManager()
{
    ASSERT(!RunLoop::isMain());
}

void StorageManager::createSessionStorageNamespace(StorageNamespaceIdentifier storageNamespaceID, unsigned quotaInBytes)
{
    ASSERT(!RunLoop::isMain());

    m_sessionStorageNamespaces.ensure(storageNamespaceID, [quotaInBytes] {
        return makeUnique<SessionStorageNamespace>(quotaInBytes);
    });
}

void StorageManager::destroySessionStorageNamespace(StorageNamespaceIdentifier storageNamespaceID)
{
    ASSERT(!RunLoop::isMain());

    ASSERT(m_sessionStorageNamespaces.contains(storageNamespaceID));
    m_sessionStorageNamespaces.remove(storageNamespaceID);
}

void StorageManager::cloneSessionStorageNamespace(StorageNamespaceIdentifier storageNamespaceID, StorageNamespaceIdentifier newStorageNamespaceID)
{
    ASSERT(!RunLoop::isMain());

    SessionStorageNamespace* sessionStorageNamespace = m_sessionStorageNamespaces.get(storageNamespaceID);
    if (!sessionStorageNamespace)
        return;

    SessionStorageNamespace* newSessionStorageNamespace = getOrCreateSessionStorageNamespace(newStorageNamespaceID);
    ASSERT(newSessionStorageNamespace);

    sessionStorageNamespace->cloneTo(*newSessionStorageNamespace);
}

HashSet<SecurityOriginData> StorageManager::getSessionStorageOriginsCrossThreadCopy() const
{
    ASSERT(!RunLoop::isMain());

    HashSet<SecurityOriginData> origins;
    for (const auto& sessionStorageNamespace : m_sessionStorageNamespaces.values()) {
        for (auto& origin : sessionStorageNamespace->origins())
            origins.add(crossThreadCopy(origin));
    }

    return origins;
}

void StorageManager::deleteSessionStorageOrigins()
{
    ASSERT(!RunLoop::isMain());

    for (auto& sessionStorageNamespace : m_sessionStorageNamespaces.values())
        sessionStorageNamespace->clearAllStorageAreas();
}

void StorageManager::deleteSessionStorageEntriesForOrigins(const Vector<SecurityOriginData>& origins)
{
    ASSERT(!RunLoop::isMain());

    for (auto& origin : origins) {
        for (auto& sessionStorageNamespace : m_sessionStorageNamespaces.values())
            sessionStorageNamespace->clearStorageAreasMatchingOrigin(origin);
    }
}

HashSet<SecurityOriginData> StorageManager::getLocalStorageOriginsCrossThreadCopy() const
{
    ASSERT(!RunLoop::isMain());

    HashSet<SecurityOriginData> origins;
    if (m_localStorageDatabaseTracker) {
    for (auto& origin : m_localStorageDatabaseTracker->origins())
        origins.add(origin.isolatedCopy());
    } else {
        for (const auto& localStorageNameSpace : m_localStorageNamespaces.values()) {
            for (auto& origin : localStorageNameSpace->ephemeralOrigins())
                origins.add(origin.isolatedCopy());
        }
    }

    for (auto& transientLocalStorageNamespace : m_transientLocalStorageNamespaces.values()) {
        for (auto& origin : transientLocalStorageNamespace->origins())
            origins.add(origin.isolatedCopy());
    }

    return origins;
}

Vector<LocalStorageDatabaseTracker::OriginDetails> StorageManager::getLocalStorageOriginDetailsCrossThreadCopy() const
{
    ASSERT(!RunLoop::isMain());

    if (m_localStorageDatabaseTracker)
        return m_localStorageDatabaseTracker->originDetailsCrossThreadCopy();
    return { };
}

void StorageManager::deleteLocalStorageOriginsModifiedSince(WallTime time)
{
    ASSERT(!RunLoop::isMain());

    if (m_localStorageDatabaseTracker) {
        auto originsToDelete = m_localStorageDatabaseTracker->databasesModifiedSince(time);

        for (auto& transientLocalStorageNamespace : m_transientLocalStorageNamespaces.values())
            transientLocalStorageNamespace->clearAllStorageAreas();

        for (const auto& origin : originsToDelete) {
            for (auto& localStorageNamespace : m_localStorageNamespaces.values())
                localStorageNamespace->clearStorageAreasMatchingOrigin(origin);
            m_localStorageDatabaseTracker->deleteDatabaseWithOrigin(origin);
        }
    } else {
        for (auto& localStorageNamespace : m_localStorageNamespaces.values())
            localStorageNamespace->clearAllStorageAreas();
    }
}

void StorageManager::deleteLocalStorageEntriesForOrigins(const Vector<SecurityOriginData>& origins)
{
    ASSERT(!RunLoop::isMain());

    for (auto& origin : origins) {
        for (auto& localStorageNamespace : m_localStorageNamespaces.values())
            localStorageNamespace->clearStorageAreasMatchingOrigin(origin);

        for (auto& transientLocalStorageNamespace : m_transientLocalStorageNamespaces.values())
            transientLocalStorageNamespace->clearStorageAreasMatchingOrigin(origin);

        if (m_localStorageDatabaseTracker)
            m_localStorageDatabaseTracker->deleteDatabaseWithOrigin(origin);
    }
}

StorageArea* StorageManager::createLocalStorageArea(StorageNamespaceIdentifier storageNamespaceID, SecurityOriginData&& origin, Ref<WorkQueue>&& workQueue)
{
    ASSERT(!RunLoop::isMain());

    if (auto* localStorageNamespace = getOrCreateLocalStorageNamespace(storageNamespaceID))
        return &localStorageNamespace->getOrCreateStorageArea(WTFMove(origin), m_localStorageDatabaseTracker ? LocalStorageNamespace::IsEphemeral::No : LocalStorageNamespace::IsEphemeral::Yes, WTFMove(workQueue));

    return nullptr;
}

StorageArea* StorageManager::createTransientLocalStorageArea(StorageNamespaceIdentifier storageNamespaceID, SecurityOriginData&& topLevelOrigin, SecurityOriginData&& origin, Ref<WorkQueue>&& workQueue)
{
    ASSERT(!RunLoop::isMain());
    ASSERT((HashMap<StorageNamespaceIdentifier, RefPtr<TransientLocalStorageNamespace>>::isValidKey(storageNamespaceID)));

    if (auto* transientLocalStorageNamespace = getOrCreateTransientLocalStorageNamespace(storageNamespaceID, WTFMove(topLevelOrigin)))
        return &transientLocalStorageNamespace->getOrCreateStorageArea(WTFMove(origin), WTFMove(workQueue));
    
    return nullptr;
}

StorageArea* StorageManager::createSessionStorageArea(StorageNamespaceIdentifier storageNamespaceID, SecurityOriginData&& origin, Ref<WorkQueue>&& workQueue)
{
    ASSERT(!RunLoop::isMain());
    ASSERT((HashMap<StorageNamespaceIdentifier, RefPtr<SessionStorageNamespace>>::isValidKey(storageNamespaceID)));

    if (auto* sessionStorageNamespace = getOrCreateSessionStorageNamespace(storageNamespaceID))
        return &sessionStorageNamespace->getOrCreateStorageArea(WTFMove(origin), WTFMove(workQueue));
    
    return nullptr;
}

LocalStorageNamespace* StorageManager::getOrCreateLocalStorageNamespace(StorageNamespaceIdentifier storageNamespaceID)
{
    ASSERT(!RunLoop::isMain());

    if (!m_localStorageNamespaces.isValidKey(storageNamespaceID))
        return nullptr;

    return m_localStorageNamespaces.ensure(storageNamespaceID, [this, storageNamespaceID]() {
        return makeUnique<LocalStorageNamespace>(*this, storageNamespaceID);
    }).iterator->value.get();
}

TransientLocalStorageNamespace* StorageManager::getOrCreateTransientLocalStorageNamespace(StorageNamespaceIdentifier storageNamespaceID, SecurityOriginData&& topLevelOrigin)
{
    ASSERT(!RunLoop::isMain());

    if (!m_transientLocalStorageNamespaces.isValidKey({ storageNamespaceID, topLevelOrigin }))
        return nullptr;

    return m_transientLocalStorageNamespaces.ensure({ storageNamespaceID, WTFMove(topLevelOrigin) }, [] {
        return makeUnique<TransientLocalStorageNamespace>();
    }).iterator->value.get();
}

SessionStorageNamespace* StorageManager::getOrCreateSessionStorageNamespace(StorageNamespaceIdentifier storageNamespaceID)
{
    ASSERT(!RunLoop::isMain());

    if (!m_sessionStorageNamespaces.isValidKey(storageNamespaceID))
        return nullptr;

    return m_sessionStorageNamespaces.ensure(storageNamespaceID, [] {
        // We currently have no limit on session storage.
        return makeUnique<SessionStorageNamespace>(std::numeric_limits<unsigned>::max());
    }).iterator->value.get();
}

void StorageManager::clearStorageNamespaces()
{
    ASSERT(!RunLoop::isMain());

    m_localStorageNamespaces.clear();
    m_transientLocalStorageNamespaces.clear();
    m_sessionStorageNamespaces.clear();
}

Vector<StorageAreaIdentifier> StorageManager::allStorageAreaIdentifiers() const
{
    ASSERT(!RunLoop::isMain());

    Vector<StorageAreaIdentifier> identifiers;
    for (const auto& localStorageNamespace : m_localStorageNamespaces.values()) {
        for (auto key : localStorageNamespace->storageAreaIdentifiers())
            identifiers.append(key);
    }

    for (const auto& trasientLocalStorageNamespace : m_transientLocalStorageNamespaces.values()) {
        for (auto key : trasientLocalStorageNamespace->storageAreaIdentifiers())
            identifiers.append(key);
    }

    for (const auto& sessionStorageNamespace : m_sessionStorageNamespaces.values()) {
        for (auto key : sessionStorageNamespace->storageAreaIdentifiers())
            identifiers.append(key);
    }

    return identifiers;
}

} // namespace WebKit
