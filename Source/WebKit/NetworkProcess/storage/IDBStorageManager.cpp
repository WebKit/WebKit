/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "IDBStorageManager.h"

#include "IDBStorageRegistry.h"
#include <WebCore/IDBRequestData.h>
#include <WebCore/IDBServer.h>
#include <WebCore/MemoryIDBBackingStore.h>
#include <WebCore/SQLiteFileSystem.h>
#include <WebCore/SQLiteIDBBackingStore.h>

namespace WebKit {

static bool migrateOriginDataImpl(const String& oldOriginDirectory, const String& newOriginDirectory, Function<String(const String&)>&& createFileNameFunction)
{
    if (oldOriginDirectory.isEmpty() || !FileSystem::fileExists(oldOriginDirectory))
        return true;

    auto fileNames = FileSystem::listDirectory(oldOriginDirectory);
    if (fileNames.isEmpty()) {
        FileSystem::deleteEmptyDirectory(oldOriginDirectory);
        return true;
    }

    FileSystem::makeAllDirectories(newOriginDirectory);
    bool allMoved = true;
    for (auto& name : fileNames) {
        // This is an origin directory for third-party data.
        if (auto origin = WebCore::SecurityOriginData::fromDatabaseIdentifier(name))
            continue;

        // Do not overwrite existing files.
        auto newPath = FileSystem::pathByAppendingComponent(newOriginDirectory, createFileNameFunction(name));
        if (FileSystem::fileExists(newPath))
            continue;

        auto oldPath = FileSystem::pathByAppendingComponent(oldOriginDirectory, name);
        allMoved &= FileSystem::moveFile(oldPath, newPath);
    }

    FileSystem::deleteEmptyDirectory(oldOriginDirectory);
    return allMoved;
}

void IDBStorageManager::createVersionDirectoryIfNeeded(const String& rootDirectory)
{
    if (rootDirectory.isEmpty())
        return;

    bool oldVersionDirectoryExists = false;
    Vector<String> oldVersionNames;
    auto fileNames = FileSystem::listDirectory(rootDirectory);
    for (auto& name : fileNames) {
        if (name == "v0"_s)
            oldVersionDirectoryExists = true;
        if (name != "v0"_s && name != "v1"_s) {
            if (auto origin = WebCore::SecurityOriginData::fromDatabaseIdentifier(name))
                oldVersionNames.append(name);
        }
    }

    if (oldVersionNames.isEmpty() && !oldVersionDirectoryExists)
        return;

    // Delete file or symlink named v0.
    auto oldVersionDirectory = FileSystem::pathByAppendingComponent(rootDirectory, "v0"_s);
    if (auto fileType = FileSystem::fileType(oldVersionDirectory)) {
        // v0 should not be a file in normal case.
        if (*fileType == FileSystem::FileType::Regular) {
            FileSystem::deleteFile(oldVersionDirectory);
            oldVersionDirectoryExists = false;
        } else if (*fileType == FileSystem::FileType::SymbolicLink) {
            FileSystem::deleteNonEmptyDirectory(oldVersionDirectory);
            oldVersionDirectoryExists = false;
        }
    }

    if (oldVersionNames.isEmpty()) {
        if (oldVersionDirectoryExists)
            FileSystem::deleteEmptyDirectory(oldVersionDirectory);
        return;
    }

    // Migrate data to v0 directory.
    if (!oldVersionDirectoryExists)
        FileSystem::makeAllDirectories(oldVersionDirectory);

    for (auto& name : oldVersionNames) {
        auto oldPath = FileSystem::pathByAppendingComponent(rootDirectory, name);
        auto newPath = FileSystem::pathByAppendingComponent(oldVersionDirectory, name);
        FileSystem::moveFile(oldPath, newPath);
    }
}

String IDBStorageManager::idbStorageOriginDirectory(const String& rootDirectory, const WebCore::ClientOrigin& origin)
{
    if (rootDirectory.isEmpty())
        return emptyString();

    auto originDirectory = WebCore::IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin, rootDirectory, "v1"_s);
    auto oldOriginDirectory = WebCore::IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin, rootDirectory, "v0"_s);
    migrateOriginDataImpl(oldOriginDirectory, originDirectory, [](const String& name) {
        return WebCore::SQLiteFileSystem::computeHashForFileName(WebCore::IDBServer::SQLiteIDBBackingStore::decodeDatabaseName(name));
    });

    return originDirectory;
}

