/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SQLiteStatement.h"

#include "Logging.h"
#include "SQLValue.h"
#include "SQLiteDatabaseTracker.h"
#include <sqlite3.h>
#include <variant>
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringView.h>

// SQLite 3.6.16 makes sqlite3_prepare_v2 automatically retry preparing the statement
// once if the database scheme has changed. We rely on this behavior.
#if SQLITE_VERSION_NUMBER < 3006016
#error SQLite version 3.6.16 or newer is required
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SQLiteStatement);

SQLiteStatement::SQLiteStatement(SQLiteDatabase& db, sqlite3_stmt* statement)
    : m_database(db)
    , m_statement(statement)
{
    ASSERT(statement);
    m_database->incrementStatementCount();
}

SQLiteStatement::SQLiteStatement(SQLiteStatement&& other)
    : m_database(other.database())
    , m_statement(std::exchange(other.m_statement, nullptr))
{
    m_database->incrementStatementCount();
}

SQLiteStatement::~SQLiteStatement()
{
    sqlite3_finalize(m_statement);
    m_database->decrementStatementCount();
}

int SQLiteStatement::step()
{
    Locker databaseLock { m_database->databaseMutex() };

    // If we're not within a transaction and we call sqlite3_step(), SQLite will implicitly create a transaction for us.
    // In such case, we should bump our transaction count to reflect that.
    std::optional<SQLiteTransactionInProgressAutoCounter> transactionCounter;
    if (!m_database->transactionInProgress() && !isReadOnly())
        transactionCounter.emplace();

    int error = sqlite3_step(m_statement);
    if (error != SQLITE_DONE && error != SQLITE_ROW)
        LOG(SQLDatabase, "sqlite3_step failed (%i)\nError - %s", error, sqlite3_errmsg(m_database->sqlite3Handle()));

    return error;
}

int SQLiteStatement::reset() 
{
    int status = sqlite3_reset(m_statement);
    sqlite3_clear_bindings(m_statement);
    return status;
}

bool SQLiteStatement::executeCommand()
{
    return step() == SQLITE_DONE;
}

int SQLiteStatement::bindBlob(int index, std::span<const uint8_t> blob)
{
    ASSERT(index > 0);
    ASSERT(static_cast<unsigned>(index) <= bindParameterCount());
    ASSERT(blob.data() || !blob.size());

    return sqlite3_bind_blob(m_statement, index, blob.data(), blob.size(), SQLITE_TRANSIENT);
}

int SQLiteStatement::bindBlob(int index, const String& text)
{
    // String::characters() returns 0 for the empty string, which SQLite
    // treats as a null, so we supply a non-null pointer for that case.
    auto upconvertedCharacters = StringView(text).upconvertedCharacters();
    UChar anyCharacter = 0;
    std::span<const UChar> characters;
    if (text.isEmpty() && !text.isNull())
        characters = unsafeMakeSpan(&anyCharacter, 0);
    else
        characters = upconvertedCharacters.span();

    return bindBlob(index, spanReinterpretCast<const uint8_t>(characters));
}

int SQLiteStatement::bindText(int index, StringView text)
{
    ASSERT(index > 0);
    ASSERT(static_cast<unsigned>(index) <= bindParameterCount());

    // Fast path when the input text is all ASCII.
    if (text.is8Bit() && text.containsOnlyASCII()) {
        auto characters = spanReinterpretCast<const char>(text.span8());
        return sqlite3_bind_text(m_statement, index, characters.empty() ? "" : characters.data(), characters.size(), SQLITE_TRANSIENT);
    }
    auto utf8Text = text.utf8();
    return sqlite3_bind_text(m_statement, index, utf8Text.data(), utf8Text.length(), SQLITE_TRANSIENT);
}

int SQLiteStatement::bindInt(int index, int integer)
{
    ASSERT(index > 0);
    ASSERT(static_cast<unsigned>(index) <= bindParameterCount());

    return sqlite3_bind_int(m_statement, index, integer);
}

int SQLiteStatement::bindInt64(int index, int64_t integer)
{
    ASSERT(index > 0);
    ASSERT(static_cast<unsigned>(index) <= bindParameterCount());

    return sqlite3_bind_int64(m_statement, index, integer);
}

int SQLiteStatement::bindDouble(int index, double number)
{
    ASSERT(index > 0);
    ASSERT(static_cast<unsigned>(index) <= bindParameterCount());

    return sqlite3_bind_double(m_statement, index, number);
}

int SQLiteStatement::bindNull(int index)
{
    ASSERT(index > 0);
    ASSERT(static_cast<unsigned>(index) <= bindParameterCount());

    return sqlite3_bind_null(m_statement, index);
}

