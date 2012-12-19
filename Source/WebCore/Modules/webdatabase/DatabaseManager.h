/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef DatabaseManager_h
#define DatabaseManager_h

#if ENABLE(SQL_DATABASE)

#include "AbstractDatabaseServer.h"
#include "DatabaseBasicTypes.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

class AbstractDatabaseServer;
class Database;
class DatabaseCallback;
class DatabaseManagerClient;
class DatabaseSync;
class DatabaseTaskSynchronizer;
class SecurityOrigin;
class ScriptExecutionContext;

class DatabaseManager {
    WTF_MAKE_NONCOPYABLE(DatabaseManager); WTF_MAKE_FAST_ALLOCATED;
public:
    static DatabaseManager& manager();

    void initialize(const String& databasePath);
    void setClient(DatabaseManagerClient*);
    String databaseDirectoryPath() const;
    void setDatabaseDirectoryPath(const String&);

    bool isAvailable();
    void setIsAvailable(bool);

    PassRefPtr<Database> openDatabase(ScriptExecutionContext*, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback>, ExceptionCode&);
    PassRefPtr<DatabaseSync> openDatabaseSync(ScriptExecutionContext*, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback>, ExceptionCode&);

    bool hasOpenDatabases(ScriptExecutionContext*);
    void setHasOpenDatabases(ScriptExecutionContext*);
    void stopDatabases(ScriptExecutionContext*, DatabaseTaskSynchronizer*);

    String fullPathForDatabase(SecurityOrigin*, const String& name, bool createIfDoesNotExist = true);

#if !PLATFORM(CHROMIUM)
    bool hasEntryForOrigin(SecurityOrigin*);
    void origins(Vector<RefPtr<SecurityOrigin> >& result);
    bool databaseNamesForOrigin(SecurityOrigin*, Vector<String>& result);
    DatabaseDetails detailsForNameAndOrigin(const String&, SecurityOrigin*);

    unsigned long long usageForOrigin(SecurityOrigin*);
    unsigned long long quotaForOrigin(SecurityOrigin*);

    void setQuota(SecurityOrigin*, unsigned long long);

    void deleteAllDatabases();
    bool deleteOrigin(SecurityOrigin*);
    bool deleteDatabase(SecurityOrigin*, const String& name);

    // From a secondary thread, must be thread safe with its data
    void scheduleNotifyDatabaseChanged(SecurityOrigin*, const String& name);

    void databaseChanged(AbstractDatabase*);

#else // PLATFORM(CHROMIUM)
    void closeDatabasesImmediately(const String& originIdentifier, const String& name);
#endif // PLATFORM(CHROMIUM)

    void interruptAllDatabasesForContext(const ScriptExecutionContext*);

    bool canEstablishDatabase(ScriptExecutionContext*, const String& name, const String& displayName, unsigned long estimatedSize);
    void addOpenDatabase(AbstractDatabase*);
    void removeOpenDatabase(AbstractDatabase*);

    void setDatabaseDetails(SecurityOrigin*, const String& name, const String& displayName, unsigned long estimatedSize);
    unsigned long long getMaxSizeForDatabase(const AbstractDatabase*);

private:
    DatabaseManager();
    ~DatabaseManager() { }

    AbstractDatabaseServer* m_server;
    DatabaseManagerClient* m_client;
    bool m_databaseIsAvailable;
};

} // namespace WebCore

#endif // ENABLE(SQL_DATABASE)

#endif // DatabaseManager_h
