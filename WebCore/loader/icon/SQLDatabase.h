/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef SQLDatabase_h
#define SQLDatabase_h

#include "PlatformString.h"
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

#ifndef NDEBUG
#include <pthread.h>
#endif

#if COMPILER(MSVC)
#pragma warning(disable: 4800)
#endif

typedef struct sqlite3 sqlite3;

namespace WebCore {

class SQLStatement;
class SQLTransaction;

extern const int SQLResultError;
extern const int SQLResultDone;
extern const int SQLResultOk;
extern const int SQLResultRow;

class SQLDatabase : public Noncopyable
{
friend class SQLTransaction;
public:
    SQLDatabase();
    ~SQLDatabase() { close(); }

    bool open(const String& filename);
    bool isOpen() const { return m_db; }
    String path() const { return m_path; }
    void close();

    bool executeCommand(const String&);
    bool returnsAtLeastOneResult(const String&);
    
    bool tableExists(const String&);
    void clearAllTables();
    void runVacuumCommand();
    
    bool transactionInProgress() const { return m_transactionInProgress; }
    
    int64_t lastInsertRowID();
    int lastChanges();

    void setBusyTimeout(int ms);
    void setBusyHandler(int(*)(void*, int));
    
    // TODO - add pragma and sqlite_master accessors here
    void setFullsync(bool);
    
    // The SQLite SYNCHRONOUS pragma can be either FULL, NORMAL, or OFF
    // FULL - Any writing calls to the DB block until the data is actually on the disk surface
    // NORMAL - SQLite pauses at some critical moments when writing, but much less than FULL
    // OFF - Calls return immediately after the data has been passed to disk
    enum SynchronousPragma {
        SyncOff = 0, SyncNormal = 1, SyncFull = 2
    };
    void setSynchronous(SynchronousPragma);
    
    int lastError();
    const char* lastErrorMsg();
    
    sqlite3* sqlite3Handle() const {
        ASSERT(pthread_equal(m_openingThread, pthread_self()));
        return m_db;
    }
    
private:
    String   m_path;
    sqlite3* m_db;
    int m_lastError;
    
    bool m_transactionInProgress;
    
#ifndef NDEBUG
    pthread_t m_openingThread;
#endif
    
}; // class SQLDatabase

inline String escapeSQLString(const String& s)
{
    String es = s;
    es.replace('\'', "''");
    return es;
}

} // namespace WebCore



#endif
