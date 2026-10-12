/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include <span>
#include <sqlite3.h>
#include <wtf/Platform.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/UTF8CStringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

inline int sqliteBindBlob(sqlite3_stmt* statement, int index, std::span<const uint8_t> data, void(*destructor)(void*) = SQLITE_TRANSIENT)
{
    // sqlite3_bind_blob64() symbol is undefined on the PlayStation port.
#if PLATFORM(PLAYSTATION)
    return sqlite3_bind_blob(statement, index, data.data(), data.size(), destructor); // NOLINT
#else
    return sqlite3_bind_blob64(statement, index, data.data(), data.size(), destructor); // NOLINT
#endif
}

inline int sqliteBindText(sqlite3_stmt* statement, int index, std::span<const char> text, void(*destructor)(void*) = SQLITE_TRANSIENT)
{
    return sqlite3_bind_text(statement, index, text.data(), text.size(), destructor); // NOLINT
}

inline int sqliteBindText(sqlite3_stmt* statement, int index, const UTF8CString& text, void(*destructor)(void*) = SQLITE_TRANSIENT)
{
    return sqliteBindText(statement, index, byteCast<char>(text.span()), destructor);
}

inline String sqliteColumnName(sqlite3_stmt* statement, int index)
{
    return String::fromUTF8(sqlite3_column_name(statement, index)); // NOLINT
}

inline String sqliteValueText(sqlite3_value* value)
{
    return String::fromUTF8(unsafeMakeSpan(sqlite3_value_text(value), sqlite3_value_bytes(value))); // NOLINT
}

inline String sqliteColumnText(sqlite3_stmt* statement, int index)
{
    return String::fromUTF8(unsafeMakeSpan(sqlite3_column_text(statement, index), sqlite3_column_bytes(statement, index))); // NOLINT
}

template<typename T = uint8_t>
inline std::span<const T> sqliteColumnBlob(sqlite3_stmt* statement, int index)
{
    auto* blob = static_cast<const T*>(sqlite3_column_blob(statement, index)); // NOLINT
    if (!blob)
        return { };
    auto blobSize = sqlite3_column_bytes(statement, index); // NOLINT
    if (blobSize < 0 || (blobSize % sizeof(T)))
        return { };
    return unsafeMakeSpan(blob, blobSize / sizeof(T));
}

inline int sqliteOpen(UTF8CStringView filename, sqlite3** database, int flags, UTF8CStringView vfs = nullptr)
{
    return sqlite3_open_v2(filename.utf8(), database, flags, vfs.utf8()); // NOLINT
}

// Passing the length including the null terminator lets SQLite avoid copying the query.
inline int sqlitePrepare(sqlite3* database, std::span<const char> queryIncludingNullTerminator, sqlite3_stmt** statement, const char** tail = nullptr)
{
    return sqlite3_prepare_v2(database, queryIncludingNullTerminator.data(), queryIncludingNullTerminator.size(), statement, tail); // NOLINT
}

inline int sqlitePrepare(sqlite3* database, UTF8CStringView query, sqlite3_stmt** statement, const char** tail = nullptr)
{
    return sqlitePrepare(database, byteCast<char>(query.spanIncludingNullTerminator()), statement, tail);
}

inline int sqliteCreateCollation(sqlite3* database, UTF8CStringView name, int textRepresentation, void* argument, int(*compare)(void*, int, const void*, int, const void*), void(*destroy)(void*))
{
    return sqlite3_create_collation_v2(database, name.utf8(), textRepresentation, argument, compare, destroy); // NOLINT
}

inline void sqliteResultError(sqlite3_context* context, UTF8CStringView message)
{
    auto characters = byteCast<char>(message.span());
    sqlite3_result_error(context, characters.data(), characters.size()); // NOLINT
}

} // namespace WebCore
