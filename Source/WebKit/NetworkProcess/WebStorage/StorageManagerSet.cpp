/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StorageManagerSet.h"

#include "StorageArea.h"
#include "StorageAreaMapMessages.h"
#include "StorageManagerSetMessages.h"
#include <pal/text/TextEncoding.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebKit {

Ref<StorageManagerSet> StorageManagerSet::create()
{
    return adoptRef(*new StorageManagerSet);
}

StorageManagerSet::StorageManagerSet()
    : m_queue(SuspendableWorkQueue::create("com.apple.WebKit.WebStorage"))
{
    ASSERT(RunLoop::isMain());

    // Make sure the encoding is initialized before we start dispatching things to the queue.
    PAL::UTF8Encoding();
}

StorageManagerSet::~StorageManagerSet()
{
    ASSERT(RunLoop::isMain());

    waitUntilTasksFinished();
}

void StorageManagerSet::add(PAL::SessionID sessionID, const String& localStorageDirectory, SandboxExtension::Handle& localStorageDirectoryHandle)
{
    ASSERT(RunLoop::isMain());

    if (m_storageManagerPaths.add(sessionID, localStorageDirectory).isNewEntry) {
        if (!sessionID.isEphemeral())
            SandboxExtension::consumePermanently(localStorageDirectoryHandle);

        m_queue->dispatch([this, protectedThis = Ref { *this }, sessionID, localStorageDirectory = localStorageDirectory.isolatedCopy()]() mutable {
            m_storageManagers.ensure(sessionID, [&]() mutable {
                return makeUnique<StorageManager>(WTFMove(localStorageDirectory));
            });
        });
    }
}

void StorageManagerSet::remove(PAL::SessionID sessionID)
{
    ASSERT(RunLoop::isMain());

    if (m_storageManagerPaths.remove(sessionID)) {
        m_queue->dispatch([this, protectedThis = Ref { *this }, sessionID]() {
            if (auto storageManager = m_storageManagers.get(sessionID)) {
                for (auto storageAreaID : storageManager->allStorageAreaIdentifiers())
                    m_storageAreas.remove(storageAreaID);

                m_storageManagers.remove(sessionID);
            }
        });
    }
}

bool StorageManagerSet::contains(PAL::SessionID sessionID)
{
    ASSERT(RunLoop::isMain());

    return m_storageManagerPaths.contains(sessionID);
}

void StorageManagerSet::addConnection(IPC::Connection& connection) 
{
    ASSERT(RunLoop::isMain());

    auto connectionID = connection.uniqueID();
    ASSERT(!m_connections.contains(connectionID));
    if (m_connections.add(connectionID).isNewEntry)
        connection.addWorkQueueMessageReceiver(Messages::StorageManagerSet::messageReceiverName(), m_queue.get(), this);
}

void StorageManagerSet::removeConnection(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());

    auto connectionID = connection.uniqueID();
    ASSERT(m_connections.contains(connectionID));
    m_connections.remove(connectionID);
    connection.removeWorkQueueMessageReceiver(Messages::StorageManagerSet::messageReceiverName());

    m_queue->dispatch([this, protectedThis = Ref { *this }, connectionID]() {
        Vector<StorageAreaIdentifier> identifiersToRemove;
        for (auto& [identifier, storageArea] : m_storageAreas) {
            if (storageArea)
                storageArea->removeListener(connectionID);

            if (!storageArea)
                identifiersToRemove.append(identifier);
        }

        for (auto identifier : identifiersToRemove)
            m_storageAreas.remove(identifier);
    });
}

void StorageManagerSet::handleLowMemoryWarning()
{
    ASSERT(RunLoop::isMain());
    m_queue->dispatch([this, protectedThis = Ref { *this }] {
        for (auto& storageArea : m_storageAreas.values())
            storageArea->handleLowMemoryWarning();
    });
}

void StorageManagerSet::waitUntilTasksFinished()
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatchSync([this] {
        for (auto& storageManager : m_storageManagers.values())
            storageManager->clearStorageNamespaces();

        m_storageManagers.clear();
        m_storageAreas.clear();
    });
}

void StorageManagerSet::waitUntilSyncingLocalStorageFinished()
{
    ASSERT(RunLoop::isMain());

    BinarySemaphore semaphore;
    m_queue->dispatch([this, &semaphore] {
        flushLocalStorage();
        semaphore.signal();
    });
    semaphore.wait();
}

void StorageManagerSet::flushLocalStorage()
{
    ASSERT(!RunLoop::isMain());
    for (const auto& storageArea : m_storageAreas.values()) {
        ASSERT(storageArea);
        if (storageArea)
            storageArea->syncToDatabase();
    }
}

