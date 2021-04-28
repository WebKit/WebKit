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

#include <WebCore/SQLiteDatabase.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class SQLiteStatementAutoResetScope;
class SuddenTerminationDisabler;

struct SecurityOriginData;
}

namespace WebKit {

class LocalStorageDatabaseTracker;

class LocalStorageDatabase : public RefCounted<LocalStorageDatabase> {
public:
    static Ref<LocalStorageDatabase> create(Ref<WorkQueue>&&, Ref<LocalStorageDatabaseTracker>&&, const WebCore::SecurityOriginData&, unsigned quotaInBytes);
    ~LocalStorageDatabase();

    HashMap<String, String> items() const;
    void openIfExisting();
    void removeItem(const String& key, String& oldValue);
    bool clear();
    String item(const String& key) const;
    void setItem(const String& key, const String& value, String& oldValue, bool& quotaException);

    // Will block until all pending changes have been written to disk.
    void close();

    void handleLowMemoryWarning();

private:
    LocalStorageDatabase(Ref<WorkQueue>&&, Ref<LocalStorageDatabaseTracker>&&, const WebCore::SecurityOriginData&, unsigned quotaInBytes);

    enum class ShouldCreateDatabase : bool { No, Yes };
    bool tryToOpenDatabase(ShouldCreateDatabase);
    void openDatabase(ShouldCreateDatabase);

    bool migrateItemTableIfNeeded();
    bool databaseIsEmpty() const;

    WebCore::SQLiteStatementAutoResetScope scopedStatement(std::unique_ptr<WebCore::SQLiteStatement>&, const String& query) const;

    Ref<WorkQueue> m_queue;
    Ref<LocalStorageDatabaseTracker> m_tracker;
    WebCore::SecurityOriginData m_securityOrigin;

    String m_databasePath;
    mutable WebCore::SQLiteDatabase m_database;
    const unsigned m_quotaInBytes { 0 };
    bool m_failedToOpenDatabase { false };
    bool m_isClosed { false };
    Optional<unsigned> m_databaseSize;

    std::unique_ptr<WebCore::SuddenTerminationDisabler> m_disableSuddenTerminationWhileWritingToLocalStorage;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_clearStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_insertStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_getItemStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_getItemsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_deleteItemStatement;
};


} // namespace WebKit
