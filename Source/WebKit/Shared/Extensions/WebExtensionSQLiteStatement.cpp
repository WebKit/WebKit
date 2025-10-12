/*
 * Copyright (C) 2024 Igalia, S.L. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebExtensionSQLiteStatement.h"

#include "Logging.h"
#include "WebExtensionSQLiteDatabase.h"
#include "WebExtensionSQLiteHelpers.h"
#include "WebExtensionSQLiteRow.h"
#include <WebCore/SQLiteExtras.h>
#include <sqlite3.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionSQLiteStatement);

WebExtensionSQLiteStatement::WebExtensionSQLiteStatement(Ref<WebExtensionSQLiteDatabase> database, const String& query, RefPtr<API::Error>& outError)
    : m_db(database)
{
    Ref db = m_db;

    ASSERT(db->sqlite3Handle());

    db->assertQueue();

    int result = sqlite3_prepare_v2(db->sqlite3Handle(), query.utf8().data(), -1, &m_handle, 0);
    if (result != SQLITE_OK) {
        db->reportErrorWithCode(result, query, outError);
        return;
    }
}

WebExtensionSQLiteStatement::~WebExtensionSQLiteStatement()
{
    sqlite3_stmt* handle = m_handle;
    if (!handle)
        return;

    database()->queue().dispatch([database = Ref { database() }, handle = WTFMove(handle)]() mutable {
        // The database might have closed already;
        if (!database->sqlite3Handle())
            return;

        sqlite3_finalize(handle);
    });
}

int WebExtensionSQLiteStatement::execute()
{
    database()->assertQueue();
    ASSERT(isValid());

    int resultCode = sqlite3_step(m_handle);
    if (!SQLiteIsExecutionError(resultCode))
        return resultCode;

    return resultCode;
}

bool WebExtensionSQLiteStatement::execute(RefPtr<API::Error>& outError)
{
    Ref database = m_db;

    database->assertQueue();
    ASSERT(isValid());

    int resultCode = sqlite3_step(m_handle);
    if (!SQLiteIsExecutionError(resultCode))
        return true;

    database->reportErrorWithCode(resultCode, m_handle, outError);
    return false;
}

Ref<WebExtensionSQLiteRowEnumerator> WebExtensionSQLiteStatement::fetch()
{
    m_db->assertQueue();
    ASSERT(isValid());

    Ref<WebExtensionSQLiteStatement> protectedThis(*this);
    return WebExtensionSQLiteRowEnumerator::create(*this);
}

bool WebExtensionSQLiteStatement::fetchWithEnumerationCallback(Function<void(RefPtr<WebExtensionSQLiteRow>, bool)>& callback, RefPtr<API::Error>& outError)
{
    m_db->assertQueue();
    ASSERT(isValid());

    RefPtr<WebExtensionSQLiteRow> row;
    Ref<WebExtensionSQLiteStatement> protectedThis(*this);

    int result = SQLITE_OK;
    bool stop = false;
    while (!stop) {
        result = sqlite3_step(m_handle);
        if (result != SQLITE_ROW)
            break;

        if (!row)
            row = WebExtensionSQLiteRow::create(*this);

        callback(row, stop);
    }

    if (result == SQLITE_DONE)
        return true;

    return false;
}

void WebExtensionSQLiteStatement::reset()
{
    m_db->assertQueue();
    ASSERT(isValid());

    int result = sqlite3_reset(m_handle);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not reset statement: %s (%d)", m_db->m_lastErrorMessage.data(), result);
}

void WebExtensionSQLiteStatement::invalidate()
{
    m_db->assertQueue();
    ASSERT(isValid());

    int result = sqlite3_finalize(m_handle);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not finalize statement: %s (%d)", m_db->m_lastErrorMessage.data(), (int)result);
    m_handle = nullptr;
}

void WebExtensionSQLiteStatement::bind(const String& string, int parameterIndex)
{
    m_db->assertQueue();
    ASSERT(isValid());
    ASSERT_ARG(parameterIndex, parameterIndex > 0);

    int result = WebCore::sqliteBindText(m_handle, parameterIndex, string.utf8());
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind string: %s (%d)", m_db->m_lastErrorMessage.data(), (int)result);
}

void WebExtensionSQLiteStatement::bind(const int& n, int parameterIndex)
{
    m_db->assertQueue();
    ASSERT(isValid());
    ASSERT_ARG(parameterIndex, parameterIndex > 0);

    int result = sqlite3_bind_int(m_handle, parameterIndex, n);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind int: %s (%d)", m_db->m_lastErrorMessage.data(), (int)result);
}

void WebExtensionSQLiteStatement::bind(const int64_t& n, int parameterIndex)
{
    m_db->assertQueue();
    ASSERT(isValid());
    ASSERT_ARG(parameterIndex, parameterIndex > 0);

    int result = sqlite3_bind_int64(m_handle, parameterIndex, n);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind integer: %s (%d)", m_db->m_lastErrorMessage.data(), (int)result);
}

void WebExtensionSQLiteStatement::bind(const double& n, int parameterIndex)
{
    m_db->assertQueue();
    ASSERT(isValid());
    ASSERT_ARG(parameterIndex, parameterIndex > 0);

    int result = sqlite3_bind_double(m_handle, parameterIndex, n);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind int: %s (%d)", m_db->m_lastErrorMessage.data(), (int)result);
}

void WebExtensionSQLiteStatement::bind(const RefPtr<API::Data>& data, int parameterIndex)
{
    m_db->assertQueue();
    ASSERT(isValid());
    ASSERT_ARG(parameterIndex, parameterIndex > 0);

    int result = WebCore::sqliteBindBlob(m_handle, parameterIndex, data->span());
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind blob: %s (%d)", m_db->m_lastErrorMessage.data(), (int)result);
}

void WebExtensionSQLiteStatement::bind(int parameterIndex)
{
    m_db->assertQueue();
    ASSERT(isValid());
    ASSERT_ARG(parameterIndex, parameterIndex > 0);

    int result = sqlite3_bind_null(m_handle, parameterIndex);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind null: %s (%d)", m_db->m_lastErrorMessage.data(), (int)result);
}

HashMap<String, int> WebExtensionSQLiteStatement::columnNamesToIndicies()
{
    m_db->assertQueue();
    ASSERT(isValid());

    if (!m_columnNamesToIndicies.isEmpty())
        return m_columnNamesToIndicies;

    int columnCount = sqlite3_column_count(m_handle);
    m_columnNamesToIndicies.reserveInitialCapacity(columnCount);

    for (int i = 0; i < columnCount; ++i)
        m_columnNamesToIndicies.add(WebCore::sqliteColumnName(m_handle, i), i);

    return m_columnNamesToIndicies;
}

Vector<String> WebExtensionSQLiteStatement::columnNames()
{
    m_db->assertQueue();
    ASSERT(isValid());

    if (!m_columnNames.isEmpty())
        return m_columnNames;

    int columnCount = sqlite3_column_count(m_handle);
    m_columnNames.reserveInitialCapacity(columnCount);

    for (int i = 0; i < columnCount; ++i)
        m_columnNames.append(WebCore::sqliteColumnName(m_handle, i));

    return m_columnNames;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