uint64_t IDBStorageManager::idbStorageSize(const String& originDirectory)
{
    if (originDirectory.isEmpty())
        return 0;

    return WebCore::IDBServer::SQLiteIDBBackingStore::databasesSizeForDirectory(originDirectory);
}

static void getOriginsForVersion(const String& versionPath, HashSet<WebCore::ClientOrigin>& origins)
{
    for (auto& topDatabaseIdentifier : FileSystem::listDirectory(versionPath)) {
        auto topOrigin = WebCore::SecurityOriginData::fromDatabaseIdentifier(topDatabaseIdentifier);
        if (!topOrigin)
            continue;

        auto topOriginDirectory = FileSystem::pathByAppendingComponent(versionPath, topDatabaseIdentifier);
        for (auto& databaseIdentifier : FileSystem::listDirectory(topOriginDirectory)) {
            auto originDirectory = FileSystem::pathByAppendingComponent(topOriginDirectory, databaseIdentifier);
            if (FileSystem::deleteEmptyDirectory(originDirectory))
                continue;

            auto clientOrigin = WebCore::SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier);
            // This is not origin directory, but may be database directory.
            if (!clientOrigin) {
                origins.add(WebCore::ClientOrigin { *topOrigin, *topOrigin });
                continue;
            }

            origins.add(WebCore::ClientOrigin { *topOrigin, *clientOrigin });
        }

        // We may have deleted empty children directories above, which make this directory empty.
        FileSystem::deleteEmptyDirectory(topOriginDirectory);
    }
}

HashSet<WebCore::ClientOrigin> IDBStorageManager::originsOfIDBStorageData(const String& rootDirectory)
{
    HashSet<WebCore::ClientOrigin> origins;
    if (rootDirectory.isEmpty())
        return origins;

    getOriginsForVersion(FileSystem::pathByAppendingComponent(rootDirectory, "v0"_s), origins);
    getOriginsForVersion(FileSystem::pathByAppendingComponent(rootDirectory, "v1"_s), origins);

    return origins;
}

bool IDBStorageManager::migrateOriginData(const String& oldOriginDirectory, const String& newOriginDirectory)
{
    return migrateOriginDataImpl(oldOriginDirectory, newOriginDirectory, [](const String& name) {
        return name;
    });
}

IDBStorageManager::IDBStorageManager(const String& path, IDBStorageRegistry& registry, QuotaCheckFunction&& quotaCheckFunction)
    : m_path(path)
    , m_registry(registry)
    , m_quotaCheckFunction(WTFMove(quotaCheckFunction))
{
}

IDBStorageManager::~IDBStorageManager()
{
    for (auto& database : m_databases.values())
        database->immediateClose();
}

bool IDBStorageManager::isActive() const
{
    return !m_databases.isEmpty();
}

bool IDBStorageManager::hasDataInMemory() const
{
    return WTF::anyOf(m_databases.values(), [&] (auto& database) {
        return database->hasDataInMemory();
    });
}

void IDBStorageManager::closeDatabasesForDeletion()
{
    for (auto& database : m_databases.values())
        database->immediateClose();

    m_databases.clear();
}

void IDBStorageManager::stopDatabaseActivitiesForSuspend()
{
    if (m_path.isEmpty())
        return;

    for (auto& database : m_databases.values()) {
        // Only stop databases with non-ephemeral backing store that can hold database file lock.
        if (!database->identifier().isTransient())
            database->abortActiveTransactions();
    }
}

