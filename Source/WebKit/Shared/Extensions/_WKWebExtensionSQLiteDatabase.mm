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
#import "_WKWebExtensionSQLiteDatabase.h"

#import "CocoaHelpers.h"
#import "Logging.h"
#import "_WKWebExtensionSQLiteHelpers.h"
#import <sqlite3.h>

NSString * const _WKWebExtensionSQLiteErrorDomain = @"com.apple.WebKit.SQLite";
static NSString * const _WKWebExtensionSQLiteInMemoryDatabaseName = @"file::memory:";
static NSString * const _WKWebExtensionSQLiteErrorMessageKey = @"Message";
static NSString * const _WKWebExtensionSQLiteErrorSQLKey = @"SQL";

using namespace WebKit;

#if ENABLE(WK_WEB_EXTENSIONS)

@implementation _WKWebExtensionSQLiteDatabase {
    sqlite3 *_handle;
    dispatch_queue_t _queue;
}

- (instancetype)initWithURL:(NSURL *)url queue:(dispatch_queue_t)queue
{
    ASSERT_ARG(url, url.isFileURL);

    if (!(self = [super init]))
        return nil;

    _url = [url copy];
    _queue = queue;

    return self;
}

- (void)dealloc
{
    ASSERT(!_handle);
}

- (_WKSQLiteErrorCode)close
{
    dispatch_assert_queue(_queue);

    _WKSQLiteErrorCode result = sqlite3_close_v2(_handle);
    if (result != SQLITE_OK) {
        RELEASE_LOG_ERROR(Extensions, "Failed to close database: %{public}@ (%d)", self.lastErrorMessage, (int)result);
        return result;
    }

    _handle = nullptr;
    return result;
}

- (BOOL)reportErrorWithCode:(_WKSQLiteErrorCode)errorCode query:(NSString *)query error:(NSError **)error
{
    dispatch_assert_queue(_queue);
    ASSERT(errorCode != SQLITE_OK);

    static const int expectedDictionarySize = 3;

    NSMutableDictionary *userInfo = [NSMutableDictionary dictionaryWithCapacity:expectedDictionarySize];

    if (_url)
        userInfo[NSURLErrorKey] = [_url copy];

    if (_WKSQLiteErrorCode(errorCode)) {
        if (const char* errMsg = sqlite3_errmsg(_handle))
            userInfo[_WKWebExtensionSQLiteErrorMessageKey] = @(errMsg);
    }

    if (query)
        userInfo[_WKWebExtensionSQLiteErrorSQLKey] = [query copy];

    NSError *errorObject = [NSError errorWithDomain:_WKWebExtensionSQLiteErrorDomain code:errorCode userInfo:userInfo];

    if (error)
        *error = errorObject;
    else
        RELEASE_LOG_ERROR(Extensions, "Unhandled SQLite error: %{public}@", privacyPreservingDescription(errorObject));

    return NO;
}

- (BOOL)reportErrorWithCode:(_WKSQLiteErrorCode)errorCode statement:(sqlite3_stmt*)statement error:(NSError **)error
{
    dispatch_assert_queue(_queue);
    ASSERT(errorCode != SQLITE_OK);

    if (statement) {
        if (char* sql = sqlite3_expanded_sql(statement)) {
            BOOL result = [self reportErrorWithCode:errorCode query:@(sql) error:error];
            sqlite3_free(sql);
            return result;
        }
    }

    return [self reportErrorWithCode:errorCode query:nil error:error];
}

- (BOOL)enableWAL:(NSError **)error
{
    // SQLite docs: The synchronous NORMAL setting is a good choice for most applications running in WAL mode.
    if (!SQLiteDatabaseExecuteAndReturnError(self, error, @"PRAGMA synchronous = NORMAL"))
        return NO;
    return SQLiteDatabaseEnumerate(self, error, @"PRAGMA journal_mode = WAL", std::tie(std::ignore));
}

- (BOOL)openWithAccessType:(SQLiteDatabaseAccessType)accessType error:(NSError **)error
{
    return [self openWithAccessType:accessType vfs:nil error:error];
}

