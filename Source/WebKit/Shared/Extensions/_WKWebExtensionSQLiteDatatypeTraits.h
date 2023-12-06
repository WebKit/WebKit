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

#import "_WKWebExtensionSQLiteDatabase.h"
#import <sqlite3.h>
#import <tuple>

NS_ASSUME_NONNULL_BEGIN

namespace WebKit {

template<typename Type>
class _WKWebExtensionSQLiteDatatypeTraits {
public:
    static Type _Nullable fetch(sqlite3_stmt* statement, int index);
    static _WKSQLiteErrorCode bind(sqlite3_stmt* statement, int index, const Type _Nullable &);
};

template<>
class _WKWebExtensionSQLiteDatatypeTraits<int> {
public:
    static inline int fetch(sqlite3_stmt* statement, int index)
    {
        return sqlite3_column_int(statement, index);
    }

    static inline _WKSQLiteErrorCode bind(sqlite3_stmt* statement, int index, int value)
    {
        return sqlite3_bind_int(statement, index, value);
    }
};

template<>
class _WKWebExtensionSQLiteDatatypeTraits<int64_t> {
public:
    static inline int64_t fetch(sqlite3_stmt* statement, int index)
    {
        return sqlite3_column_int64(statement, index);
    }

    static inline _WKSQLiteErrorCode bind(sqlite3_stmt* statement, int index, int64_t value)
    {
        return sqlite3_bind_int64(statement, index, value);
    }
};

template<>
class _WKWebExtensionSQLiteDatatypeTraits<double> {
public:
    static inline double fetch(sqlite3_stmt* statement, int index)
    {
        return sqlite3_column_double(statement, index);
    }

    static inline _WKSQLiteErrorCode bind(sqlite3_stmt* statement, int index, double value)
    {
        return sqlite3_bind_double(statement, index, value);
    }
};

template<>
class _WKWebExtensionSQLiteDatatypeTraits<NSString *> {
public:
    static NSString * _Nullable fetch(sqlite3_stmt* statement, int index)
    {
        if (sqlite3_column_type(statement, index) == SQLITE_NULL)
            return nil;

        return CFBridgingRelease(CFStringCreateWithBytes(kCFAllocatorDefault, sqlite3_column_text(statement, index), sqlite3_column_bytes(statement, index), kCFStringEncodingUTF8, false));
    }

    static inline _WKSQLiteErrorCode bind(sqlite3_stmt* statement, int index, NSString * _Nullable value)
    {
        if (!value)
            return sqlite3_bind_null(statement, index);

        return sqlite3_bind_text(statement, index, value.UTF8String, -1, SQLITE_TRANSIENT);
    }
};

template<>
class _WKWebExtensionSQLiteDatatypeTraits<NSData *> {
public:
    static NSData * _Nullable fetch(sqlite3_stmt* statement, int index)
    {
        if (sqlite3_column_type(statement, index) == SQLITE_NULL)
            return nil;

        return CFBridgingRelease(CFDataCreate(kCFAllocatorDefault, (const UInt8 *)sqlite3_column_blob(statement, index), sqlite3_column_bytes(statement, index)));
    }

    static inline _WKSQLiteErrorCode bind(sqlite3_stmt* statement, int index, NSData * _Nullable value)
    {
        if (!value)
            return sqlite3_bind_null(statement, index);

        return sqlite3_bind_blob64(statement, index, value.bytes, value.length, SQLITE_TRANSIENT);
    }
};

template<>
class _WKWebExtensionSQLiteDatatypeTraits<NSDate *> {
public:
    static inline NSDate * _Nullable fetch(sqlite3_stmt* statement, int index)
    {
        if (sqlite3_column_type(statement, index) == SQLITE_NULL)
            return nil;

        return [NSDate dateWithTimeIntervalSinceReferenceDate:sqlite3_column_double(statement, index)];
    }

