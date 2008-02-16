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

#include "MessageQueue.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "SQLiteDatabase.h"
#include "SQLTransaction.h"
#include "StringHash.h"
#include "Timer.h"
#include "VoidCallback.h"

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Deque.h>

namespace WebCore {

class DatabaseAuthorizer;
class DatabaseThread;
class Document;
class SQLResultSet;
class SQLTransactionCallback;
class SQLTransactionErrorCallback;
class SQLValue;
    
typedef int ExceptionCode;

class Database : public ThreadSafeShared<Database> {
    friend class DatabaseTransactionTask;
    friend class SQLStatement;
    friend class SQLTransaction;
public:
    ~Database();

// Direct support for the DOM API
    static PassRefPtr<Database> openDatabase(Document* document, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize, ExceptionCode&);
    String version() const;
    void changeVersion(const String& oldVersion, const String& newVersion, 
                       PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback,
                       PassRefPtr<VoidCallback> successCallback);
    void transaction(PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback,
                     PassRefPtr<VoidCallback> successCallback);
    
// Internal engine support
    static const String& databaseInfoTableName();

    void disableAuthorizer();
    void enableAuthorizer();

    Vector<String> tableNames();

    Document* document() const { return m_document.get(); }
    PassRefPtr<SecurityOrigin> securityOriginCopy() const;
    String stringIdentifier() const;
    
    bool getVersionFromDatabase(String&);
    bool setVersionInDatabase(const String&);
    void setExpectedVersion(const String&);
    bool versionMatchesExpected() const;

    void markAsDeletedAndClose();
    bool deleted() const { return m_deleted; }

    void close();
    
    void stop();
    bool stopped() const { return m_stopped; }

    unsigned long long databaseSize() const;
    unsigned long long maximumSize() const;

// Called from DatabaseThread, must be prepared to work on the background thread
    void resetAuthorizer();
    void performPolicyChecks();

    bool performOpenAndVerify(ExceptionCode&);

    Vector<String> performGetTableNames();

private:
    Database(Document* document, const String& name, const String& expectedVersion);

    bool openAndVerifyVersion(ExceptionCode&);

    void scheduleTransaction();
    void scheduleTransactionCallback(SQLTransaction*);
    void scheduleTransactionStep(SQLTransaction* transaction);
    
    MessageQueue<RefPtr<SQLTransaction> > m_transactionQueue;
    Mutex m_transactionInProgressMutex;
    bool m_transactionInProgress;

    static void deliverPendingCallback(void*);

    RefPtr<Document> m_document;
    RefPtr<SecurityOrigin> m_securityOrigin;
    String m_name;
    int m_guid;
    String m_expectedVersion;
    String m_filename;

    bool m_deleted;
    
    bool m_stopped;

    SQLiteDatabase m_sqliteDatabase;
    RefPtr<DatabaseAuthorizer> m_databaseAuthorizer;

#ifndef NDEBUG
    String databaseDebugName() const { return m_securityOrigin->toString() + "::" + m_name; }
#endif
};

} // namespace WebCore

#endif // Database_h
