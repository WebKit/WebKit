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
#import "_WKWebExtensionSQLiteStore.h"

#import "CocoaHelpers.h"
#import "Logging.h"
#import "_WKWebExtensionSQLiteDatabase.h"
#import "_WKWebExtensionSQLiteHelpers.h"
#import "_WKWebExtensionSQLiteRow.h"
#import <sqlite3.h>
#import <wtf/BlockPtr.h>
#import <wtf/FileSystem.h>
#import <wtf/RunLoop.h>
#import <wtf/WeakObjCPtr.h>

using namespace WebKit;

#if ENABLE(WK_WEB_EXTENSIONS)

static constexpr std::array<const char *, 2> databaseFileSuffixes { "-shm", "-wal" };

@implementation _WKWebExtensionSQLiteStore

- (instancetype)initWithUniqueIdentifier:(NSString *)uniqueIdentifier directory:(NSString *)directory usesInMemoryDatabase:(BOOL)useInMemoryDatabase
{
    ASSERT_WITH_MESSAGE(![self.class isEqual:_WKWebExtensionSQLiteStore.class], "Must instantiate a concrete subclass instead.");

    if (!(self = [super init]))
        return nil;

    _uniqueIdentifier = [uniqueIdentifier copy];
    _directory = [NSURL fileURLWithPath:directory];
    _useInMemoryDatabase = useInMemoryDatabase;

    NSString *extensionDatabaseQueueName = [NSString stringWithFormat:@"com.apple.WebKit.WKWebExtensionSQLiteStore.%@", _uniqueIdentifier];
    _databaseQueue = dispatch_queue_create([extensionDatabaseQueueName cStringUsingEncoding:NSUTF8StringEncoding], DISPATCH_QUEUE_SERIAL);

    return self;
}

- (void)dealloc
{
    _WKWebExtensionSQLiteDatabase *database = _database;
    dispatch_async(_databaseQueue, ^{
        [database close];
    });
}

- (void)deleteStorageWithCompletionHandler:(void (^)(NSError *error))completionHandler
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), ^{
        NSError *error;
        [NSFileManager.defaultManager removeItemAtURL:self->_directory error:&error];
        completionHandler(error);
    });
}

- (void)deleteDatabaseWithCompletionHandler:(void (^)(NSString *errorMessage))completionHandler
{
    auto weakSelf = WeakObjCPtr<_WKWebExtensionSQLiteStore> { self };
    dispatch_async(_databaseQueue, ^{
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        NSString *deleteDatabaseErrorMessage = [strongSelf _deleteDatabase];
        dispatch_async(dispatch_get_main_queue(), ^{
            completionHandler(deleteDatabaseErrorMessage);
        });
    });
}

- (BOOL)useInMemoryDatabase
{
    return _useInMemoryDatabase;
}

// MARK: Database Management

- (void)_vacuum
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(_database);

    DatabaseResult result = SQLiteDatabaseExecute(_database, @"VACUUM");
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to vacuum database for extension %{private}@: %{public}@ (%d)", _uniqueIdentifier, _database.lastErrorMessage, result);
}

- (BOOL)_openDatabaseIfNecessaryReturningErrorMessage:(NSString **)outErrorMessage
{
    dispatch_assert_queue(_databaseQueue);

    if ([self _isDatabaseOpen]) {
        *outErrorMessage = nil;
        return YES;
    }

    *outErrorMessage = [self _openDatabase:self._databaseURL deleteDatabaseFileOnError:YES];
    return [self _isDatabaseOpen];
}

