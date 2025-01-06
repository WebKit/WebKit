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
#include "LocalStorageManager.h"

#include "MemoryStorageArea.h"
#include "SQLiteStorageArea.h"
#include "StorageAreaRegistry.h"
#include <WebCore/SecurityOriginData.h>
#include <wtf/FileSystem.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

// Suggested by https://www.w3.org/TR/webstorage/#disk-space
constexpr unsigned localStorageQuotaInBytes = 5 * MB;
constexpr auto s_fileSuffix = ".localstorage"_s;
constexpr auto s_fileName = "localstorage.sqlite3"_s;

// This is intended to be used for existing files.
// We should not include origin in file name.
static std::optional<WebCore::SecurityOriginData> fileNameToOrigin(const String& fileName)
{
    if (!fileName.endsWith(StringView { s_fileSuffix }))
        return std::nullopt;

    auto suffixLength = s_fileSuffix.length();
    auto fileNameLength = fileName.length();
    if (fileNameLength <= suffixLength)
        return std::nullopt;

    auto originIdentifier = fileName.left(fileNameLength - suffixLength);
    return WebCore::SecurityOriginData::fromDatabaseIdentifier(originIdentifier);
}

static String originToFileName(const WebCore::ClientOrigin& origin)
{
    return makeString(origin.clientOrigin.databaseIdentifier(), ".localstorage"_s);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(LocalStorageManager);

Vector<WebCore::SecurityOriginData> LocalStorageManager::originsOfLocalStorageData(const String& path)
{
    Vector<WebCore::SecurityOriginData> origins;
    if (path.isEmpty())
        return origins;

    for (auto& fileName : FileSystem::listDirectory(path)) {
        if (auto origin = fileNameToOrigin(fileName))
            origins.append(*origin);
    }

    return origins;
}

String LocalStorageManager::localStorageFilePath(const String& directory, const WebCore::ClientOrigin& origin)
{
    if (directory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(directory, originToFileName(origin));
}

String LocalStorageManager::localStorageFilePath(const String& directory)
{
    if (directory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(directory, s_fileName);
}

LocalStorageManager::LocalStorageManager(const String& path, StorageAreaRegistry& registry)
    : m_path(path)
    , m_registry(registry)
{
}

bool LocalStorageManager::isActive() const
{
    return (m_localStorageArea && m_localStorageArea->hasListeners()) || (m_transientStorageArea && m_transientStorageArea->hasListeners());
}

bool LocalStorageManager::hasDataInMemory() const
{
    RefPtr memoryLocalStorage = dynamicDowncast<MemoryStorageArea>(m_localStorageArea);
    if (memoryLocalStorage && !memoryLocalStorage->isEmpty())
        return true;

    RefPtr transientStorage = m_transientStorageArea;
    return transientStorage && !transientStorage->isEmpty();
}

void LocalStorageManager::clearDataInMemory()
{
    if (RefPtr storage = dynamicDowncast<MemoryStorageArea>(m_localStorageArea))
        storage->clear();

    if (RefPtr transientStorage = m_transientStorageArea)
        transientStorage->clear();
}

void LocalStorageManager::clearDataOnDisk()
{
    if (RefPtr storage = dynamicDowncast<SQLiteStorageArea>(m_localStorageArea))
        storage->clear();
}

void LocalStorageManager::close()
{
    if (RefPtr storage = dynamicDowncast<SQLiteStorageArea>(m_localStorageArea))
        storage->close();
}

void LocalStorageManager::handleLowMemoryWarning()
{
    if (RefPtr storage = dynamicDowncast<SQLiteStorageArea>(m_localStorageArea))
        storage->handleLowMemoryWarning();
}

void LocalStorageManager::syncLocalStorage()
{
    if (RefPtr storage = dynamicDowncast<SQLiteStorageArea>(m_localStorageArea))
        storage->commitTransactionIfNecessary();
}

void LocalStorageManager::connectionClosed(IPC::Connection::UniqueID connection)
{
    connectionClosedForLocalStorageArea(connection);
    connectionClosedForTransientStorageArea(connection);
}

void LocalStorageManager::connectionClosedForLocalStorageArea(IPC::Connection::UniqueID connection)
{
    RefPtr storage = m_localStorageArea;
    if (!storage)
        return;

    storage->removeListener(connection);
    if (storage->hasListeners())
        return;

    if (RefPtr memoryStorage = dynamicDowncast<MemoryStorageArea>(*storage); memoryStorage && !memoryStorage->isEmpty())
        return;

    m_registry->unregisterStorageArea(storage->identifier());
    m_localStorageArea = nullptr;
}

void LocalStorageManager::connectionClosedForTransientStorageArea(IPC::Connection::UniqueID connection)
{
    RefPtr transientStorageArea = m_transientStorageArea;
    if (!transientStorageArea)
        return;

    transientStorageArea->removeListener(connection);
    if (transientStorageArea->hasListeners() || !transientStorageArea->isEmpty())
        return;

    m_registry->unregisterStorageArea(transientStorageArea->identifier());
    m_transientStorageArea = nullptr;
}

StorageAreaBase& LocalStorageManager::ensureLocalStorageArea(const WebCore::ClientOrigin& origin, Ref<WorkQueue>&& workQueue)
{
    if (!m_localStorageArea) {
        RefPtr<StorageAreaBase> storage;
        if (!m_path.isEmpty())
            storage = SQLiteStorageArea::create(localStorageQuotaInBytes, origin, m_path, WTFMove(workQueue));
        else
            storage = MemoryStorageArea::create(origin, StorageAreaBase::StorageType::Local);

        m_localStorageArea = storage;
        m_registry->registerStorageArea(storage->identifier(), *storage);
    }

    return *m_localStorageArea;
}

StorageAreaIdentifier LocalStorageManager::connectToLocalStorageArea(IPC::Connection::UniqueID connection, StorageAreaMapIdentifier sourceIdentifier, const WebCore::ClientOrigin& origin, Ref<WorkQueue>&& workQueue)
{
    Ref localStorageArea = ensureLocalStorageArea(origin, WTFMove(workQueue));
    ASSERT(m_path.isEmpty() || is<SQLiteStorageArea>(localStorageArea));
    localStorageArea->addListener(connection, sourceIdentifier);
    return localStorageArea->identifier();
}

MemoryStorageArea& LocalStorageManager::ensureTransientLocalStorageArea(const WebCore::ClientOrigin& origin)
{
    RefPtr transientStorageArea = m_transientStorageArea;
    if (!transientStorageArea) {
        transientStorageArea = MemoryStorageArea::create(origin, StorageAreaBase::StorageType::Local);
        m_transientStorageArea = transientStorageArea;
        m_registry->registerStorageArea(transientStorageArea->identifier(), *transientStorageArea);
    }

    return *m_transientStorageArea;
}

StorageAreaIdentifier LocalStorageManager::connectToTransientLocalStorageArea(IPC::Connection::UniqueID connection, StorageAreaMapIdentifier sourceIdentifier, const WebCore::ClientOrigin& origin)
{
    Ref transientStorageArea = ensureTransientLocalStorageArea(origin);
    ASSERT(is<MemoryStorageArea>(transientStorageArea));
    transientStorageArea->addListener(connection, sourceIdentifier);
    return transientStorageArea->identifier();
}

void LocalStorageManager::cancelConnectToLocalStorageArea(IPC::Connection::UniqueID connection)
{
    connectionClosedForLocalStorageArea(connection);
}

void LocalStorageManager::cancelConnectToTransientLocalStorageArea(IPC::Connection::UniqueID connection)
{
    connectionClosedForLocalStorageArea(connection);
}

void LocalStorageManager::disconnectFromStorageArea(IPC::Connection::UniqueID connection, StorageAreaIdentifier identifier)
{
    if (m_localStorageArea && m_localStorageArea->identifier() == identifier) {
        connectionClosedForLocalStorageArea(connection);
        return;
    }

    if (m_transientStorageArea && m_transientStorageArea->identifier() == identifier)
        connectionClosedForTransientStorageArea(connection);
}

HashMap<String, String> LocalStorageManager::fetchStorageMap() const
{
    if (RefPtr localStorageArea = m_localStorageArea)
        return localStorageArea->allItems();

    if (RefPtr transientStorageArea = m_transientStorageArea)
        return transientStorageArea->allItems();

    return { };
}

bool LocalStorageManager::setStorageMap(WebCore::ClientOrigin clientOrigin, HashMap<String, String>&& storageMap, Ref<WorkQueue>&& workQueue)
{
    bool succeeded = true;

    if (clientOrigin.topOrigin == clientOrigin.clientOrigin) {
        Ref localStorageArea = ensureLocalStorageArea(clientOrigin, WTFMove(workQueue));
        for (auto& [key, value] : storageMap) {
            if (!localStorageArea->setItem({ }, { }, WTFMove(key), WTFMove(value), { }))
                succeeded = false;
        }
    } else {
        Ref transientStorageArea = ensureTransientLocalStorageArea(clientOrigin);
        for (auto& [key, value] : storageMap) {
            if (!transientStorageArea->setItem({ }, { }, WTFMove(key), WTFMove(value), { }))
                succeeded = false;
        }
    }

    return succeeded;
}

} // namespace WebKit
