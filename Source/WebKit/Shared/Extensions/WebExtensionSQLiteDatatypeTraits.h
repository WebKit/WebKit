/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "APIData.h"
#include "WebExtensionSQLiteDatabase.h"
#include <WebCore/SQLiteExtras.h>
#include <sqlite3.h>
#include <tuple>

namespace WebKit {

template<typename Type>
class WebExtensionSQLiteDatatypeTraits {
public:
    static Type fetch(sqlite3_stmt* statement, int index);
    static int bind(sqlite3_stmt* statement, int index, const Type&);
};

template<>
class WebExtensionSQLiteDatatypeTraits<int> {
public:
    static inline int fetch(sqlite3_stmt* statement, int index)
    {
        return sqlite3_column_int(statement, index);
    }

    static inline int bind(sqlite3_stmt* statement, int index, int value)
    {
        return sqlite3_bind_int(statement, index, value);
    }
};

template<>
class WebExtensionSQLiteDatatypeTraits<int64_t> {
public:
    static inline int64_t fetch(sqlite3_stmt* statement, int index)
    {
        return sqlite3_column_int64(statement, index);
    }

    static inline int bind(sqlite3_stmt* statement, int index, int64_t value)
    {
        return sqlite3_bind_int64(statement, index, value);
    }
};

template<>
class WebExtensionSQLiteDatatypeTraits<double> {
public:
    static inline double fetch(sqlite3_stmt* statement, int index)
    {
        return sqlite3_column_double(statement, index);
    }

    static inline int bind(sqlite3_stmt* statement, int index, double value)
    {
        return sqlite3_bind_double(statement, index, value);
    }
};

template<>
class WebExtensionSQLiteDatatypeTraits<String> {
public:
    static String fetch(sqlite3_stmt* statement, int index)
    {
        if (sqlite3_column_type(statement, index) == SQLITE_NULL)
            return emptyString();

        return WebCore::sqliteColumnText(statement, index);
    }

    static inline int bind(sqlite3_stmt* statement, int index, const String& value)
    {
        if (!value)
            return sqlite3_bind_null(statement, index);

        return WebCore::sqliteBindText(statement, index, value.utf8());
    }
};

template<>
class WebExtensionSQLiteDatatypeTraits<RefPtr<API::Data>> {
public:
    static RefPtr<API::Data> fetch(sqlite3_stmt* statement, int index)
    {
        if (sqlite3_column_type(statement, index) == SQLITE_NULL)
            return nullptr;

        auto blob = WebCore::sqliteColumnBlob(statement, index);
        if (!blob.data())
            return nullptr;

        return API::Data::create(blob);
    }

    static inline int bind(sqlite3_stmt* statement, int index, RefPtr<API::Data> value)
    {
        if (!value)
            return sqlite3_bind_null(statement, index);

        return WebCore::sqliteBindBlob(statement, index, value->span());
    }
};

template<>
class WebExtensionSQLiteDatatypeTraits<std::nullptr_t> {
public:
    static inline std::nullptr_t fetch(sqlite3_stmt* statement, int index)
    {
        return std::nullptr_t();
    }

    static inline int bind(sqlite3_stmt* statement, int index, std::nullptr_t)
    {
        return sqlite3_bind_null(statement, index);
    }
};

template<>
class WebExtensionSQLiteDatatypeTraits<decltype(std::ignore)> {
public:
    static inline decltype(std::ignore) fetch(sqlite3_stmt* statement, int index)
    {
        return std::ignore;
    }

    static inline int bind(sqlite3_stmt* statement, int index, decltype(std::ignore))
    {
        return SQLITE_OK;
    }
};

} // namespace WebKit