WebCore::IDBServer::UniqueIDBDatabase& IDBStorageManager::getOrCreateUniqueIDBDatabase(const WebCore::IDBDatabaseIdentifier& identifier)
{
    auto addResult = m_databases.add(identifier, nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = makeUnique<WebCore::IDBServer::UniqueIDBDatabase>(*this, identifier);

    return *addResult.iterator->value;
}

void IDBStorageManager::openDatabase(WebCore::IDBServer::IDBConnectionToClient& connectionToClient, const WebCore::IDBOpenRequestData& requestData)
{
    auto& database = getOrCreateUniqueIDBDatabase(requestData.databaseIdentifier());
    database.openDatabaseConnection(connectionToClient, requestData);
}

void IDBStorageManager::deleteDatabase(WebCore::IDBServer::IDBConnectionToClient& connectionToClient, const WebCore::IDBOpenRequestData& requestData)
{
    auto& database = getOrCreateUniqueIDBDatabase(requestData.databaseIdentifier());
    database.handleDelete(connectionToClient, requestData);

    // This database is created for deletion.
    if (database.tryClose())
        m_databases.remove(database.identifier());
}

Vector<WebCore::IDBDatabaseNameAndVersion> IDBStorageManager::getAllDatabaseNamesAndVersions()
{
    Vector<WebCore::IDBDatabaseNameAndVersion> result;
    HashSet<String> visitedDatabasePaths;
    for (auto& database : m_databases.values()) {
        auto path = database->filePath();
        if (!path.isEmpty())
            visitedDatabasePaths.add(path);

        if (auto nameAndVersion = database->nameAndVersion())
            result.append(WTFMove(*nameAndVersion));
    }

    auto databaseIdentifiers = FileSystem::listDirectory(m_path);
    for (auto identifier : databaseIdentifiers) {
        auto databaseDirectory = FileSystem::pathByAppendingComponent(m_path, identifier);
        auto databasePath = WebCore::IDBServer::SQLiteIDBBackingStore::fullDatabasePathForDirectory(databaseDirectory);
        if (visitedDatabasePaths.contains(databasePath))
            continue;

        if (auto nameAndVersion = WebCore::IDBServer::SQLiteIDBBackingStore::databaseNameAndVersionFromFile(databasePath))
            result.append(WTFMove(*nameAndVersion));
    }

    return result;
}

void IDBStorageManager::openDBRequestCancelled(const WebCore::IDBOpenRequestData& requestData)
{
    auto* database = m_databases.get(requestData.databaseIdentifier());
    if (!database)
        return;

    database->openDBRequestCancelled(requestData.requestIdentifier());

    // Database becomes idle after request is cancelled.
    if (database->tryClose())
        m_databases.remove(database->identifier());
}

void IDBStorageManager::registerConnection(WebCore::IDBServer::UniqueIDBDatabaseConnection& connection)
{
    m_registry.registerConnection(connection);
}

void IDBStorageManager::unregisterConnection(WebCore::IDBServer::UniqueIDBDatabaseConnection& connection)
{
    m_registry.unregisterConnection(connection);
}

void IDBStorageManager::registerTransaction(WebCore::IDBServer::UniqueIDBDatabaseTransaction& transaction)
{
    m_registry.registerTransaction(transaction);
}

void IDBStorageManager::unregisterTransaction(WebCore::IDBServer::UniqueIDBDatabaseTransaction& transaction)
{
    m_registry.unregisterTransaction(transaction);
}

std::unique_ptr<WebCore::IDBServer::IDBBackingStore> IDBStorageManager::createBackingStore(const WebCore::IDBDatabaseIdentifier& identifier)
{
    if (m_path.isEmpty() || identifier.isTransient())
        return makeUnique<WebCore::IDBServer::MemoryIDBBackingStore>(identifier);

    auto name = WebCore::SQLiteFileSystem::computeHashForFileName(identifier.databaseName());
    return makeUnique<WebCore::IDBServer::SQLiteIDBBackingStore>(identifier, FileSystem::pathByAppendingComponent(m_path, name));
}

void IDBStorageManager::requestSpace(const WebCore::ClientOrigin&, uint64_t size, CompletionHandler<void(bool)>&& completionHandler)
{
    m_quotaCheckFunction(size, WTFMove(completionHandler));
}

void IDBStorageManager::handleLowMemoryWarning()
{
    for (auto& database : m_databases.values())
        database->handleLowMemoryWarning();
}

} // namespace WebKit