- (NSString *)_openDatabase:(NSURL *)databaseURL deleteDatabaseFileOnError:(BOOL)deleteDatabaseFileOnError
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(![self _isDatabaseOpen]);

    BOOL usingDatabaseFile = !_useInMemoryDatabase;

    if (usingDatabaseFile) {
        NSURL *storageFolderURL = ensureDirectoryExists(_directory);
        if (!storageFolderURL) {
            RELEASE_LOG_ERROR(Extensions, "Failed to create extension storage directory for extension %{private}@", _uniqueIdentifier);
            return @"Failed to create extension storage directory.";
        }
    }

    _database = [[_WKWebExtensionSQLiteDatabase alloc] initWithURL:databaseURL queue:_databaseQueue];

    // FIXME: rdar://87898825 (unlimitedStorage: Allow the SQLite database to be opened as SQLiteDatabaseAccessTypeReadOnly if the request is to calculate storage size).
    NSError *error;
    if (![_database openWithAccessType:SQLiteDatabaseAccessTypeReadWriteCreate error:&error]) {
        RELEASE_LOG_ERROR(Extensions, "Failed to open database for extension %{private}@: %{public}@", _uniqueIdentifier, privacyPreservingDescription(error));
        _database = nil;

        if (usingDatabaseFile && deleteDatabaseFileOnError)
            [self _deleteDatabaseFileAtURL:databaseURL reopenDatabase:YES];

        return @"Failed to open extension storage database.";
    }

    // Enable write-ahead logging to minimize the impact of SQLite's disk I/O.
    if (![_database enableWAL:&error]) {
        RELEASE_LOG_ERROR(Extensions, "Failed to enable write-ahead logging on database for extension %{private}@: %{public}@", _uniqueIdentifier, privacyPreservingDescription(error));
        _database = nil;

        if (usingDatabaseFile && deleteDatabaseFileOnError)
            [self _deleteDatabaseFileAtURL:databaseURL reopenDatabase:YES];

        return @"Failed to open extension storage database.";
    }

    return [self _handleSchemaVersioningWithDeleteDatabaseFileOnError:deleteDatabaseFileOnError];
}

- (BOOL)_isDatabaseOpen
{
    dispatch_assert_queue(_databaseQueue);

    return !!_database;
}

- (NSString *)_deleteDatabaseFileAtURL:(NSURL *)databaseURL reopenDatabase:(BOOL)reopenDatabase
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(!_useInMemoryDatabase);

    NSString *errorMessage;
    if ([self _isDatabaseOpen]) {
        if ([_database close] != SQLITE_OK)
            errorMessage = @"Failed to close extension storage database.";
        _database = nil;
    }

    auto databaseFilePath = (String)databaseURL.path;

    // -shm and -wal files may not exist, so don't report errors for those.
    for (auto* suffix : databaseFileSuffixes)
        FileSystem::deleteFile(databaseFilePath + suffix);

    if (!FileSystem::deleteFile(databaseFilePath)) {
        RELEASE_LOG_ERROR(Extensions, "Failed to delete database for extension %{private}@", _uniqueIdentifier);
        return @"Failed to delete extension storage database file.";
    }

    if (!reopenDatabase)
        return errorMessage;

    // Only try to recover from errors opening the database by deleting the file once.
    return [self _openDatabase:databaseURL deleteDatabaseFileOnError:NO];
}

- (void)_deleteExtensionStorageFolderIfEmpty
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(!_useInMemoryDatabase);

    FileSystem::deleteEmptyDirectory(_directory.path);
}

- (NSString *)_deleteDatabaseIfEmpty
{
    dispatch_assert_queue(_databaseQueue);
    if (!self._isDatabaseEmpty)
        return nil;

    return [self _deleteDatabase];
}

- (NSString *)_deleteDatabase
{
    dispatch_assert_queue(_databaseQueue);

    NSString *databaseCloseErrorMessage;
    if ([self _isDatabaseOpen]) {
        if ([_database close] != SQLITE_OK)
            databaseCloseErrorMessage = @"Failed to close extension storage database.";
        _database = nil;
    }

    if (_useInMemoryDatabase)
        return databaseCloseErrorMessage;

    NSString *deleteDatabaseFileErrorMessage = [self _deleteDatabaseFileAtURL:self._databaseURL reopenDatabase:NO];

    // We don't tell the extension if we fail to delete the enclosing folder since it only cares whether the database itself was deleted.
    [self _deleteExtensionStorageFolderIfEmpty];

    // An error from closing the database takes precedence over an error deleting the database file.
    return databaseCloseErrorMessage.length ? databaseCloseErrorMessage : deleteDatabaseFileErrorMessage;
}

