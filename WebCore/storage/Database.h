/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef Database_h
#define Database_h

#if ENABLE(DATABASE)
#include "AbstractDatabase.h"
#include "PlatformString.h"
#include "SQLiteDatabase.h"
#ifndef NDEBUG
#include "SecurityOrigin.h"
#endif

#include <wtf/Deque.h>
#include <wtf/Forward.h>

namespace WebCore {

class DatabaseAuthorizer;
class DatabaseCallback;
class DatabaseThread;
class ScriptExecutionContext;
class SecurityOrigin;
class SQLTransaction;
class SQLTransactionCallback;
class SQLTransactionClient;
class SQLTransactionCoordinator;
class SQLTransactionErrorCallback;
class VoidCallback;

typedef int ExceptionCode;

class Database : public AbstractDatabase {
public:
    virtual ~Database();

    // Direct support for the DOM API
    static PassRefPtr<Database> openDatabase(ScriptExecutionContext*, const String& name, const String& expectedVersion, const String& displayName,
                                             unsigned long estimatedSize, PassRefPtr<DatabaseCallback>, ExceptionCode&);
    String version() const;
    void changeVersion(const String& oldVersion, const String& newVersion, PassRefPtr<SQLTransactionCallback>,
                       PassRefPtr<SQLTransactionErrorCallback>, PassRefPtr<VoidCallback> successCallback);
    void transaction(PassRefPtr<SQLTransactionCallback>, PassRefPtr<SQLTransactionErrorCallback>,
                     PassRefPtr<VoidCallback> successCallback, bool readOnly);

    // Internal engine support
    static const String& databaseInfoTableName();

    void disableAuthorizer();
    void enableAuthorizer();
    void setAuthorizerReadOnly();
    bool lastActionChangedDatabase();
    bool lastActionWasInsert();
    void resetDeletes();
    bool hadDeletes();

    Vector<String> tableNames();

    virtual ScriptExecutionContext* scriptExecutionContext() const { return m_scriptExecutionContext.get(); }
    virtual SecurityOrigin* securityOrigin() const;
    SQLiteDatabase& sqliteDatabase() { return m_sqliteDatabase; }
    virtual String stringIdentifier() const;
    virtual String displayName() const;
    virtual unsigned long estimatedSize() const;
    virtual String fileName() const;

    bool getVersionFromDatabase(String&);
    bool setVersionInDatabase(const String&);
    void setExpectedVersion(const String&);
    bool versionMatchesExpected() const;

    virtual void markAsDeletedAndClose();
    bool deleted() const { return m_deleted; }

    enum ClosePolicy { DoNotRemoveDatabaseFromContext, RemoveDatabaseFromContext };
    void close(ClosePolicy);
    virtual void closeImmediately();
    bool opened() const { return m_opened; }

    void stop();
    bool stopped() const { return m_stopped; }

    bool isNew() const { return m_new; }

    unsigned long long databaseSize() const;
    unsigned long long maximumSize() const;

    // Called from DatabaseThread, must be prepared to work on the background thread
    void resetAuthorizer();
    void performPolicyChecks();

    bool performOpenAndVerify(ExceptionCode&);

    void inProgressTransactionCompleted();
    void scheduleTransactionCallback(SQLTransaction*);
    void scheduleTransactionStep(SQLTransaction*, bool immediately = false);

    Vector<String> performGetTableNames();
    void performCreationCallback();

    SQLTransactionClient* transactionClient() const;
    SQLTransactionCoordinator* transactionCoordinator() const;

    void incrementalVacuumIfNeeded();

private:
    Database(ScriptExecutionContext*, const String& name, const String& expectedVersion,
             const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback>);

    bool openAndVerifyVersion(ExceptionCode&);

    void scheduleTransaction();

    Deque<RefPtr<SQLTransaction> > m_transactionQueue;
    Mutex m_transactionInProgressMutex;
    bool m_transactionInProgress;
    bool m_isTransactionQueueEnabled;

    static void deliverPendingCallback(void*);

    RefPtr<ScriptExecutionContext> m_scriptExecutionContext;
    RefPtr<SecurityOrigin> m_contextThreadSecurityOrigin;
    RefPtr<SecurityOrigin> m_databaseThreadSecurityOrigin;
    String m_name;
    int m_guid;
    String m_expectedVersion;
    String m_displayName;
    unsigned long m_estimatedSize;
    String m_filename;

    bool m_deleted;

    bool m_stopped;

    bool m_opened;

    bool m_new;

    SQLiteDatabase m_sqliteDatabase;
    RefPtr<DatabaseAuthorizer> m_databaseAuthorizer;

    RefPtr<DatabaseCallback> m_creationCallback;

#ifndef NDEBUG
    String databaseDebugName() const { return m_contextThreadSecurityOrigin->toString() + "::" + m_name; }
#endif
};

} // namespace WebCore

#endif // ENABLE(DATABASE)

#endif // Database_h
