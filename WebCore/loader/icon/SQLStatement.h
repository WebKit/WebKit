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

#ifndef SQLStatement_h
#define SQLStatement_h

#include "PlatformString.h"
#include "SQLDatabase.h"
#include <wtf/Noncopyable.h>

typedef struct sqlite3_stmt sqlite3_stmt;

namespace WebCore {

class SQLDatabase;

class SQLStatement : public Noncopyable
{
public:
    SQLStatement(SQLDatabase& db, const String&);
    ~SQLStatement();
    
    int prepare();
    bool isPrepared() { return m_statement; }
    int bindBlob(int index, const void* blob, int size, bool copy = true);
    int bindText(int index, const char* text, bool copy = true);
    int bindText16(int index, const String& text, bool copy = true);
    int bindInt64(int index, int64_t integer);
    int bindNull(int index);

    int step();
    int finalize();
    int reset();
    
    int prepareAndStep() { prepare();  return step(); }
    
    // prepares, steps, and finalizes the query.
    // returns true if all 3 steps succeed with step() returning SQLITE_DONE
    // returns false otherwise  
    bool executeCommand();
    
    // prepares, steps, and finalizes.  
    // returns true is step() returns SQLITE_ROW
    // returns false otherwise
    bool returnsAtLeastOneResult();

    bool isExpired();

    // Returns -1 on last-step failing.  Otherwise, returns number of rows
    // returned in the last step()
    int columnCount();
    
    String getColumnName(int col);
    String getColumnName16(int col);
    String getColumnText(int col);
    String getColumnText16(int col);
    double getColumnDouble(int col);
    int getColumnInt(int col);
    int64_t getColumnInt64(int col);
    const void* getColumnBlob(int col, int& size);
    void getColumnBlobAsVector(int col, Vector<char>&);

    bool returnTextResults(int col, Vector<String>&);
    bool returnTextResults16(int col, Vector<String>&);
    bool returnIntResults(int col, Vector<int>&);
    bool returnInt64Results(int col, Vector<int64_t>&);
    bool returnDoubleResults(int col, Vector<double>&);

    int lastError() { return m_database.lastError(); }
    const char* lastErrorMsg() { return m_database.lastErrorMsg(); }
    
    SQLDatabase* database() { return &m_database; }
private:
    SQLDatabase& m_database;
    String      m_query;

    sqlite3_stmt* m_statement;
};

} // namespace WebCore

#endif // SQLStatement_h