void StorageManagerSet::suspend(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->suspend([protectedThis = Ref { *this }] {
        protectedThis->flushLocalStorage();
    }, WTFMove(completionHandler));
}

void StorageManagerSet::resume()
{
    ASSERT(RunLoop::isMain());

    m_queue->resume();
}

void StorageManagerSet::getSessionStorageOrigins(PAL::SessionID sessionID, GetOriginsCallback&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, sessionID, completionHandler = WTFMove(completionHandler)]() mutable {
        auto* storageManager = m_storageManagers.get(sessionID);
        ASSERT(storageManager);

        auto origins = storageManager->getSessionStorageOriginsCrossThreadCopy();
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), origins = WTFMove(origins)]() mutable {
            completionHandler(WTFMove(origins));
        });
    });
}

void StorageManagerSet::deleteSessionStorage(PAL::SessionID sessionID, DeleteCallback&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, sessionID, completionHandler = WTFMove(completionHandler)]() mutable {
        auto* storageManager = m_storageManagers.get(sessionID);
        ASSERT(storageManager);

        storageManager->deleteSessionStorageOrigins();
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void StorageManagerSet::deleteSessionStorageForOrigins(PAL::SessionID sessionID, const Vector<WebCore::SecurityOriginData>& originDatas, DeleteCallback&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, sessionID, copiedOriginDatas = crossThreadCopy(originDatas), completionHandler = WTFMove(completionHandler)]() mutable {
        auto* storageManager = m_storageManagers.get(sessionID);
        ASSERT(storageManager);

        storageManager->deleteSessionStorageEntriesForOrigins(copiedOriginDatas);
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void StorageManagerSet::getLocalStorageOrigins(PAL::SessionID sessionID, GetOriginsCallback&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, sessionID, completionHandler = WTFMove(completionHandler)]() mutable {
        auto* storageManager = m_storageManagers.get(sessionID);
        ASSERT(storageManager);

        auto origins = storageManager->getLocalStorageOriginsCrossThreadCopy();
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), origins = WTFMove(origins)]() mutable {
            completionHandler(WTFMove(origins));
        });
    });
}

void StorageManagerSet::deleteLocalStorageModifiedSince(PAL::SessionID sessionID, WallTime time, DeleteCallback&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, sessionID, time, completionHandler = WTFMove(completionHandler)]() mutable {
        auto* storageManager = m_storageManagers.get(sessionID);
        ASSERT(storageManager);

        storageManager->deleteLocalStorageOriginsModifiedSince(time);
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void StorageManagerSet::deleteLocalStorageForOrigins(PAL::SessionID sessionID, const Vector<WebCore::SecurityOriginData>& originDatas, DeleteCallback&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, sessionID, copiedOriginDatas = crossThreadCopy(originDatas), completionHandler = WTFMove(completionHandler)]() mutable {
        auto* storageManager = m_storageManagers.get(sessionID);
        ASSERT(storageManager);

        storageManager->deleteLocalStorageEntriesForOrigins(copiedOriginDatas);
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void StorageManagerSet::getLocalStorageOriginDetails(PAL::SessionID sessionID, GetOriginDetailsCallback&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, sessionID, completionHandler = WTFMove(completionHandler)]() mutable {
        auto* storageManager = m_storageManagers.get(sessionID);
        ASSERT(storageManager);

        auto originDetails = storageManager->getLocalStorageOriginDetailsCrossThreadCopy();
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), originDetails = WTFMove(originDetails)]() mutable {
            completionHandler(WTFMove(originDetails));
        });
    });
}

