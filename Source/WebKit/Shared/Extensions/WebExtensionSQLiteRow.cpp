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
#include "WebExtensionSQLiteRow.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionSQLiteDatabase.h"
#include "WebExtensionSQLiteStatement.h"
#include <sqlite3.h>
#include <wtf/TZoneMallocInlines.h>
#include <WebCore/SQLiteExtras.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionSQLiteRow);
WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionSQLiteRowEnumerator);

WebExtensionSQLiteRow::WebExtensionSQLiteRow(Ref<WebExtensionSQLiteStatement> statement)
    : m_statement(statement)
    , m_handle(statement->handle())
{
    m_statement->database()->assertQueue();
}

String WebExtensionSQLiteRow::getString(int index)
{
    m_statement->database()->assertQueue();
    if (isNullAtIndex(index))
        return emptyString();

    return WebCore::sqliteColumnText(m_handle, index);
}

int WebExtensionSQLiteRow::getInt(int index)
{
    m_statement->database()->assertQueue();
    return sqlite3_column_int(m_handle, index);
}

int64_t WebExtensionSQLiteRow::getInt64(int index)
{
    m_statement->database()->assertQueue();
    return sqlite3_column_int64(m_handle, index);
}

double WebExtensionSQLiteRow::getDouble(int index)
{
    m_statement->database()->assertQueue();
    return sqlite3_column_double(m_handle, index);
}

bool WebExtensionSQLiteRow::getBool(int index)
{
    return !!getInt(index);
}

RefPtr<API::Data> WebExtensionSQLiteRow::getData(int index)
{
    m_statement->database()->assertQueue();
    if (isNullAtIndex(index))
        return nullptr;

    auto blob = WebCore::sqliteColumnBlob(m_handle, index);
    if (blob.empty())
        return nullptr;

    return API::Data::create(blob);
}

bool WebExtensionSQLiteRow::isNullAtIndex(int index)
{
    m_statement->database()->assertQueue();
    return sqlite3_column_type(m_handle, index) == SQLITE_NULL;
}

WebExtensionSQLiteRowEnumerator::WebExtensionSQLiteRowEnumerator(Ref<WebExtensionSQLiteStatement> statement)
    : m_statement(statement)
{
    m_statement->database()->assertQueue();
}

RefPtr<WebExtensionSQLiteRow> WebExtensionSQLiteRowEnumerator::next()
{
    m_statement->database()->assertQueue();

    switch (sqlite3_step(m_statement->handle())) {
    case SQLITE_ROW:
        if (!m_row)
            m_row = WebExtensionSQLiteRow::create(m_statement);
        return m_row;

    default:
        return nullptr;
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