    static inline _WKSQLiteErrorCode bind(sqlite3_stmt* statement, int index, NSDate * _Nullable value)
    {
        if (!value)
            return sqlite3_bind_null(statement, index);

        return sqlite3_bind_double(statement, index, value.timeIntervalSinceReferenceDate);
    }
};

template<>
class _WKWebExtensionSQLiteDatatypeTraits<NSNumber *> {
public:
    static NSNumber * _Nullable fetch(sqlite3_stmt* statement, int index)
    {
        switch (sqlite3_column_type(statement, index)) {
        case SQLITE_INTEGER:
            return @(sqlite3_column_int64(statement, index));
        default:
        case SQLITE_FLOAT:
            return @(sqlite3_column_double(statement, index));
        case SQLITE_NULL:
            return nil;
        }
    }

    static _WKSQLiteErrorCode bind(sqlite3_stmt* statement, int index, NSNumber * _Nullable value)
    {
        if (!value)
            return sqlite3_bind_null(statement, index);

        const char* objCType = [value objCType];

        if (!strcmp(objCType, @encode(double)) || !strcmp(objCType, @encode(float)))
            return sqlite3_bind_double(statement, index, value.doubleValue);

        return sqlite3_bind_int64(statement, index, value.longLongValue);
    }
};

template<>
class _WKWebExtensionSQLiteDatatypeTraits<NSObject *> {
public:
    static NSObject * _Nullable fetch(sqlite3_stmt* statement, int index)
    {
        switch (sqlite3_column_type(statement, index)) {
        case SQLITE_INTEGER:
            return @(sqlite3_column_int64(statement, index));
        case SQLITE_FLOAT:
            return @(sqlite3_column_double(statement, index));
        case SQLITE_NULL:
            return nil;
        case SQLITE_BLOB:
            return _WKWebExtensionSQLiteDatatypeTraits<NSData *>::fetch(statement, index);
        case SQLITE_TEXT:
            return _WKWebExtensionSQLiteDatatypeTraits<NSString *>::fetch(statement, index);
        default:
            ASSERT_NOT_REACHED();
            return nil;
        }
    }

    static _WKSQLiteErrorCode bind(sqlite3_stmt* statement, int index, NSObject * _Nullable value)
    {
        if (!value)
            return sqlite3_bind_null(statement, index);

        if ([value isKindOfClass:[NSString class]])
            return sqlite3_bind_text(statement, index, ((NSString *)value).UTF8String, -1, SQLITE_TRANSIENT);

        if ([value isKindOfClass:[NSData class]])
            return sqlite3_bind_blob(statement, index, ((NSData *)value).bytes, (int)((NSData *)value).length, SQLITE_TRANSIENT);

        if ([value isKindOfClass:[NSNumber class]])
            return _WKWebExtensionSQLiteDatatypeTraits<NSNumber *>::bind(statement, index, (NSNumber *)value);

        if ([value isKindOfClass:[NSDate class]])
            return sqlite3_bind_double(statement, index, ((NSDate *)value).timeIntervalSinceReferenceDate);

        ASSERT_NOT_REACHED();
        return SQLITE_ERROR;
    }
};

template<>
class _WKWebExtensionSQLiteDatatypeTraits<std::nullptr_t> {
public:
    static inline std::nullptr_t fetch(sqlite3_stmt* statement, int index)
    {
        return std::nullptr_t();
    }

    static inline _WKSQLiteErrorCode bind(sqlite3_stmt* statement, int index, std::nullptr_t)
    {
        return sqlite3_bind_null(statement, index);
    }
};

template<>
class _WKWebExtensionSQLiteDatatypeTraits<decltype(std::ignore)> {
public:
    static inline decltype(std::ignore) fetch(sqlite3_stmt* statement, int index)
    {
        return std::ignore;
    }

    static inline _WKSQLiteErrorCode bind(sqlite3_stmt* statement, int index, decltype(std::ignore))
    {
        return SQLITE_OK;
    }
};

} // namespace WebKit

NS_ASSUME_NONNULL_END
