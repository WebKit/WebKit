/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "IDBKey.h"

#if ENABLE(INDEXED_DATABASE)

#include "SQLiteStatement.h"
#include "SerializedScriptValue.h"

namespace WebCore {

IDBKey::IDBKey()
    : m_type(NullType)
{
}

IDBKey::IDBKey(double number)
    : m_type(NumberType)
    , m_number(number)
{
}

IDBKey::IDBKey(const String& string)
    : m_type(StringType)
    , m_string(string.crossThreadString())
{
}

IDBKey::~IDBKey()
{
}

PassRefPtr<IDBKey> IDBKey::fromQuery(SQLiteStatement& query, int baseColumn)
{
    if (!query.isColumnNull(baseColumn))
        return IDBKey::create(query.getColumnText(baseColumn));

    if (!query.isColumnNull(baseColumn + 1)) {
        ASSERT_NOT_REACHED(); // FIXME: Implement date.
        return IDBKey::create();
    }

    if (!query.isColumnNull(baseColumn + 2))
        return IDBKey::create(query.getColumnDouble(baseColumn + 2));

    return IDBKey::create(); // Null.
}

bool IDBKey::isEqual(IDBKey* other)
{
    if (!other || other->m_type != m_type)
        return false;

    switch (m_type) {
    case StringType:
        return other->m_string == m_string;
    // FIXME: Implement dates.
    case NumberType:
        return other->m_number == m_number;
    case NullType:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

String IDBKey::whereSyntax(String qualifiedTableName) const
{
    switch (m_type) {
    case IDBKey::StringType:
        return qualifiedTableName + "keyString = ?  AND  " + qualifiedTableName + "keyDate IS NULL  AND  " + qualifiedTableName + "keyNumber IS NULL  ";
    case IDBKey::NumberType:
        return qualifiedTableName + "keyString IS NULL  AND  " + qualifiedTableName + "keyDate IS NULL  AND  " + qualifiedTableName + "keyNumber = ?  ";
    // FIXME: Implement date.
    case IDBKey::NullType:
        return qualifiedTableName + "keyString IS NULL  AND  " + qualifiedTableName + "keyDate IS NULL  AND  " + qualifiedTableName + "keyNumber IS NULL  ";
    }

    ASSERT_NOT_REACHED();
    return "";
}

String IDBKey::lowerCursorWhereFragment(String comparisonOperator, String qualifiedTableName)
{
    switch (m_type) {
    case StringType:
        return "? " + comparisonOperator + " " + qualifiedTableName + "keyString  AND  ";
    // FIXME: Implement date.
    case NumberType:
        return "(? " + comparisonOperator + " " + qualifiedTableName + "keyNumber  OR  NOT " + qualifiedTableName + "keyString IS NULL  OR  NOT " + qualifiedTableName + "keyDate IS NULL)  AND  ";
    case NullType:
        if (comparisonOperator == "<")
            return "NOT(" + qualifiedTableName + "keyString IS NULL  AND  " + qualifiedTableName + "keyDate IS NULL  AND  " + qualifiedTableName + "keyNumber IS NULL)  AND  ";
        return ""; // If it's =, the upper bound half will do the constraining. If it's <=, then that's a no-op.
    }
    ASSERT_NOT_REACHED();
    return "";
}

String IDBKey::upperCursorWhereFragment(String comparisonOperator, String qualifiedTableName)
{
    switch (m_type) {
    case StringType:
        return "(" + qualifiedTableName + "keyString " + comparisonOperator + " ?  OR  " + qualifiedTableName + "keyString IS NULL)  AND  ";
    // FIXME: Implement date.
    case NumberType:
        return "(" + qualifiedTableName + "keyNumber " + comparisonOperator + " ? OR " + qualifiedTableName + "keyNumber IS NULL)  AND  " + qualifiedTableName + "keyString IS NULL  AND  " + qualifiedTableName + "keyDate IS NULL  AND  ";
    case NullType:
        if (comparisonOperator == "<")
            return "0 != 0  AND  ";
        return qualifiedTableName + "keyString IS NULL  AND  " + qualifiedTableName + "keyDate IS NULL  AND  " + qualifiedTableName + "keyNumber IS NULL  AND  ";
    }
    ASSERT_NOT_REACHED();
    return "";
}

// Returns the number of items bound.
int IDBKey::bind(SQLiteStatement& query, int column) const
{
    switch (m_type) {
    case IDBKey::StringType:
        query.bindText(column, m_string);
        return 1;
    case IDBKey::NumberType:
        query.bindDouble(column, m_number);
        return 1;
    case IDBKey::NullType:
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

void IDBKey::bindWithNulls(SQLiteStatement& query, int baseColumn) const
{
    switch (m_type) {
    case IDBKey::StringType:
        query.bindText(baseColumn + 0, m_string);
        query.bindNull(baseColumn + 1);
        query.bindNull(baseColumn + 2);
        break;
    case IDBKey::NumberType:
        query.bindNull(baseColumn + 0);
        query.bindNull(baseColumn + 1);
        query.bindDouble(baseColumn + 2, m_number);
        break;
    case IDBKey::NullType:
        query.bindNull(baseColumn + 0);
        query.bindNull(baseColumn + 1);
        query.bindNull(baseColumn + 2);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

} // namespace WebCore

#endif
