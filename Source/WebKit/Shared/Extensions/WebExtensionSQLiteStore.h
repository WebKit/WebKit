/*
 * Copyright (C) 2024 Igalia, S.L. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "WebExtensionSQLiteDatabase.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Markable.h>
#include <wtf/Noncopyable.h>
#include <wtf/UUID.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

typedef int SchemaVersion;
typedef int DatabaseResult;
using WTF::UUID;

class WebExtensionSQLiteStore : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<WebExtensionSQLiteStore> {
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionSQLiteStore);
    WTF_MAKE_NONCOPYABLE(WebExtensionSQLiteStore)

public:
    WebExtensionSQLiteStore(StringImpl* uniqueIdentifier, StringImpl* directory, bool useInMemoryDatabase);
    virtual ~WebExtensionSQLiteStore() { close(); };

    SchemaVersion databaseSchemaVersion();
    bool useInMemoryDatabase() { return m_useInMemoryDatabase; };

    bool openDatabaseIfNecessary(RefPtr<StringImpl>& outErrorMessage, bool createIfNecessary);

    void close();
    void deleteDatabase(CompletionHandler<void(StringImpl* errorMessage)>&&);
    RefPtr<StringImpl> deleteDatabaseIfEmpty();

    void createSavepoint(CompletionHandler<void(Markable<WTF::UUID> savepointIdentifier, StringImpl* errorMessage)>&&);
    void commitSavepoint(WTF::UUID& savepointIdentifier, CompletionHandler<void(StringImpl* errorMessage)>&&);
    void rollbackToSavepoint(WTF::UUID& savepointIdentifier, CompletionHandler<void(StringImpl* errorMessage)>&&);

protected:
    virtual DatabaseResult createFreshDatabaseSchema() = 0;
    virtual DatabaseResult resetDatabaseSchema() = 0;
    virtual bool isDatabaseEmpty() = 0;
    virtual SchemaVersion currentDatabaseSchemaVersion() = 0;
    virtual URL databaseURL() = 0;

    DatabaseResult setDatabaseSchemaVersion(SchemaVersion newVersion);
    virtual SchemaVersion migrateToCurrentSchemaVersionIfNeeded();

    WorkQueue& queue() { return m_queue; };
    RefPtr<WebExtensionSQLiteDatabase> database() { return m_database; };
    RefPtr<StringImpl> uniqueIdentifier() { return m_uniqueIdentifier; };
    const UTF8CString& lastErrorMessage() LIFETIME_BOUND { return m_database->m_lastErrorMessage; };
    URL directory() { return m_directory; };

private:
    void vacuum();
    NODELETE bool isDatabaseOpen();
    RefPtr<StringImpl> openDatabase(const URL& databaseURL, WebExtensionSQLiteDatabase::AccessType, bool deleteDatabaseFileOnError);
    RefPtr<StringImpl> deleteDatabaseFileAtURL(const URL& databaseURL, bool reopenDatabase);
    RefPtr<StringImpl> deleteDatabase();

    RefPtr<StringImpl> savepointNameFromUUID(const WTF::UUID& savepointIdentifier);

    RefPtr<StringImpl> handleSchemaVersioning(bool deleteDatabaseFileOnError);

    RefPtr<StringImpl> m_uniqueIdentifier;
    URL m_directory;
    RefPtr<WebExtensionSQLiteDatabase> m_database;
    const Ref<WorkQueue> m_queue;
    bool m_useInMemoryDatabase;
    bool m_savepointsAreValid { true };
};

} // namespace WebKit
