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
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class SQLiteDatabase;
class SQLiteStatement;
class SQLiteStatementAutoResetScope;
class SQLiteTransaction;
}

namespace WebKit {

class SQLiteStorageArea final : public StorageAreaBase, public RefCounted<SQLiteStorageArea> {
    WTF_MAKE_TZONE_ALLOCATED(SQLiteStorageArea);
public:
    static Ref<SQLiteStorageArea> create(unsigned quota, const WebCore::ClientOrigin&, StringImpl* path, Ref<WorkQueue>&&);
    ~SQLiteStorageArea();

    void close();
    void handleLowMemoryWarning();
    void commitTransactionIfNecessary();
    void clear() final;

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    SQLiteStorageArea(unsigned quota, const WebCore::ClientOrigin&, StringImpl* path, Ref<WorkQueue>&&);

    // StorageAreaBase
    Type type() const final { return StorageAreaBase::Type::SQLite; };
    StorageType storageType() const final { return StorageAreaBase::StorageType::Local; };
    bool isEmpty() final;
    HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> allItems() final;
    std::expected<void, StorageError> setItem(std::optional<IPC::Connection::UniqueID>, std::optional<StorageAreaImplIdentifier>, RefPtr<StringImpl>&& key, RefPtr<StringImpl>&& value, StringImpl* urlString) final;
    std::expected<void, StorageError> removeItem(IPC::Connection::UniqueID, StorageAreaImplIdentifier, StringImpl* key, StringImpl* urlString) final;
    std::expected<void, StorageError> clear(IPC::Connection::UniqueID, StorageAreaImplIdentifier, StringImpl* urlString) final;

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
    std::expected<RefPtr<StringImpl>, StorageError> getItem(StringImpl* key);
    std::expected<RefPtr<StringImpl>, StorageError> getItemFromDatabase(StringImpl* key);
    enum class IsDatabaseDeleted : bool { No, Yes };
    IsDatabaseDeleted handleDatabaseErrorIfNeeded(int databaseError);
    void updateCacheIfNeeded(StringImpl* key, StringImpl* value);
    bool requestSpace(StringImpl* key, StringImpl* value);

    RefPtr<StringImpl> m_path;
    const Ref<WorkQueue> m_queue;
    std::unique_ptr<WebCore::SQLiteDatabase> m_database;
    std::unique_ptr<WebCore::SQLiteTransaction> m_transaction;
    Vector<std::unique_ptr<WebCore::SQLiteStatement>> m_cachedStatements;
    using Value = Variant<RefPtr<StringImpl>, unsigned>;
    std::optional<HashMap<RefPtr<StringImpl>, Value>> m_cache;
    std::optional<unsigned> m_cacheSize;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::SQLiteStorageArea)
    static bool isType(const WebKit::StorageAreaBase& area) { return area.type() == WebKit::StorageAreaBase::Type::SQLite; }
SPECIALIZE_TYPE_TRAITS_END()
