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
#import "_WKWebExtensionSQLiteStatement.h"

#import "Logging.h"
#import "_WKWebExtensionSQLiteDatabase.h"
#import "_WKWebExtensionSQLiteHelpers.h"
#import "_WKWebExtensionSQLiteRow.h"
#import <WebCore/SQLiteExtras.h>
#import <sqlite3.h>

#if ENABLE(WK_WEB_EXTENSIONS)

using namespace WebKit;

@interface _WKWebExtensionSQLiteStatement ()
- (instancetype)init NS_DESIGNATED_INITIALIZER;
@end

@implementation _WKWebExtensionSQLiteStatement {
    _WKWebExtensionSQLiteDatabase *_database;
    sqlite3_stmt* _handle;

    NSDictionary *_columnNamesToIndexes;
    NSArray *_columnNames;
}

- (instancetype)init
{
    ASSERT_NOT_REACHED();
    return nil;
}

- (instancetype)initWithDatabase:(_WKWebExtensionSQLiteDatabase *)database query:(NSString *)query error:(NSError **)error
{
    ASSERT_ARG(database, database.handle);

    if (!(self = [super init]))
        return nil;

    _database = database;

    dispatch_assert_queue(_database.queue);
    _WKSQLiteErrorCode result = sqlite3_prepare_v2(database.handle, query.UTF8String, -1, &_handle, 0);
    if (result != SQLITE_OK) {
        [database reportErrorWithCode:result query:query error:error];
        return nil;
    }

    return self;
}

- (instancetype)initWithDatabase:(_WKWebExtensionSQLiteDatabase *)database query:(NSString *)query
{
    return [self initWithDatabase:database query:query error:nullptr];
}

- (void)dealloc
{
    sqlite3_stmt* handle = _handle;
    if (!handle)
        return;

    auto *database = _database;
    dispatch_async(database.queue, ^{
        // The database might have closed already.
        if (!database.handle)
            return;

        sqlite3_finalize(handle);
    });
}

- (_WKSQLiteErrorCode)execute
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);

    _WKSQLiteErrorCode resultCode = sqlite3_step(_handle);
    if (!SQLiteIsExecutionError(resultCode))
        return resultCode;

    [_database reportErrorWithCode:resultCode statement:_handle error:nullptr];
    return resultCode;
}

- (BOOL)execute:(NSError **)error
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);

    _WKSQLiteErrorCode resultCode = sqlite3_step(_handle);
    if (!SQLiteIsExecutionError(resultCode))
        return YES;

    [_database reportErrorWithCode:resultCode statement:_handle error:error];
    return NO;
}

- (_WKWebExtensionSQLiteRowEnumerator *)fetch
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);

    return [[_WKWebExtensionSQLiteRowEnumerator alloc] initWithResultsOfStatement:self];
}

- (BOOL)fetchWithEnumerationBlock:(void (^)(_WKWebExtensionSQLiteRow *row, BOOL* stop))enumerationBlock error:(NSError **)error
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);

    _WKWebExtensionSQLiteRow *row;

    _WKSQLiteErrorCode result = SQLITE_OK;
    BOOL stop = NO;
    while (!stop) {
        result = sqlite3_step(_handle);
        if (result != SQLITE_ROW)
            break;

        if (!row)
            row = [[_WKWebExtensionSQLiteRow alloc] initWithStatement:self];

        enumerationBlock(row, &stop);
    }

    if (result == SQLITE_DONE)
        return YES;

    return NO;
}

- (void)reset
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);

    _WKSQLiteErrorCode result = sqlite3_reset(_handle);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not reset statement: %@ (%d)", _database.lastErrorMessage, (int)result);
}

- (void)invalidate
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);

    _WKSQLiteErrorCode result = sqlite3_finalize(_handle);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not finalize statement: %@ (%d)", _database.lastErrorMessage, (int)result);
    _handle = nullptr;
}

- (void)bindString:(NSString *)string atParameterIndex:(NSUInteger)index
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);
    ASSERT_ARG(index, index > 0);

    _WKSQLiteErrorCode result = WebCore::sqliteBindText(_handle, index, String(string).utf8());
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind string: %@ (%d)", _database.lastErrorMessage, (int)result);
}

- (void)bindInt:(int)n atParameterIndex:(NSUInteger)index
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);
    ASSERT_ARG(index, index > 0);

    _WKSQLiteErrorCode result = sqlite3_bind_int(_handle, index, n);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind int: %@ (%d)", _database.lastErrorMessage, (int)result);
}

- (void)bindInt64:(int64_t)n atParameterIndex:(NSUInteger)index
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);
    ASSERT_ARG(index, index > 0);

    _WKSQLiteErrorCode result = sqlite3_bind_int64(_handle, index, n);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind integer: %@ (%d)", _database.lastErrorMessage, (int)result);
}

- (void)bindDouble:(double)n atParameterIndex:(NSUInteger)index
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);
    ASSERT_ARG(index, index > 0);

    _WKSQLiteErrorCode result = sqlite3_bind_double(_handle, index, n);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind double: %@ (%d)", _database.lastErrorMessage, (int)result);
}

- (void)bindData:(NSData *)data atParameterIndex:(NSUInteger)index
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);
    ASSERT_ARG(index, index > 0);

    _WKSQLiteErrorCode result = WebCore::sqliteBindBlob(_handle, index, span(data));
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind blob: %@ (%d)", _database.lastErrorMessage, (int)result);
}

- (void)bindNullAtParameterIndex:(NSUInteger)index
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);
    ASSERT_ARG(index, index > 0);

    _WKSQLiteErrorCode result = sqlite3_bind_null(_handle, index);
    if (result != SQLITE_OK)
        RELEASE_LOG_DEBUG(Extensions, "Could not bind null: %@ (%d)", _database.lastErrorMessage, (int)result);
}

- (NSDictionary *)columnNamesToIndexes
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);
    if (_columnNamesToIndexes)
        return _columnNamesToIndexes;

    int columnCount = sqlite3_column_count(_handle);
    NSMutableDictionary *columnNamesToIndexes = [[NSMutableDictionary alloc] initWithCapacity:columnCount];
    for (int i = 0; i < columnCount; ++i) {
        String columnName = WebCore::sqliteColumnName(_handle, i);
        columnNamesToIndexes[columnName.createNSString().get()] = @(i);
    }

    _columnNamesToIndexes = columnNamesToIndexes;
    return _columnNamesToIndexes;
}

- (NSArray *)columnNames
{
    dispatch_assert_queue(_database.queue);
    ASSERT(self._isValid);
    if (_columnNames)
        return _columnNames;

    int columnCount = sqlite3_column_count(_handle);
    NSMutableArray *columnNames = [[NSMutableArray alloc] initWithCapacity:columnCount];
    for (int i = 0; i < columnCount; ++i)
        [columnNames addObject:WebCore::sqliteColumnName(_handle, i).createNSString().get()];

    _columnNames = columnNames;
    return _columnNames;
}

- (BOOL)_isValid
{
    return !!_handle;
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
