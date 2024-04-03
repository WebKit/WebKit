/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "_WKWebExtensionStorageSQLiteStore.h"

#import "CocoaHelpers.h"
#import "Logging.h"
#import "WebExtensionDataType.h"
#import "WebExtensionUtilities.h"
#import "_WKWebExtensionSQLiteDatabase.h"
#import "_WKWebExtensionSQLiteHelpers.h"
#import "_WKWebExtensionSQLiteRow.h"
#import <wtf/WeakObjCPtr.h>

using namespace WebKit;

#if ENABLE(WK_WEB_EXTENSIONS)

static const SchemaVersion currentDatabaseSchemaVersion = 1;

static NSString *rowFilterStringFromRowKeys(NSArray<NSString *> *keys)
{
    auto *escapedAndQuotedKeys = [NSMutableArray arrayWithCapacity:keys.count];

    for (NSString *key in keys) {
        auto *keyWithSingleQuotesEscaped = [key stringByReplacingOccurrencesOfString:@"'" withString:@"''"];
        [escapedAndQuotedKeys addObject:[NSString stringWithFormat:@"'%@'", keyWithSingleQuotesEscaped]];
    }

    return [escapedAndQuotedKeys componentsJoinedByString:@","];
}

@implementation _WKWebExtensionStorageSQLiteStore {
    WebExtensionDataType _storageType;
}

- (instancetype)initWithUniqueIdentifier:(NSString *)uniqueIdentifier storageType:(WebExtensionDataType)storageType directory:(NSString *)directory usesInMemoryDatabase:(BOOL)useInMemoryDatabase
{
    if (!(self = [super initWithUniqueIdentifier:uniqueIdentifier directory:directory usesInMemoryDatabase:useInMemoryDatabase]))
        return nil;

    _storageType = storageType;

    return self;
}

- (void)getValuesForKeys:(NSArray<NSString *> *)keys completionHandler:(void (^)(NSDictionary<NSString *, NSString *> *results, NSString *errorMessage))completionHandler
{
    auto weakSelf = WeakObjCPtr<_WKWebExtensionStorageSQLiteStore> { self };
    dispatch_async(_databaseQueue, ^{
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            RELEASE_LOG_ERROR(Extensions, "Failed to retrieve keys: %{private}@ for extension %{private}@.", keys, self->_uniqueIdentifier);
            completionHandler(nil, [NSString stringWithFormat:@"Failed to retrieve keys %@", keys]);
            return;
        }

        NSString *errorMessage;
        auto *results = keys.count ? [self _getValuesForKeys:keys outErrorMessage:&errorMessage] : [self _getValuesForAllKeysReturningErrorMessage:&errorMessage];

        dispatch_async(dispatch_get_main_queue(), ^{
            completionHandler(results, errorMessage);
        });
    });
}

- (void)getStorageSizeForKeys:(NSArray<NSString *> *)keys completionHandler:(void (^)(size_t storageSize, NSString *errorMessage))completionHandler
{
    auto weakSelf = WeakObjCPtr<_WKWebExtensionStorageSQLiteStore> { self };
    dispatch_async(_databaseQueue, ^{
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            RELEASE_LOG_ERROR(Extensions, "Failed to calculate storage size for keys: %{private}@ for extension %{private}@.", keys, self->_uniqueIdentifier);
            completionHandler(0, [NSString stringWithFormat:@"Failed to caluclate storage size for keys: %@", keys]);
            return;
        }

        NSString *errorMessage;

        if (keys.count) {
            auto *keysAndValues = [self _getValuesForKeys:keys outErrorMessage:&errorMessage];
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(storageSizeOf(keysAndValues), errorMessage);
            });

            return;
        }

        // Return storage size for all keys if no keys are specified.
        if (![self _openDatabaseIfNecessaryReturningErrorMessage:&errorMessage createIfNecessary:NO]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(0, errorMessage);
            });
            return;
        }

        int64_t result = 0;
        NSError *error;
        bool success = SQLiteDatabaseEnumerate(self->_database, &error, @"SELECT SUM(LENGTH(key) + LENGTH(value)) FROM extension_storage", std::tie(result));

        dispatch_async(dispatch_get_main_queue(), ^{
            if (success)
                completionHandler(result, nil);
            else {
                RELEASE_LOG_ERROR(Extensions, "Failed to calculate storage size for keys: %{private}@ for extension %{private}@. %{private}@", keys, self->_uniqueIdentifier, error.localizedDescription);
                completionHandler(0, error.localizedDescription);
            }
        });
    });
}

