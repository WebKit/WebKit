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

#pragma once

#include <WebCore/UniqueIDBDatabase.h>
#include <WebCore/UniqueIDBDatabaseManager.h>

namespace WebCore {
class IDBRequestData;

namespace IDBServer {
class IDBBackingStore;
class IDBConnectionToClient;
class UniqueIDBTransaction;
}

struct IDBDatabaseNameAndVersion;
}

namespace WebKit {

class IDBStorageRegistry;

class IDBStorageManager final : public WebCore::IDBServer::UniqueIDBDatabaseManager {
public:
    static void createVersionDirectoryIfNeeded(const String& rootDirectory);
    static String idbStorageOriginDirectory(const String& rootDirectory, const WebCore::ClientOrigin&);
    static uint64_t idbStorageSize(const String& originDirectory);
    static HashSet<WebCore::ClientOrigin> originsOfIDBStorageData(const String& rootDirectory);
    static bool migrateOriginData(const String& oldOriginDirectory, const String& newOriginDirectory);

    using QuotaCheckFunction = Function<void(uint64_t spaceRequested, CompletionHandler<void(bool)>&&)>;
    IDBStorageManager(const String& path, IDBStorageRegistry&, QuotaCheckFunction&&);
    ~IDBStorageManager();
    bool isActive() const;
    bool hasDataInMemory() const;
    void closeDatabasesForDeletion();
    void stopDatabaseActivitiesForSuspend();
    void handleLowMemoryWarning();

    void openDatabase(WebCore::IDBServer::IDBConnectionToClient&, const WebCore::IDBOpenRequestData&);
    void openDBRequestCancelled(const WebCore::IDBOpenRequestData&);
    void deleteDatabase(WebCore::IDBServer::IDBConnectionToClient&, const WebCore::IDBOpenRequestData&);
    Vector<WebCore::IDBDatabaseNameAndVersion> getAllDatabaseNamesAndVersions();

private:
    WebCore::IDBServer::UniqueIDBDatabase& getOrCreateUniqueIDBDatabase(const WebCore::IDBDatabaseIdentifier&);

    // WebCore::UniqueIDBDatabaseManager
    void registerConnection(WebCore::IDBServer::UniqueIDBDatabaseConnection&) final;
    void unregisterConnection(WebCore::IDBServer::UniqueIDBDatabaseConnection&) final;
    void registerTransaction(WebCore::IDBServer::UniqueIDBDatabaseTransaction&) final;
    void unregisterTransaction(WebCore::IDBServer::UniqueIDBDatabaseTransaction&) final;
    std::unique_ptr<WebCore::IDBServer::IDBBackingStore> createBackingStore(const WebCore::IDBDatabaseIdentifier&) final;
    void requestSpace(const WebCore::ClientOrigin&, uint64_t size, CompletionHandler<void(bool)>&&) final;

    String m_path;
    IDBStorageRegistry& m_registry;
    QuotaCheckFunction m_quotaCheckFunction;
    HashMap<WebCore::IDBDatabaseIdentifier, std::unique_ptr<WebCore::IDBServer::UniqueIDBDatabase>> m_databases;
};

} // namespace WebKit