void StorageManagerSet::renameOrigin(PAL::SessionID sessionID, const URL& oldName, const URL& newName, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, sessionID, oldName = oldName.isolatedCopy(), newName = newName.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        auto* storageManager = m_storageManagers.get(sessionID);
        ASSERT(storageManager);
        storageManager->renameOrigin(oldName, newName);
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void StorageManagerSet::connectToLocalStorageArea(IPC::Connection& connection, PAL::SessionID sessionID, StorageNamespaceIdentifier storageNamespaceID, SecurityOriginData&& originData, ConnectToStorageAreaCallback&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto* storageManager = m_storageManagers.get(sessionID);
    if (!storageManager) {
        completionHandler(std::nullopt);
        return;
    }

    auto* storageArea = storageManager->createLocalStorageArea(storageNamespaceID, WTFMove(originData), m_queue.copyRef());
    if (!storageArea) {
        completionHandler(std::nullopt);
        return;
    }

    auto storageAreaID = storageArea->identifier();
    auto iter = m_storageAreas.add(storageAreaID, storageArea).iterator;
    ASSERT_UNUSED(iter, storageArea == iter->value.get());

    completionHandler(storageAreaID);

    storageArea->addListener(connection.uniqueID());
}

void StorageManagerSet::connectToTransientLocalStorageArea(IPC::Connection& connection, PAL::SessionID sessionID, StorageNamespaceIdentifier storageNamespaceID, SecurityOriginData&& topLevelOriginData, SecurityOriginData&& originData, ConnectToStorageAreaCallback&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto* storageManager = m_storageManagers.get(sessionID);
    if (!storageManager) {
        completionHandler(std::nullopt);
        return;
    }

    auto* storageArea = storageManager->createTransientLocalStorageArea(storageNamespaceID, WTFMove(topLevelOriginData), WTFMove(originData), m_queue.copyRef());
    if (!storageArea) {
        completionHandler(std::nullopt);
        return;
    }

    auto storageAreaID = storageArea->identifier();
    auto iter = m_storageAreas.add(storageAreaID, storageArea).iterator;
    ASSERT_UNUSED(iter, storageArea == iter->value.get());

    completionHandler(storageAreaID);

    storageArea->addListener(connection.uniqueID());
}

void StorageManagerSet::connectToSessionStorageArea(IPC::Connection& connection, PAL::SessionID sessionID, StorageNamespaceIdentifier storageNamespaceID, SecurityOriginData&& originData, ConnectToStorageAreaCallback&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto* storageManager = m_storageManagers.get(sessionID);
    if (!storageManager) {
        completionHandler(std::nullopt);
        return;
    }

    auto* storageArea = storageManager->createSessionStorageArea(storageNamespaceID, WTFMove(originData), m_queue.copyRef());
    if (!storageArea) {
        completionHandler(std::nullopt);
        return;
    }

    auto storageAreaID = storageArea->identifier();
    auto iter = m_storageAreas.add(storageAreaID, storageArea).iterator;
    ASSERT_UNUSED(iter, storageArea == iter->value.get());

    completionHandler(storageAreaID);
    
    storageArea->addListener(connection.uniqueID());
}

void StorageManagerSet::disconnectFromStorageArea(IPC::Connection& connection, StorageAreaIdentifier storageAreaID)
{
    ASSERT(!RunLoop::isMain());

    const auto& storageArea = m_storageAreas.get(storageAreaID);
    ASSERT(storageArea);
    ASSERT(storageArea->hasListener(connection.uniqueID()));

    if (!storageArea)
        return;

    storageArea->removeListener(connection.uniqueID());
    if (!storageArea)
        m_storageAreas.remove(storageAreaID);
}

void StorageManagerSet::getValues(IPC::Connection& connection, StorageAreaIdentifier storageAreaID, GetValuesCallback&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    const auto& storageArea = m_storageAreas.get(storageAreaID);
    ASSERT(!storageArea || storageArea->hasListener(connection.uniqueID()));

    completionHandler(storageArea ? storageArea->items() : HashMap<String, String>());
}

void StorageManagerSet::setItem(IPC::Connection& connection, StorageAreaIdentifier storageAreaID, StorageAreaImplIdentifier storageAreaImplID, const String& key, const String& value, const String& urlString, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    const auto& storageArea = m_storageAreas.get(storageAreaID);
    ASSERT(storageArea);

    bool quotaError = false;
    if (storageArea)
        storageArea->setItem(connection.uniqueID(), storageAreaImplID, key, value, urlString, quotaError);

    completionHandler(quotaError);
} 

void StorageManagerSet::removeItem(IPC::Connection& connection, StorageAreaIdentifier storageAreaID, StorageAreaImplIdentifier storageAreaImplID, const String& key, const String& urlString, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    const auto& storageArea = m_storageAreas.get(storageAreaID);
    ASSERT(storageArea);

    if (storageArea)
        storageArea->removeItem(connection.uniqueID(), storageAreaImplID, key, urlString);

    completionHandler();
}

void StorageManagerSet::clear(IPC::Connection& connection, StorageAreaIdentifier storageAreaID, StorageAreaImplIdentifier storageAreaImplID, const String& urlString, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    const auto& storageArea = m_storageAreas.get(storageAreaID);
    ASSERT(storageArea);

    if (storageArea)
        storageArea->clear(connection.uniqueID(), storageAreaImplID, urlString);

    completionHandler();
}

void StorageManagerSet::cloneSessionStorageNamespace(IPC::Connection&, PAL::SessionID sessionID, StorageNamespaceIdentifier fromStorageNamespaceID, StorageNamespaceIdentifier toStorageNamespaceID)
{
    ASSERT(!RunLoop::isMain());

    if (auto* storageManager = m_storageManagers.get(sessionID))
        storageManager->cloneSessionStorageNamespace(fromStorageNamespaceID, toStorageNamespaceID);
}

} // namespace WebKit
