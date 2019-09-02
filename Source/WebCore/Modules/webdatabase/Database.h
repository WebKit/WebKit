/*
 * Copyright (C) 2007, 2008, 2013, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionOr.h"
#include "SQLiteDatabase.h"
#include <wtf/Deque.h>
#include <wtf/Lock.h>

namespace WebCore {

class DatabaseCallback;
class DatabaseContext;
class DatabaseDetails;
class DatabaseThread;
class Document;
class SecurityOrigin;
class SQLTransaction;
class SQLTransactionBackend;
class SQLTransactionCallback;
class SQLTransactionCoordinator;
class SQLTransactionErrorCallback;
class SQLTransactionWrapper;
class VoidCallback;
struct SecurityOriginData;

using DatabaseGUID = int;

class Database : public ThreadSafeRefCounted<Database> {
public:
    ~Database();

    ExceptionOr<void> openAndVerifyVersion(bool setVersionInNewDatabase);
    void close();

    void interrupt();

    bool opened() const { return m_opened; }
    bool isNew() const { return m_new; }

    unsigned long long maximumSize();

    void scheduleTransactionStep(SQLTransaction&);
    void inProgressTransactionCompleted();

    bool hasPendingTransaction();

    bool hasPendingCreationEvent() const { return m_hasPendingCreationEvent; }
    void setHasPendingCreationEvent(bool value) { m_hasPendingCreationEvent = value; }

    void didCommitWriteTransaction();
    bool didExceedQuota();

    SQLTransactionCoordinator* transactionCoordinator();

    // Direct support for the DOM API
    String version() const;
    void changeVersion(const String& oldVersion, const String& newVersion, RefPtr<SQLTransactionCallback>&&, RefPtr<SQLTransactionErrorCallback>&&, RefPtr<VoidCallback>&& successCallback);
    void transaction(RefPtr<SQLTransactionCallback>&&, RefPtr<SQLTransactionErrorCallback>&&, RefPtr<VoidCallback>&& successCallback);
    void readTransaction(RefPtr<SQLTransactionCallback>&&, RefPtr<SQLTransactionErrorCallback>&&, RefPtr<VoidCallback>&& successCallback);

    // Internal engine support
    String stringIdentifierIsolatedCopy() const;
    String displayNameIsolatedCopy() const;
    String expectedVersionIsolatedCopy() const;
    unsigned long long estimatedSize() const;
    String fileNameIsolatedCopy() const;
    DatabaseDetails details() const;
    SQLiteDatabase& sqliteDatabase() { return m_sqliteDatabase; }

    void disableAuthorizer();
    void enableAuthorizer();
    void setAuthorizerPermissions(int);
    bool lastActionChangedDatabase();
    bool lastActionWasInsert();
    void resetDeletes();
    bool hadDeletes();
    void resetAuthorizer();

    DatabaseContext& databaseContext() { return m_databaseContext; }
    DatabaseThread& databaseThread();
    Document& document() { return m_document; }
    void logErrorMessage(const String& message);

    Vector<String> tableNames();

    SecurityOriginData securityOrigin();

    void markAsDeletedAndClose();
    bool deleted() const { return m_deleted; }

    void scheduleTransactionCallback(SQLTransaction*);

    void incrementalVacuumIfNeeded();

    // Called from DatabaseTask
    ExceptionOr<void> performOpenAndVerify(bool shouldSetVersionInNewDatabase);
    Vector<String> performGetTableNames();

    // Called from DatabaseTask and DatabaseThread
    void performClose();

private:
    Database(DatabaseContext&, const String& name, const String& expectedVersion, const String& displayName, unsigned long long estimatedSize);

    void closeDatabase();

    bool getVersionFromDatabase(String& version, bool shouldCacheVersion = true);
    bool setVersionInDatabase(const String& version, bool shouldCacheVersion = true);
    void setExpectedVersion(const String&);
    String getCachedVersion() const;
    void setCachedVersion(const String&);
    bool getActualVersionForTransaction(String& version);
    void setEstimatedSize(unsigned long long);

    void scheduleTransaction();

    void runTransaction(RefPtr<SQLTransactionCallback>&&, RefPtr<SQLTransactionErrorCallback>&&, RefPtr<VoidCallback>&& successCallback, RefPtr<SQLTransactionWrapper>&&, bool readOnly);

#if !LOG_DISABLED || !ERROR_DISABLED
    String databaseDebugName() const;
#endif

    Ref<Document> m_document;
    Ref<SecurityOrigin> m_contextThreadSecurityOrigin;
    Ref<SecurityOrigin> m_databaseThreadSecurityOrigin;
    Ref<DatabaseContext> m_databaseContext;

    bool m_deleted { false };
    bool m_hasPendingCreationEvent { false };

    String m_name;
    String m_expectedVersion;
    String m_displayName;
    unsigned long long m_estimatedSize;
    String m_filename;

    DatabaseGUID m_guid;
    bool m_opened { false };
    bool m_new { false };

    SQLiteDatabase m_sqliteDatabase;

    Ref<DatabaseAuthorizer> m_databaseAuthorizer;

    Deque<Ref<SQLTransaction>> m_transactionQueue;
    Lock m_transactionInProgressMutex;
    bool m_transactionInProgress { false };
    bool m_isTransactionQueueEnabled { true };

    friend class ChangeVersionWrapper;
    friend class DatabaseManager;
    friend class SQLTransaction;
    friend class SQLTransactionBackend;
};

} // namespace WebCore