- (void)getStorageSizeForAllKeysIncludingKeyedData:(NSDictionary<NSString *, NSString *> *)additionalKeyedData withCompletionHandler:(void (^)(size_t storageSize, NSUInteger numberOfKeysIncludingAdditionalKeyedData, NSDictionary<NSString *, NSString *> *existingKeysAndValues, NSString *errorMessage))completionHandler
{
    [self getStorageSizeForKeys:@[] completionHandler:^(size_t storageSize, NSString *errorMessage) {
        if (errorMessage.length) {
            completionHandler(0.0, 0, @{ }, errorMessage);
            return;
        }

        auto weakSelf = WeakObjCPtr<_WKWebExtensionStorageSQLiteStore> { self };
        dispatch_async(self->_databaseQueue, ^{
            auto strongSelf = weakSelf.get();
            if (!strongSelf) {
                RELEASE_LOG_ERROR(Extensions, "Failed to calculate storage size for extension %{private}@.", self->_uniqueIdentifier);
                completionHandler(0.0, 0, @{ }, @"Failed to calculate storage size");
                return;
            }

            NSString *errorMessage;
            auto *oldValuesForAdditionalKeys = [self _getValuesForKeys:additionalKeyedData.allKeys outErrorMessage:&errorMessage];
            size_t oldStorageSizeForAdditionalKeys = storageSizeOf(oldValuesForAdditionalKeys);
            size_t newStorageSizeForAdditionalKeys = storageSizeOf(additionalKeyedData);
            size_t updatedStorageSize = storageSize - oldStorageSizeForAdditionalKeys + newStorageSizeForAdditionalKeys;

            auto *existingAndAdditionalKeys = [NSMutableSet setWithSet:[self _getAllKeysReturningErrorMessage:&errorMessage]];
            [existingAndAdditionalKeys unionSet:[NSSet setWithArray:additionalKeyedData.allKeys]];

            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(updatedStorageSize, existingAndAdditionalKeys.count, oldValuesForAdditionalKeys, errorMessage);
            });
        });
    }];
}

- (void)setKeyedData:(NSDictionary<NSString *, NSString *> *)keyedData completionHandler:(void (^)(NSArray<NSString *> *keysSuccessfullySet, NSString *errorMessage))completionHandler
{
    auto weakSelf = WeakObjCPtr<_WKWebExtensionStorageSQLiteStore> { self };
    dispatch_async(_databaseQueue, ^{
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            completionHandler(nil, [NSString stringWithFormat:@"Failed to set keys %@", keyedData.allKeys]);
            return;
        }

        NSString *errorMessage;
        if (![self _openDatabaseIfNecessaryReturningErrorMessage:&errorMessage]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(nil, errorMessage);
            });
            return;
        }

        ASSERT(!errorMessage.length);

        auto *keysSuccessfullySet = [NSMutableArray array];

        for (NSString *key in keyedData) {
            errorMessage = [self _insertOrUpdateValue:keyedData[key] forKey:key inDatabase:self->_database];
            if (errorMessage.length)
                break;

            [keysSuccessfullySet addObject:key];
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            completionHandler([keysSuccessfullySet copy], errorMessage);
        });
    });
}

- (void)deleteValuesForKeys:(NSArray<NSString *> *)keys completionHandler:(void (^)(NSString *errorMessage))completionHandler
{
    auto weakSelf = WeakObjCPtr<_WKWebExtensionStorageSQLiteStore> { self };
    dispatch_async(_databaseQueue, ^{
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            completionHandler([NSString stringWithFormat:@"Failed to delete keys %@", keys]);
            return;
        }

        NSString *errorMessage;
        if (![self _openDatabaseIfNecessaryReturningErrorMessage:&errorMessage createIfNecessary:NO]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(errorMessage);
            });
            return;
        }

        ASSERT(!errorMessage.length);

        DatabaseResult result = SQLiteDatabaseExecute(self->_database, [NSString stringWithFormat:@"DELETE FROM extension_storage WHERE key in (%@)", rowFilterStringFromRowKeys(keys)]);
        if (result != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Extensions, "Failed to delete keys %{private}@ for extension %{private}@.", keys, self->_uniqueIdentifier);
            errorMessage = [NSString stringWithFormat:@"Failed to delete keys %@", keys];
        }

        NSString *deleteDatabaseErrorMessage = [self _deleteDatabaseIfEmpty];

        dispatch_async(dispatch_get_main_queue(), ^{
            // Errors from opening the database or deleting keys take precedence over an error deleting the database.
            completionHandler(errorMessage.length ? errorMessage : deleteDatabaseErrorMessage);
        });
    });
}

