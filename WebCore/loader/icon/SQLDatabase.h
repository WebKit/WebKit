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

#ifndef SQLDatabase_H
#define SQLDatabase_H

#include "PlatformString.h"
#include <sqlite3.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class SQLStatement;

class SQLDatabase : public Noncopyable
{
    friend class SQLStatement;
public:
    SQLDatabase();

    bool open(const String& filename);
    bool isOpen() { return m_db; }
    String getPath(){ return m_path; }
    void close();

    bool executeCommand(const String&);
    bool returnsAtLeastOneResult(const String&);
    
    bool tableExists(const String&);
    
    int64_t lastInsertRowID();

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
    
    int lastError() { return m_db ? sqlite3_errcode(m_db) : SQLITE_ERROR; }
    const char* lastErrorMsg() { return sqlite3_errmsg(m_db); }
private:
    String   m_path;
    sqlite3* m_db;
    int m_lastError;
    
}; // class SQLDatabase

inline String escapeSQLString(const String& s)
{
    String es = s;
    es.replace('\'', "''");
    return es;
}

} // namespace WebCore



#endif