- (NSString *)_handleSchemaVersioningWithDeleteDatabaseFileOnError:(BOOL)deleteDatabaseFileOnError
{
    SchemaVersion currentDatabaseSchemaVersion = self._currentDatabaseSchemaVersion;
    SchemaVersion schemaVersion = [self _migrateToCurrentSchemaVersionIfNeeded];
    if (schemaVersion != currentDatabaseSchemaVersion) {
        RELEASE_LOG_ERROR(Extensions, "Schema version (%d) does not match the supported schema version (%d) in database for extension %{private}@", schemaVersion, currentDatabaseSchemaVersion, _uniqueIdentifier);

        if (!_useInMemoryDatabase && deleteDatabaseFileOnError)
            [self _deleteDatabaseFileAtURL:self._databaseURL reopenDatabase:YES];
        else {
            [_database close];
            _database = nil;
        }

        return @"Failed to open extension storage database because of an invalid schema version.";
    }

    return nil;
}

- (SchemaVersion)_migrateToCurrentSchemaVersionIfNeeded
{
    dispatch_assert_queue(_databaseQueue);

    SchemaVersion currentDatabaseSchemaVersion = self._currentDatabaseSchemaVersion;

    _WKWebExtensionSQLiteRowEnumerator *rows = SQLiteDatabaseFetch(_database, @"PRAGMA user_version");
    SchemaVersion schemaVersion = [[rows nextObject] intAtIndex:0];
    [rows.statement invalidate];
    if (schemaVersion == currentDatabaseSchemaVersion)
        return schemaVersion;

    // The initial implementation of this class didn't store the schema version, which is indistinguishable from the database not existing at all.
    // Because of this, we still need to do the migration when schemaVersion is 0, even though this is unnecessary if the database we just created,
    // but we don't want to spam the log every time we create a new database.
    if (!!schemaVersion)
        RELEASE_LOG_INFO(Extensions, "Schema version (%d) does not match our supported schema version (%d) in database for extension %{private}@, recreating database", schemaVersion, currentDatabaseSchemaVersion, _uniqueIdentifier);

    // Someday we might migrate existing data from an older schema version, but we just start over for now.
    if ([self _resetDatabaseSchema] != SQLITE_DONE)
        return 0;

    if ([self _setDatabaseSchemaVersion:0] != SQLITE_DONE)
        return 0;

    [self _vacuum];

    // We're dealing with a fresh database. Create the schema from scratch.
    if ([self _createFreshDatabaseSchema] != SQLITE_DONE)
        return 0;

    [self _setDatabaseSchemaVersion:currentDatabaseSchemaVersion];

    return currentDatabaseSchemaVersion;
}

- (DatabaseResult)_setDatabaseSchemaVersion:(SchemaVersion)newVersion
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(_database);

    DatabaseResult result = SQLiteDatabaseExecute(_database, [NSString stringWithFormat:@"PRAGMA user_version = %d", newVersion]);
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to set database version for extension %{private}@: %{public}@ (%d)", _uniqueIdentifier, _database.lastErrorMessage, result);

    return result;

    return 0;
}

// MARK: - Must be implemented by subclasses

- (SchemaVersion)_currentDatabaseSchemaVersion
{
    // For subclasses to override.
    return 0;
}

- (NSURL *)_databaseURL
{
    // For subclasses to override.
    return (id _Nonnull)nil;
}

- (DatabaseResult)_createFreshDatabaseSchema
{
    // For subclasses to override.
    return 0;
}

- (DatabaseResult)_resetDatabaseSchema
{
    // For subclasses to override.
    return 0;
}

- (BOOL)_isDatabaseEmpty
{
    // For subclasses to override.
    return NO;
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