- (NSURL *)_databaseURL
{
    if (_useInMemoryDatabase)
        return _WKWebExtensionSQLiteDatabase.inMemoryDatabaseURL;

    NSString *databaseName;
    switch (_storageType) {
    case WebExtensionDataType::Local:
        databaseName = @"LocalStorage.db";
        break;
    case WebExtensionDataType::Sync:
        databaseName = @"SyncStorage.db";
        break;
    case WebExtensionDataType::Session:
        // Session storage is kept in memory only.
        ASSERT_NOT_REACHED();
    }

    ASSERT(_directory);

    return [_directory URLByAppendingPathComponent:databaseName isDirectory:NO];
}

- (NSString *)_insertOrUpdateValue:(NSString *)value forKey:(NSString *)key inDatabase:(_WKWebExtensionSQLiteDatabase *)database
{
    dispatch_assert_queue(_databaseQueue);
    DatabaseResult result = SQLiteDatabaseExecute(database, @"INSERT OR REPLACE INTO extension_storage (key, value) VALUES (?, ?)", key, value);
    if (result != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Extensions, "Failed to insert value %{private}@ for key %{private}@ for extension %{private}@.", value, key, _uniqueIdentifier);
        return [NSString stringWithFormat:@"Failed to insert value %@ for key %@", value, key];
    }

    return nil;
}

- (NSDictionary<NSString *, NSString *> *)_getValuesForAllKeysReturningErrorMessage:(NSString **)outErrorMessage
{
    dispatch_assert_queue(_databaseQueue);

    if (![self _openDatabaseIfNecessaryReturningErrorMessage:outErrorMessage createIfNecessary:NO])
        return @{ };

    ASSERT(!(*outErrorMessage).length);

    _WKWebExtensionSQLiteRowEnumerator *rows = SQLiteDatabaseFetch(_database, @"SELECT * FROM extension_storage");
    return [self _getKeysAndValuesFromRowEnumerator:rows];
}

- (NSDictionary<NSString *, NSString *> *)_getKeysAndValuesFromRowEnumerator:(_WKWebExtensionSQLiteRowEnumerator *)rows
{
    auto *results = [NSMutableDictionary dictionary];

    for (_WKWebExtensionSQLiteRow *row in rows) {
        auto *key = [row stringAtIndex:0];
        auto *value = [row stringAtIndex:1];

        results[key] = value;
    }

    return [results copy];
}

- (NSSet<NSString *> *)_getAllKeysReturningErrorMessage:(NSString **)outErrorMessage
{
    dispatch_assert_queue(_databaseQueue);

    if (![self _openDatabaseIfNecessaryReturningErrorMessage:outErrorMessage createIfNecessary:NO])
        return [NSSet set];

    ASSERT(!(*outErrorMessage).length);

    auto *keys = [NSMutableSet set];
    _WKWebExtensionSQLiteRowEnumerator *rows = SQLiteDatabaseFetch(_database, @"SELECT key FROM extension_storage");
    for (_WKWebExtensionSQLiteRow *row in rows)
        [keys addObject:[row stringAtIndex:0]];

    return keys;
}

- (NSDictionary<NSString *, NSString *> *)_getValuesForKeys:(NSArray<NSString *> *)keys outErrorMessage:(NSString **)outErrorMessage
{
    dispatch_assert_queue(_databaseQueue);

    if (![self _openDatabaseIfNecessaryReturningErrorMessage:outErrorMessage createIfNecessary:NO])
        return @{ };

    ASSERT(!(*outErrorMessage).length);

    _WKWebExtensionSQLiteRowEnumerator *rows = SQLiteDatabaseFetch(_database, [NSString stringWithFormat:@"SELECT * FROM extension_storage WHERE key in (%@)", rowFilterStringFromRowKeys(keys)]);
    return [self _getKeysAndValuesFromRowEnumerator:rows];
}

// MARK: Database Schema

- (SchemaVersion)_currentDatabaseSchemaVersion
{
    return currentDatabaseSchemaVersion;
}

- (DatabaseResult)_createFreshDatabaseSchema
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(_database);

    DatabaseResult result = SQLiteDatabaseExecute(_database, @"CREATE TABLE extension_storage (key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL)");
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to create the extension_storage table for extension %{private}@: %{public}@ (%d)", _uniqueIdentifier, _database.lastErrorMessage, result);

    return result;
}

- (DatabaseResult)_resetDatabaseSchema
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(_database);

    DatabaseResult result = SQLiteDatabaseExecute(_database, @"DROP TABLE IF EXISTS extension_storage");
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to reset database schema for extension %{private}@: %{public}@ (%d)", _uniqueIdentifier, _database.lastErrorMessage, result);

    return result;
}

- (BOOL)_isDatabaseEmpty
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(_database);

    _WKWebExtensionSQLiteRowEnumerator *rows = SQLiteDatabaseFetch(_database, @"SELECT COUNT(*) FROM extension_storage");
    return ![[rows nextObject] int64AtIndex:0];
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
