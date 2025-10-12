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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "_WKWebExtensionSQLiteRow.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "_WKWebExtensionSQLiteDatabase.h"
#import "_WKWebExtensionSQLiteStatement.h"
#import <WebCore/SQLiteExtras.h>
#import <sqlite3.h>
#import <wtf/cocoa/SpanCocoa.h>

@interface _WKWebExtensionSQLiteRow ()
- (instancetype)init NS_DESIGNATED_INITIALIZER;
@end

@implementation _WKWebExtensionSQLiteRow {
    sqlite3_stmt *_handle;
    _WKWebExtensionSQLiteStatement *_statement;
}

- (instancetype)init
{
    ASSERT_NOT_REACHED();
    return nil;
}

- (instancetype)initWithStatement:(_WKWebExtensionSQLiteStatement *)statement
{
    if (!(self = [super init]))
        return nil;

    _handle = statement.handle;

    _statement = statement;
    dispatch_assert_queue(_statement.database.queue);

    return self;
}

- (instancetype)initWithCurrentRowOfEnumerator:(_WKWebExtensionSQLiteRowEnumerator *)rowEnumerator
{
    return [self initWithStatement:rowEnumerator.statement];
}

#pragma mark - Columns by index

- (NSString *)stringAtIndex:(NSUInteger)index
{
    dispatch_assert_queue(_statement.database.queue);
    if ([self _isNullAtIndex:index])
        return nil;

    sqlite3_stmt* handle = _handle;

    return WebCore::sqliteColumnText(handle, index).createNSString().autorelease();
}

- (int)intAtIndex:(NSUInteger)index
{
    dispatch_assert_queue(_statement.database.queue);
    return sqlite3_column_int(_handle, index);
}

- (int64_t)int64AtIndex:(NSUInteger)index
{
    dispatch_assert_queue(_statement.database.queue);
    return sqlite3_column_int64(_handle, index);
}

- (double)doubleAtIndex:(NSUInteger)index
{
    dispatch_assert_queue(_statement.database.queue);
    return sqlite3_column_double(_handle, index);
}

- (BOOL)boolAtIndex:(NSUInteger)index
{
    return !![self intAtIndex:index];
}

- (NSData *)dataAtIndex:(NSUInteger)index
{
    RawData data = [self uncopiedRawDataAtIndex:index];
    if (data.isNull)
        return nil;
    return toNSData(data.bytes).autorelease();
}

- (NSData *)uncopiedDataAtIndex:(NSUInteger)index
{
    RawData data = [self uncopiedRawDataAtIndex:index];
    if (data.isNull)
        return nil;
    return toNSDataNoCopy(data.bytes, FreeWhenDone::No).autorelease();
}

- (RawData)uncopiedRawDataAtIndex:(NSUInteger)index
{
    dispatch_assert_queue(_statement.database.queue);
    if ([self _isNullAtIndex:index])
        return RawData { true, { } };

    sqlite3_stmt* handle = _handle;

    auto blob = WebCore::sqliteColumnBlob(handle, index);
    return RawData { false, blob };
}

- (BOOL)_isNullAtIndex:(NSUInteger)index
{
    dispatch_assert_queue(_statement.database.queue);
    return sqlite3_column_type(_handle, index) == SQLITE_NULL;
}

- (NSObject *)objectAtIndex:(NSUInteger)index
{
    switch (sqlite3_column_type(_handle, index)) {
    case SQLITE_INTEGER:
        return @(sqlite3_column_int64(_handle, index));
    case SQLITE_FLOAT:
        return @(sqlite3_column_double(_handle, index));
    case SQLITE_TEXT:
        return [self stringAtIndex:index];
    case SQLITE_NULL:
        return [NSNull null];
    case SQLITE_BLOB:
        return [self dataAtIndex:index];
    default:
        ASSERT_NOT_REACHED();
        return nil;
    }
}

@end

@implementation _WKWebExtensionSQLiteRowEnumerator {
    _WKWebExtensionSQLiteStatement *_statement;
    _WKWebExtensionSQLiteRow *_row;
}

- (instancetype)initWithResultsOfStatement:(_WKWebExtensionSQLiteStatement *)statement
{
    if (!(self = [super init]))
        return nil;

    dispatch_assert_queue(statement.database.queue);
    _statement = statement;

    return self;
}

- (id)nextObject
{
    dispatch_assert_queue(_statement.database.queue);

    _lastResultCode = sqlite3_step(_statement.handle);

    switch (_lastResultCode) {
    case SQLITE_ROW:
        if (!_row)
            _row = [[_WKWebExtensionSQLiteRow alloc] initWithCurrentRowOfEnumerator:self];
        return _row;
    case SQLITE_OK:
    case SQLITE_DONE:
        break;

    default:
        [_statement.database reportErrorWithCode:_lastResultCode statement:_statement.handle error:nullptr];
        break;
    }

    return nil;
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
