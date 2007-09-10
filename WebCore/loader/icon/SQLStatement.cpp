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

#include "config.h"
#include "SQLStatement.h"

#include "Logging.h"
#include <sqlite3.h>
#include <wtf/Assertions.h>

namespace WebCore {

SQLStatement::SQLStatement(SQLDatabase& db, const String& sql)
    : m_database(db)
    , m_query(sql)
    , m_statement(0)
{
    m_query.append(UChar(0));
}

SQLStatement::~SQLStatement()
{
    finalize();
}

int SQLStatement::prepare()
{    
    const void* tail;
    LOG(SQLDatabase, "SQL - prepare - %s", m_query.ascii().data());
    if (sqlite3_prepare16(m_database.sqlite3Handle(), m_query.characters(), -1, &m_statement, &tail) != SQLITE_OK) {
        LOG(SQLDatabase, "sqlite3_prepare16 failed (%i)\n%s\n%s", lastError(), m_query.ascii().data(), sqlite3_errmsg(m_database.sqlite3Handle()));
        m_statement = 0;
    }
    return lastError();
}
    
int SQLStatement::step()
{
    if (!isPrepared())
        return SQLITE_ERROR;
    LOG(SQLDatabase, "SQL - step - %s", m_query.ascii().data());
    int error = sqlite3_step(m_statement);
    if (error != SQLITE_DONE && error != SQLITE_ROW) {
        LOG(SQLDatabase, "sqlite3_step failed (%i)\nQuery - %s\nError - %s", 
            error, m_query.ascii().data(), sqlite3_errmsg(m_database.sqlite3Handle()));
    }
    return error;
}
    
int SQLStatement::finalize()
{
    if (m_statement) {
        LOG(SQLDatabase, "SQL - finalize - %s", m_query.ascii().data());
        int result = sqlite3_finalize(m_statement);
        m_statement = 0;
        return result;
    }
    return lastError();
}

int SQLStatement::reset() 
{
    if (m_statement) {
        LOG(SQLDatabase, "SQL - reset - %s", m_query.ascii().data());
        return sqlite3_reset(m_statement);
    }
    return SQLITE_ERROR;
}

bool SQLStatement::executeCommand()
{
    if (!isPrepared())
        if (prepare() != SQLITE_OK)
            return false;
    if (step() != SQLITE_DONE) {
        finalize();
        return false;
    }
    finalize();
    return true;
}

bool SQLStatement::returnsAtLeastOneResult()
{
    if (!isPrepared())
        if (prepare() != SQLITE_OK)
            return false;
    if (step() != SQLITE_ROW) {
        finalize();
        return false;
    }
    finalize();
    return true;

}

int SQLStatement::bindBlob(int index, const void* blob, int size, bool copy)
{
    ASSERT(blob);
    ASSERT(size > -1);
    if (copy)
        sqlite3_bind_blob(m_statement, index, blob, size, SQLITE_TRANSIENT);
    else
        sqlite3_bind_blob(m_statement, index, blob, size, SQLITE_STATIC);
    return lastError();
}

int SQLStatement::bindText(int index, const char* text, bool copy)
{
    ASSERT(text);
    if (copy)
        sqlite3_bind_text(m_statement, index, text, strlen(text), SQLITE_TRANSIENT);
    else
        sqlite3_bind_text(m_statement, index, text, strlen(text), SQLITE_STATIC);
    return lastError();
}

int SQLStatement::bindText16(int index, const String& text, bool copy)
{
    if (copy)
        sqlite3_bind_text16(m_statement, index, text.characters(), sizeof(UChar) * text.length(), SQLITE_TRANSIENT);
    else
        sqlite3_bind_text16(m_statement, index, text.characters(), sizeof(UChar) * text.length(), SQLITE_STATIC);
    return lastError();
}


int SQLStatement::bindInt64(int index, int64_t integer)
{
    return sqlite3_bind_int64(m_statement, index, integer);
}

int SQLStatement::bindNull(int index)
{
    return sqlite3_bind_null(m_statement, index);
}

int SQLStatement::columnCount()
{
    if (m_statement)
        return sqlite3_data_count(m_statement);
    return 0;
}

String SQLStatement::getColumnName(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return String();
    if (columnCount() <= col)
        return String();
        
    return String(sqlite3_column_name(m_statement, col));
}

String SQLStatement::getColumnName16(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return String();
    if (columnCount() <= col)
        return String();
    return String((const UChar*)sqlite3_column_name16(m_statement, col));
}
    
String SQLStatement::getColumnText(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return String();
    if (columnCount() <= col)
        return String();
    return String((const char*)sqlite3_column_text(m_statement, col));
}

String SQLStatement::getColumnText16(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return String();
    if (columnCount() <= col)
        return String();
    return String((const UChar*)sqlite3_column_text16(m_statement, col));
}
    
double SQLStatement::getColumnDouble(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return 0.0;
    if (columnCount() <= col)
        return 0.0;
    return sqlite3_column_double(m_statement, col);
}

int SQLStatement::getColumnInt(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return 0;
    if (columnCount() <= col)
        return 0;
    return sqlite3_column_int(m_statement, col);
}

int64_t SQLStatement::getColumnInt64(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return 0;
    if (columnCount() <= col)
        return 0;
    return sqlite3_column_int64(m_statement, col);
}
    
void SQLStatement::getColumnBlobAsVector(int col, Vector<char>& result)
{
    if (!m_statement && prepareAndStep() != SQLITE_ROW) {
        result.clear();
        return;
    }

    if (columnCount() <= col) {
        result.clear();
        return;
    }

 
    const void* blob = sqlite3_column_blob(m_statement, col);
    if (!blob) {
        result.clear();
        return;
    }
        
    int size = sqlite3_column_bytes(m_statement, col);
    result.resize((size_t)size);
    for (int i = 0; i < size; ++i)
        result[i] = ((const unsigned char*)blob)[i];
}

const void* SQLStatement::getColumnBlob(int col, int& size)
{
    if (finalize() != SQLITE_OK)
        LOG(SQLDatabase, "Finalize failed");
    if (prepare() != SQLITE_OK)
        LOG(SQLDatabase, "Prepare failed");
    if (step() != SQLITE_ROW)
        {LOG(SQLDatabase, "Step wasn't a row");size=0;return 0;}

    if (columnCount() <= col) {
        size = 0;
        return 0;
    }
        
    const void* blob = sqlite3_column_blob(m_statement, col);
    if (blob) {
        size = sqlite3_column_bytes(m_statement, col);
        return blob;
    } 
    size = 0;
    return 0;
}

bool SQLStatement::returnTextResults(int col, Vector<String>& v)
{
    bool result = true;
    if (m_statement)
        finalize();
    prepare();
        
    v.clear();
    while (step() == SQLITE_ROW) {
        v.append(getColumnText(col));
    }
    if (lastError() != SQLITE_DONE) {
        result = false;
        LOG(SQLDatabase, "Error reading results from database query %s", m_query.ascii().data());
    }
    finalize();
    return result;
}

bool SQLStatement::returnTextResults16(int col, Vector<String>& v)
{
    bool result = true;
    if (m_statement)
        finalize();
    prepare();
        
    v.clear();
    while (step() == SQLITE_ROW) {
        v.append(getColumnText16(col));
    }
    if (lastError() != SQLITE_DONE) {
        result = false;
        LOG(SQLDatabase, "Error reading results from database query %s", m_query.ascii().data());
    }
    finalize();
    return result;
}

bool SQLStatement::returnIntResults(int col, Vector<int>& v)
{
    bool result = true;
    if (m_statement)
        finalize();
    prepare();
        
    v.clear();
    while (step() == SQLITE_ROW) {
        v.append(getColumnInt(col));
    }
    if (lastError() != SQLITE_DONE) {
        result = false;
        LOG(SQLDatabase, "Error reading results from database query %s", m_query.ascii().data());
    }
    finalize();
    return result;
}

bool SQLStatement::returnInt64Results(int col, Vector<int64_t>& v)
{
    bool result = true;
    if (m_statement)
        finalize();
    prepare();
        
    v.clear();
    while (step() == SQLITE_ROW) {
        v.append(getColumnInt64(col));
    }
    if (lastError() != SQLITE_DONE) {
        result = false;
        LOG(SQLDatabase, "Error reading results from database query %s", m_query.ascii().data());
    }
    finalize();
    return result;
}

bool SQLStatement::returnDoubleResults(int col, Vector<double>& v)
{
    bool result = true;
    if (m_statement)
        finalize();
    prepare();
        
    v.clear();
    while (step() == SQLITE_ROW) {
        v.append(getColumnDouble(col));
    }
    if (lastError() != SQLITE_DONE) {
        result = false;
        LOG(SQLDatabase, "Error reading results from database query %s", m_query.ascii().data());
    }
    finalize();
    return result;
}

bool SQLStatement::isExpired()
{
    return m_statement ? sqlite3_expired(m_statement) : true;
}

} // namespace WebCore