- (BOOL)openWithAccessType:(SQLiteDatabaseAccessType)accessType vfs:(NSString *)vfs error:(NSError **)error
{
    return [self openWithAccessType:accessType protectionType:SQLiteDatabaseProtectionTypeDefault vfs:vfs error:error];
}

- (BOOL)openWithAccessType:(SQLiteDatabaseAccessType)accessType protectionType:(SQLiteDatabaseProtectionType)protectionType vfs:(NSString *)vfs error:(NSError **)error
{
    int flags = SQLITE_OPEN_NOMUTEX;

    switch (accessType) {
    case SQLiteDatabaseAccessTypeReadOnly:
        flags |= SQLITE_OPEN_READONLY;
        break;
    case SQLiteDatabaseAccessTypeReadWrite:
        flags |= SQLITE_OPEN_READWRITE;
        break;
    case SQLiteDatabaseAccessTypeReadWriteCreate:
        flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        break;
    }

#if PLATFORM(IOS_FAMILY)
    switch (protectionType) {
    case SQLiteDatabaseProtectionTypeDefault:
    case SQLiteDatabaseProtectionTypeCompleteUntilFirstUserAuthentication:
        flags |= SQLITE_OPEN_FILEPROTECTION_COMPLETEUNTILFIRSTUSERAUTHENTICATION;
        break;
    case SQLiteDatabaseProtectionTypeCompleteUnlessOpen:
        flags |= SQLITE_OPEN_FILEPROTECTION_COMPLETEUNLESSOPEN;
        break;
    case SQLiteDatabaseProtectionTypeComplete:
        flags |= SQLITE_OPEN_FILEPROTECTION_COMPLETE;
        break;
    }
#endif

    return [self _openWithFlags:flags vfs:vfs error:error];
}

- (BOOL)_openWithFlags:(int)flags vfs:(NSString *)vfs error:(NSError **)error
{
    dispatch_assert_queue(_queue);
    ASSERT(!_handle);

    const char *databasePath;
    if ([_url isEqual:[_WKWebExtensionSQLiteDatabase inMemoryDatabaseURL]])
        databasePath = [_WKWebExtensionSQLiteInMemoryDatabaseName UTF8String];
    else if ([_url isEqual:[_WKWebExtensionSQLiteDatabase privateOnDiskDatabaseURL]])
        databasePath = "";
    else {
        ASSERT(_url.path.length);
        databasePath = _url.path.fileSystemRepresentation;
        if (!ensureDirectoryExists(_url.URLByDeletingLastPathComponent)) {
            if (error) {
                RELEASE_LOG_ERROR(Extensions, "Unable to create parent folder for database at path: %{private}@", _url);
                *error = [self.class _errorWith_WKSQLiteErrorCode:SQLITE_CANTOPEN userInfo:nil];
            }

            return NO;
        }
    }

    _WKSQLiteErrorCode result = sqlite3_open_v2(databasePath, &_handle, flags, vfs.UTF8String);
    if (result == SQLITE_OK)
        return YES;

    // SQLite may return a valid database handle even if an error occurred. sqlite3_close silently
    // ignores calls with a null handle so we can call it here unconditionally.
    sqlite3_close_v2(_handle);
    _handle = nullptr;

    if (error)
        *error = [[self class] _errorWith_WKSQLiteErrorCode:result userInfo:nil];

    return NO;
}

+ (NSURL *)inMemoryDatabaseURL
{
    return [NSURL URLWithString:_WKWebExtensionSQLiteInMemoryDatabaseName];
}

+ (NSURL *)privateOnDiskDatabaseURL
{
    return [NSURL URLWithString:@"webkit::privateondisk:"];
}

+ (NSError *)_errorWith_WKSQLiteErrorCode:(_WKSQLiteErrorCode)errorCode userInfo:(NSDictionary *)userInfo
{
    if (errorCode == SQLITE_OK)
        return nil;

    NSString *errorMessage = @(sqlite3_errstr(errorCode));
    NSMutableDictionary *modifiedUserInfo = [userInfo mutableCopy];
    modifiedUserInfo[_WKWebExtensionSQLiteErrorMessageKey] = errorMessage;
    return [NSError errorWithDomain:_WKWebExtensionSQLiteErrorDomain code:errorCode userInfo:modifiedUserInfo];
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
