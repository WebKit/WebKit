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

namespace WebKit {

// Suggested by https://www.w3.org/TR/webstorage/#disk-space
constexpr unsigned localStorageQuotaInBytes = 5 * MB;
constexpr auto fileSuffix = ".localstorage"_s;
constexpr auto fileName = "localstorage.sqlite3"_s;

// This is intended to be used for existing files.
// We should not include origin in file name.
static std::optional<WebCore::SecurityOriginData> fileNameToOrigin(const String& fileName)
{
    if (!fileName.endsWith(fileSuffix))
        return std::nullopt;

    auto suffixLength = strlen(fileSuffix);
    auto fileNameLength = fileName.length();
    if (fileNameLength <= suffixLength)
        return std::nullopt;

    auto originIdentifier = fileName.substring(0, fileNameLength - suffixLength);
    return WebCore::SecurityOriginData::fromDatabaseIdentifier(originIdentifier);
}

static String originToFileName(const WebCore::ClientOrigin& origin)
{
    return origin.clientOrigin.databaseIdentifier() + ".localstorage";
}

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

    return FileSystem::pathByAppendingComponent(directory, fileName);
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
    bool hasDataInLocalStorageArea = m_localStorageArea && is<MemoryStorageArea>(*m_localStorageArea) && !m_localStorageArea->isEmpty();
    return hasDataInLocalStorageArea || (m_transientStorageArea && !m_transientStorageArea->isEmpty());
}

void LocalStorageManager::clearDataInMemory()
{
    if (m_localStorageArea && is<MemoryStorageArea>(*m_localStorageArea))
        m_localStorageArea->clear();

    if (m_transientStorageArea)
        m_transientStorageArea->clear();
}

void LocalStorageManager::clearDataOnDisk()
{
    if (m_localStorageArea && is<SQLiteStorageArea>(*m_localStorageArea))
        m_localStorageArea->clear();
}

void LocalStorageManager::close()
{
    if (m_localStorageArea && is<SQLiteStorageArea>(*m_localStorageArea))
        downcast<SQLiteStorageArea>(*m_localStorageArea).close();
}

void LocalStorageManager::handleLowMemoryWarning()
{
    if (m_localStorageArea && is<SQLiteStorageArea>(*m_localStorageArea))
        downcast<SQLiteStorageArea>(*m_localStorageArea).handleLowMemoryWarning();
}

void LocalStorageManager::syncLocalStorage()
{
    if (m_localStorageArea && is<SQLiteStorageArea>(*m_localStorageArea))
        downcast<SQLiteStorageArea>(*m_localStorageArea).commitTransactionIfNecessary();
}

void LocalStorageManager::connectionClosed(IPC::Connection::UniqueID connection)
{
    connectionClosedForLocalStorageArea(connection);
    connectionClosedForTransientStorageArea(connection);
}

void LocalStorageManager::connectionClosedForLocalStorageArea(IPC::Connection::UniqueID connection)
{
    if (!m_localStorageArea)
        return;

    m_localStorageArea->removeListener(connection);
    if (m_localStorageArea->hasListeners())
        return;

    if (is<MemoryStorageArea>(*m_localStorageArea) && !m_localStorageArea->isEmpty())
        return;

    m_registry.unregisterStorageArea(m_localStorageArea->identifier());
    m_localStorageArea = nullptr;
}

void LocalStorageManager::connectionClosedForTransientStorageArea(IPC::Connection::UniqueID connection)
{
    if (!m_transientStorageArea)
        return;

    m_transientStorageArea->removeListener(connection);
    if (m_transientStorageArea->hasListeners() || !m_transientStorageArea->isEmpty())
        return;

    m_registry.unregisterStorageArea(m_transientStorageArea->identifier());
    m_transientStorageArea = nullptr;
}

StorageAreaIdentifier LocalStorageManager::connectToLocalStorageArea(IPC::Connection::UniqueID connection, StorageAreaMapIdentifier sourceIdentifier, const WebCore::ClientOrigin& origin, Ref<WorkQueue>&& workQueue)
{
    if (!m_localStorageArea) {
        if (!m_path.isEmpty())
            m_localStorageArea = makeUnique<SQLiteStorageArea>(localStorageQuotaInBytes, origin, m_path, WTFMove(workQueue));
        else
            m_localStorageArea = makeUnique<MemoryStorageArea>(origin, StorageAreaBase::StorageType::Local);

        m_registry.registerStorageArea(m_localStorageArea->identifier(), *m_localStorageArea);
    }

    ASSERT(m_path.isEmpty() || m_localStorageArea->type() == StorageAreaBase::Type::SQLite);
    m_localStorageArea->addListener(connection, sourceIdentifier);
    return m_localStorageArea->identifier();
}

StorageAreaIdentifier LocalStorageManager::connectToTransientLocalStorageArea(IPC::Connection::UniqueID connection, StorageAreaMapIdentifier sourceIdentifier, const WebCore::ClientOrigin& origin)
{
    if (!m_transientStorageArea) {
        m_transientStorageArea = makeUnique<MemoryStorageArea>(origin, StorageAreaBase::StorageType::Local);
        m_registry.registerStorageArea(m_transientStorageArea->identifier(), *m_transientStorageArea);
    }

    ASSERT(m_transientStorageArea->type() == StorageAreaBase::Type::Memory);
    m_transientStorageArea->addListener(connection, sourceIdentifier);
    return m_transientStorageArea->identifier();
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

} // namespace WebKit
