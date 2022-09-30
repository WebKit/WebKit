/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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

#include "StorageAreaBase.h"
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatementAutoResetScope.h>

namespace WebKit {

class SQLiteStorageArea final : public StorageAreaBase {
public:
    SQLiteStorageArea(unsigned quota, const WebCore::ClientOrigin&, const String& path, Ref<WorkQueue>&&);
    ~SQLiteStorageArea();

    void close();
    void handleLowMemoryWarning();
    void commitTransactionIfNecessary();

private:
    // StorageAreaBase
    Type type() const final { return StorageAreaBase::Type::SQLite; };
    StorageType storageType() const final { return StorageAreaBase::StorageType::Local; };
    bool isEmpty() final;
    void clear() final;
    HashMap<String, String> allItems() final;
    Expected<void, StorageError> setItem(IPC::Connection::UniqueID, StorageAreaImplIdentifier, String&& key, String&& value, const String& urlString) final;
    Expected<void, StorageError> removeItem(IPC::Connection::UniqueID, StorageAreaImplIdentifier, const String& key, const String& urlString) final;
    Expected<void, StorageError> clear(IPC::Connection::UniqueID, StorageAreaImplIdentifier, const String& urlString) final;

    bool createTableIfNecessary();
    enum class ShouldCreateIfNotExists : bool { No, Yes };
    bool prepareDatabase(ShouldCreateIfNotExists);
    void startTransactionIfNecessary();

    enum class StatementType : uint8_t {
        CountItems,
        DeleteItem,
        DeleteAllItems,
        GetItem,
        GetAllItems,
        SetItem,
        Invalid
    };
    ASCIILiteral statementString(StatementType) const;
    WebCore::SQLiteStatementAutoResetScope cachedStatement(StatementType);
    Expected<String, StorageError> getItem(const String& key);
    Expected<String, StorageError> getItemFromDatabase(const String& key);
    Expected<HashMap<String, String>, StorageError> getAllItemsFromDatabase();
    void initializeCache(const HashMap<String, String>&);

    String m_path;
    Ref<WorkQueue> m_queue;
    std::unique_ptr<WebCore::SQLiteDatabase> m_database;
    std::unique_ptr<WebCore::SQLiteTransaction> m_transaction;
    Vector<std::unique_ptr<WebCore::SQLiteStatement>> m_cachedStatements;
    struct Cache {
        HashMap<String, String> items;
        uint64_t size { 0 };
    };
    std::optional<Cache> m_cache;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::SQLiteStorageArea)
    static bool isType(const WebKit::StorageAreaBase& area) { return area.type() == WebKit::StorageAreaBase::Type::SQLite; }
SPECIALIZE_TYPE_TRAITS_END()