int SQLiteStatement::bindValue(int index, const SQLValue& value)
{
    return WTF::switchOn(value,
        [&] (const std::nullptr_t&) { return bindNull(index); },
        [&] (const String& string) { return bindText(index, string); },
        [&] (double number) { return bindDouble(index, number); }
    );
}

unsigned SQLiteStatement::bindParameterCount() const
{
    return sqlite3_bind_parameter_count(m_statement);
}

int SQLiteStatement::columnCount()
{
    return sqlite3_data_count(m_statement);
}

bool SQLiteStatement::isColumnDeclaredAsBlob(int col)
{
    ASSERT(col >= 0);
    return equalLettersIgnoringASCIICase(StringView::fromLatin1(sqlite3_column_decltype(m_statement, col)), "blob"_s);
}

String SQLiteStatement::columnName(int col)
{
    ASSERT(col >= 0);
    if (!hasStartedStepping() && step() != SQLITE_ROW)
        return String();
    if (columnCount() <= col)
        return String();
    return String::fromUTF8(sqlite3_column_name(m_statement, col));
}

SQLValue SQLiteStatement::columnValue(int col)
{
    ASSERT(col >= 0);
    if (!hasStartedStepping() && step() != SQLITE_ROW)
        return nullptr;
    if (columnCount() <= col)
        return nullptr;

    // SQLite is typed per value. optional column types are
    // "(mostly) ignored"
    sqlite3_value* value = sqlite3_column_value(m_statement, col);
    switch (sqlite3_value_type(value)) {
    case SQLITE_INTEGER: // SQLValue and JS don't represent integers, so use FLOAT -case
    case SQLITE_FLOAT:
        return sqlite3_value_double(value);
    case SQLITE_BLOB: // SQLValue and JS don't represent blobs, so use TEXT -case
    case SQLITE_TEXT:
        return String::fromUTF8(unsafeMakeSpan(sqlite3_value_text(value), sqlite3_value_bytes(value)));
    case SQLITE_NULL:
        return nullptr;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

String SQLiteStatement::columnText(int col)
{
    ASSERT(col >= 0);
    if (!hasStartedStepping() && step() != SQLITE_ROW)
        return String();
    if (columnCount() <= col)
        return String();
    return String::fromUTF8(unsafeMakeSpan(sqlite3_column_text(m_statement, col), sqlite3_column_bytes(m_statement, col)));
}
    
double SQLiteStatement::columnDouble(int col)
{
    ASSERT(col >= 0);
    if (!hasStartedStepping() && step() != SQLITE_ROW)
        return 0.0;
    if (columnCount() <= col)
        return 0.0;
    return sqlite3_column_double(m_statement, col);
}

int SQLiteStatement::columnInt(int col)
{
    ASSERT(col >= 0);
    if (!hasStartedStepping() && step() != SQLITE_ROW)
        return 0;
    if (columnCount() <= col)
        return 0;
    return sqlite3_column_int(m_statement, col);
}

int64_t SQLiteStatement::columnInt64(int col)
{
    ASSERT(col >= 0);
    if (!hasStartedStepping() && step() != SQLITE_ROW)
        return 0;
    if (columnCount() <= col)
        return 0;
    return sqlite3_column_int64(m_statement, col);
}

String SQLiteStatement::columnBlobAsString(int col)
{
    ASSERT(col >= 0);

    if (!hasStartedStepping() && step() != SQLITE_ROW)
        return String();

    if (columnCount() <= col)
        return String();

    auto* blob = static_cast<const UChar*>(sqlite3_column_blob(m_statement, col));
    if (!blob)
        return emptyString();

    int size = sqlite3_column_bytes(m_statement, col);
    if (size < 0)
        return String();

    ASSERT(!(size % sizeof(UChar)));
    return StringImpl::create8BitIfPossible(unsafeMakeSpan(blob, size / sizeof(UChar)));
}

Vector<uint8_t> SQLiteStatement::columnBlob(int col)
{
    return { columnBlobAsSpan(col) };
}

std::span<const uint8_t> SQLiteStatement::columnBlobAsSpan(int col)
{
    ASSERT(col >= 0);

    if (!hasStartedStepping() && step() != SQLITE_ROW)
        return { };

    if (columnCount() <= col)
        return { };

    auto* blob = static_cast<const uint8_t*>(sqlite3_column_blob(m_statement, col));
    if (!blob)
        return { };

    int blobSize = sqlite3_column_bytes(m_statement, col);
    if (blobSize <= 0)
        return { };

    return unsafeMakeSpan(blob, blobSize);
}

bool SQLiteStatement::hasStartedStepping()
{
    return sqlite3_stmt_busy(m_statement);
}

bool SQLiteStatement::isReadOnly()
{
    return sqlite3_stmt_readonly(m_statement);
}

} // namespace WebCore
