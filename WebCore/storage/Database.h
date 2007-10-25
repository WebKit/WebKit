/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "SQLiteDatabase.h"
#include "StringHash.h"
#include "Threading.h"
#include "Timer.h"
#include "VersionChangeCallback.h"

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Deque.h>

namespace WebCore {

class DatabaseAuthorizer;
class DatabaseCallback;
class DatabaseThread;
class Document;
class SQLCallback;
class SQLResultSet;
class SQLValue;

typedef int ExceptionCode;

class Database : public ThreadSafeShared<Database> {
public:
    ~Database();

// Direct support for the DOM API
    static PassRefPtr<Database> openDatabase(Document* document, const String& name, const String& expectedVersion, ExceptionCode&);

    String version() const;
    void changeVersion(const String& oldVersion, const String& newVersion, PassRefPtr<VersionChangeCallback> callback);

    void executeSql(const String& sqlStatement, const Vector<SQLValue>& arguments, PassRefPtr<SQLCallback> callback, ExceptionCode&);
    void closeTransaction();

// Internal engine support
    void databaseThreadGoingAway();
    static const String& databaseInfoTableName();

    void disableAuthorizer();
    void enableAuthorizer();

    Vector<String> tableNames();

    Document* document() const { return m_document; }
    
// Called from DatabaseThread, must be prepared to work on the background thread
    void resetAuthorizer();
    void performPolicyChecks();

    bool performOpenAndVerify(ExceptionCode&);
    void performChangeVersion(const String& oldVersion, const String& newVersion, PassRefPtr<VersionChangeCallback> callback);
    void performExecuteSql(const String& sqlStatement, const Vector<SQLValue>& arguments, PassRefPtr<SQLCallback> callback);
    void performCloseTransaction();
    Vector<String> performGetTableNames();

private:
    Database(Document* document, const String& name, const String& expectedVersion);

    bool openAndVerifyVersion(ExceptionCode&);
    bool getVersionFromDatabase(String&);
    bool setVersionInDatabase(const String&);

    static void scheduleFileSizeTimerOnMainThread(Database*);
    static void performScheduleFileSizeTimers();
    void scheduleFileSizeTimer();
    void sizeTimerFired(Timer<Database>*);
    OwnPtr<Timer<Database> > m_sizeTimer;

    static void deliverAllPendingCallbacks();
    void deliverPendingCallbacks();

    void scheduleDatabaseCallback(DatabaseCallback*);

    // FIXME: Is it okay to reconcile hanging on to the raw pointer?  We definitely have to rely on
    // Document telling DatabaseThread it's going away, and DatabaseThread telling Database its going away
    Document* m_document;
    SecurityOrigin m_securityOrigin;
    String m_name;
    int m_guid;
    String m_expectedVersion;
    String m_filename;

    SQLiteDatabase m_mainSQLDatabase;
    SQLiteDatabase m_threadSQLDatabase;
    RefPtr<DatabaseAuthorizer> m_databaseAuthorizer;

    Mutex m_databaseThreadMutex;
    DatabaseThread* m_databaseThread;

    Mutex m_callbackMutex;
    Vector<RefPtr<DatabaseCallback> > m_pendingCallbacks;

#ifndef NDEBUG
    String databaseDebugName() const { return m_securityOrigin.toString() + "::" + m_name; }
#endif

    static Mutex& globalCallbackMutex();
    static HashSet<RefPtr<Database> >& globalCallbackSet();
    static bool s_globalCallbackScheduled;

    static int guidForOriginAndName(const String&, const String&);

    static Mutex& guidMutex();
    static HashMap<int, String>& guidToVersionMap();
    static HashMap<int, HashSet<Database*>*>& guidToDatabaseMap();
};

} // namespace WebCore

#endif // Database_h
