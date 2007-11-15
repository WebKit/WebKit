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
#include "SQLiteStatement.h"

#include "Logging.h"
#include "SQLValue.h"
#include <sqlite3.h>
#include <wtf/Assertions.h>

namespace WebCore {

SQLiteStatement::SQLiteStatement(SQLiteDatabase& db, const String& sql)
    : m_database(db)
    , m_query(sql)
    , m_statement(0)
{
    m_query.append(UChar(0));
}

SQLiteStatement::~SQLiteStatement()
{
    finalize();
}

int SQLiteStatement::prepare()
{    
    const void* tail;
    LOG(SQLDatabase, "SQL - prepare - %s", m_query.ascii().data());
#if SQLITE_VERSION_NUMBER > 3003000 
    if (sqlite3_prepare16_v2(m_database.sqlite3Handle(), m_query.characters(), -1, &m_statement, &tail) != SQLITE_OK) {
#else
    if (sqlite3_prepare16(m_database.sqlite3Handle(), m_query.characters(), -1, &m_statement, &tail) != SQLITE_OK) {
#endif
        LOG(SQLDatabase, "sqlite3_prepare16 failed (%i)\n%s\n%s", lastError(), m_query.ascii().data(), sqlite3_errmsg(m_database.sqlite3Handle()));
        m_statement = 0;
    }
    return lastError();
}
    
int SQLiteStatement::step()
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
    
int SQLiteStatement::finalize()
{
    if (m_statement) {
        LOG(SQLDatabase, "SQL - finalize - %s", m_query.ascii().data());
        int result = sqlite3_finalize(m_statement);
        m_statement = 0;
        return result;
    }
    return lastError();
}

int SQLiteStatement::reset() 
{
    if (m_statement) {
        LOG(SQLDatabase, "SQL - reset - %s", m_query.ascii().data());
        return sqlite3_reset(m_statement);
    }
    return SQLITE_ERROR;
}

bool SQLiteStatement::executeCommand()
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

bool SQLiteStatement::returnsAtLeastOneResult()
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

int SQLiteStatement::bindBlob(int index, const void* blob, int size)
{
    ASSERT(blob);
    ASSERT(size > -1);

    sqlite3_bind_blob(m_statement, index, blob, size, SQLITE_TRANSIENT);

    return lastError();
}

int SQLiteStatement::bindText(int index, const String& text)
{
    sqlite3_bind_text16(m_statement, index, text.characters(), sizeof(UChar) * text.length(), SQLITE_TRANSIENT);
    return lastError();
}


int SQLiteStatement::bindInt64(int index, int64_t integer)
{
    return sqlite3_bind_int64(m_statement, index, integer);
}

int SQLiteStatement::bindDouble(int index, double number)
{
    return sqlite3_bind_double(m_statement, index, number);
}

int SQLiteStatement::bindNull(int index)
{
    return sqlite3_bind_null(m_statement, index);
}

int SQLiteStatement::bindValue(int index, const SQLValue& value)
{
    switch (value.type()) {
        case SQLValue::StringValue:
            return bindText(index, value.string());
        case SQLValue::NumberValue:
            return bindDouble(index, value.number());
        case SQLValue::NullValue:
            return bindNull(index);
        default:
            ASSERT_NOT_REACHED();
            // To keep the compiler happy
            return SQLITE_ERROR;
    }
}

unsigned SQLiteStatement::bindParameterCount() const
{
    ASSERT(isPrepared());
    return sqlite3_bind_parameter_count(m_statement);
}

int SQLiteStatement::columnCount()
{
    if (m_statement)
        return sqlite3_data_count(m_statement);
    return 0;
}

String SQLiteStatement::getColumnName(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return String();
    if (columnCount() <= col)
        return String();
        
    return String(sqlite3_column_name(m_statement, col));
}

String SQLiteStatement::getColumnName16(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return String();
    if (columnCount() <= col)
        return String();
    return String((const UChar*)sqlite3_column_name16(m_statement, col));
}
    
String SQLiteStatement::getColumnText(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return String();
    if (columnCount() <= col)
        return String();
    return String((const char*)sqlite3_column_text(m_statement, col));
}

String SQLiteStatement::getColumnText16(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return String();
    if (columnCount() <= col)
        return String();
    return String((const UChar*)sqlite3_column_text16(m_statement, col));
}
    
double SQLiteStatement::getColumnDouble(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return 0.0;
    if (columnCount() <= col)
        return 0.0;
    return sqlite3_column_double(m_statement, col);
}

int SQLiteStatement::getColumnInt(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return 0;
    if (columnCount() <= col)
        return 0;
    return sqlite3_column_int(m_statement, col);
}

int64_t SQLiteStatement::getColumnInt64(int col)
{
    if (!m_statement)
        if (prepareAndStep() != SQLITE_ROW)
            return 0;
    if (columnCount() <= col)
        return 0;
    return sqlite3_column_int64(m_statement, col);
}
    
void SQLiteStatement::getColumnBlobAsVector(int col, Vector<char>& result)
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

const void* SQLiteStatement::getColumnBlob(int col, int& size)
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

bool SQLiteStatement::returnTextResults(int col, Vector<String>& v)
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

bool SQLiteStatement::returnTextResults16(int col, Vector<String>& v)
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

bool SQLiteStatement::returnIntResults(int col, Vector<int>& v)
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

bool SQLiteStatement::returnInt64Results(int col, Vector<int64_t>& v)
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

bool SQLiteStatement::returnDoubleResults(int col, Vector<double>& v)
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

bool SQLiteStatement::isExpired()
{
    return m_statement ? sqlite3_expired(m_statement) : true;
}

} // namespace WebCore
