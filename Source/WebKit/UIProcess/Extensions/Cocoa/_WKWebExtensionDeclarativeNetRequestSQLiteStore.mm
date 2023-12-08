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
#import "_WKWebExtensionDeclarativeNetRequestSQLiteStore.h"

#import "CocoaHelpers.h"
#import "Logging.h"
#import "_WKWebExtensionSQLiteDatabase.h"
#import "_WKWebExtensionSQLiteHelpers.h"
#import "_WKWebExtensionSQLiteRow.h"
#import <wtf/WeakObjCPtr.h>

using namespace WebKit;

#if ENABLE(WK_WEB_EXTENSIONS)

static const SchemaVersion currentDatabaseSchemaVersion = 1;

@implementation _WKWebExtensionDeclarativeNetRequestSQLiteStore {
    NSString *_storageType;
    NSString *_tableName;
}

- (instancetype)initWithUniqueIdentifier:(NSString *)uniqueIdentifier storageType:(_WKWebExtensionDeclarativeNetRequestStorageType)storageType directory:(NSString *)directory usesInMemoryDatabase:(BOOL)useInMemoryDatabase
{
    if (!(self = [super initWithUniqueIdentifier:uniqueIdentifier directory:directory usesInMemoryDatabase:useInMemoryDatabase]))
        return nil;

    _storageType = storageType == _WKWebExtensionDeclarativeNetRequestStorageType::Dynamic ? @"dynamic" : @"session";
    _tableName = @"rules";
    return self;
}

- (void)updateRulesByRemovingIDs:(NSArray<NSNumber *> *)ruleIDs addRules:(NSArray<NSDictionary<NSString *, id> *> *)rules completionHandler:(void (^)(NSString *errorMessage))completionHandler
{
    auto weakSelf = WeakObjCPtr<_WKWebExtensionDeclarativeNetRequestSQLiteStore> { self };
    [self deleteRules:ruleIDs completionHandler:^(NSString *errorMessage) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        if (errorMessage.length) {
            completionHandler(errorMessage);
            return;
        }

        [strongSelf addRules:rules completionHandler:^(NSString *errorMessage) {
            completionHandler(errorMessage);
        }];
    }];
}

- (void)addRules:(NSArray<NSDictionary<NSString *, id> *> *)rules completionHandler:(void (^)(NSString *errorMessage))completionHandler
{
    if (!rules.count) {
        completionHandler(nil);
        return;
    }

    auto weakSelf = WeakObjCPtr<_WKWebExtensionDeclarativeNetRequestSQLiteStore> { self };
    dispatch_async(_databaseQueue, ^{
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        NSString *errorMessage;
        if (![strongSelf _openDatabaseIfNecessaryReturningErrorMessage:&errorMessage]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(errorMessage);
            });

            return;
        }

        ASSERT(!errorMessage.length);
        ASSERT(strongSelf->_database);

        NSMutableArray<NSNumber *> *rulesIDs = [NSMutableArray arrayWithCapacity:rules.count];
        for (NSDictionary<NSString *, id> *rule in rules) {
            NSNumber *ruleID = objectForKey<NSNumber>(rule, @"id");
            ASSERT(ruleID);
            [rulesIDs addObject:ruleID];
        }

        ASSERT(rulesIDs.count);

        _WKWebExtensionSQLiteRowEnumerator *rows = SQLiteDatabaseFetch(strongSelf->_database, [NSString stringWithFormat:@"SELECT id FROM %@ WHERE id in (%@)", strongSelf->_tableName, [rulesIDs componentsJoinedByString:@", "]]);

        NSMutableArray<NSNumber *> *existingRuleIDs = [NSMutableArray array];
        for (_WKWebExtensionSQLiteRow *row in rows)
            [existingRuleIDs addObject:[NSNumber numberWithInteger:[row int64AtIndex:0]]];

        if (existingRuleIDs.count == 1)
            errorMessage = [NSString stringWithFormat:@"Failed to add %@ rules. Rule %@ does not have a unique ID.", strongSelf->_storageType, existingRuleIDs.firstObject];
        else if (existingRuleIDs.count >= 2)
            errorMessage = [NSString stringWithFormat:@"Failed to add %@ rules. Some rules do not have unique IDs (%@).", strongSelf->_storageType, [existingRuleIDs componentsJoinedByString:@", "]];

        if (errorMessage.length) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(errorMessage);
            });

            return;
        }

        for (NSDictionary<NSString *, id> *rule in rules) {
            errorMessage = [strongSelf _insertRule:rule inDatabase:strongSelf->_database];
            if (errorMessage.length)
                break;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            completionHandler(errorMessage);
        });
    });
}

- (void)deleteRules:(NSArray<NSNumber *> *)ruleIDs completionHandler:(void (^)(NSString *errorMessage))completionHandler
{
    if (!ruleIDs.count) {
        completionHandler(nil);
        return;
    }

    auto weakSelf = WeakObjCPtr<_WKWebExtensionDeclarativeNetRequestSQLiteStore> { self };
    dispatch_async(_databaseQueue, ^{
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        NSString *errorMessage;
        if (![strongSelf _openDatabaseIfNecessaryReturningErrorMessage:&errorMessage]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(errorMessage);
            });

            return;
        }

        ASSERT(!errorMessage.length);
        ASSERT(strongSelf->_database);

        DatabaseResult result = SQLiteDatabaseExecute(strongSelf->_database, [NSString stringWithFormat:@"DELETE FROM %@ WHERE id in (%@)", strongSelf->_tableName, [ruleIDs componentsJoinedByString:@", "]]);
        if (result != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Extensions, "Failed to delete rules for extension %{private}@.", strongSelf->_uniqueIdentifier);
            errorMessage = [NSString stringWithFormat:@"Failed to delete rules from %@ rules storage.", strongSelf->_storageType];
        }

        NSString *deleteDatabaseErrorMessage = [strongSelf _deleteDatabaseIfEmpty];

        dispatch_async(dispatch_get_main_queue(), ^{
            // Errors from opening the database or deleting keys take precedence over an error deleting the database.
            completionHandler(errorMessage.length ? errorMessage : deleteDatabaseErrorMessage);
        });
    });
}

- (void)getRulesWithCompletionHandler:(void (^)(NSArray<NSDictionary<NSString *, id> *> *rules, NSString *errorMessage))completionHandler
{
    auto weakSelf = WeakObjCPtr<_WKWebExtensionDeclarativeNetRequestSQLiteStore> { self };
    dispatch_async(_databaseQueue, ^{
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        NSString *errorMessage;
        NSArray<NSDictionary<NSString *, id> *> *rules = [strongSelf _getRulesWithOutErrorMessage:&errorMessage];

        dispatch_async(dispatch_get_main_queue(), ^{
            completionHandler(rules, errorMessage);
        });
    });
}

- (NSArray<NSDictionary<NSString *, id> *> *)_getRulesWithOutErrorMessage:(NSString **)outErrorMessage
{
    dispatch_assert_queue(_databaseQueue);

    if (![self _openDatabaseIfNecessaryReturningErrorMessage:outErrorMessage])
        return @[ ];

    ASSERT(!(*outErrorMessage).length);
    ASSERT(_database);

    _WKWebExtensionSQLiteRowEnumerator *rows = SQLiteDatabaseFetch(_database, [NSString stringWithFormat:@"SELECT * FROM %@", _tableName]);
    return [self _getKeysAndValuesFromRowEnumerator:rows];
}

- (NSArray<NSDictionary<NSString *, id> *> *)_getKeysAndValuesFromRowEnumerator:(_WKWebExtensionSQLiteRowEnumerator *)rows
{
    static NSSet *allowedClasses = [NSSet setWithObjects:NSDictionary.class, NSNumber.class, NSString.class, NSArray.class, nil];

    NSMutableArray<NSDictionary<NSString *, id> *> *rules = [NSMutableArray array];

    for (_WKWebExtensionSQLiteRow *row in rows) {
        NSError *error;
        NSDictionary<NSString *, id> *rule = [NSKeyedUnarchiver unarchivedObjectOfClasses:allowedClasses fromData:[row dataAtIndex:1] error:&error];

        if (error)
            RELEASE_LOG_ERROR(Extensions, "Failed to deserialize dynamic declarative net request rule for extension %{private}@.", _uniqueIdentifier);
        else
            [rules addObject:rule];
    }

    return rules;
}

- (NSString *)_insertRule:(NSDictionary<NSString *, id> *)rule inDatabase:(_WKWebExtensionSQLiteDatabase *)database
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(_database);

    NSData *ruleAsData = [NSKeyedArchiver archivedDataWithRootObject:rule requiringSecureCoding:YES error:nil];
    NSNumber *ruleID = objectForKey<NSNumber>(rule, @"id");
    ASSERT(ruleID);

    DatabaseResult result = SQLiteDatabaseExecute(database, [NSString stringWithFormat:@"INSERT INTO %@ (id, rule) VALUES (?, ?)", _tableName], ruleID.integerValue, ruleAsData);
    if (result != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Extensions, "Failed to insert dynamic declarative net request rule for extension %{private}@.", _uniqueIdentifier);
        return [NSString stringWithFormat:@"Failed to add %@ rule.", self->_storageType];
    }

    return nil;
}

// MARK: Database Schema

- (SchemaVersion)_currentDatabaseSchemaVersion
{
    return currentDatabaseSchemaVersion;
}

- (NSURL *)_databaseURL
{
    if (_useInMemoryDatabase)
        return [_WKWebExtensionSQLiteDatabase inMemoryDatabaseURL];

    NSString *databaseName = @"dnr-dynamic-rules.db";
    return [_directory URLByAppendingPathComponent:databaseName isDirectory:NO];
}

- (DatabaseResult)_createFreshDatabaseSchema
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(_database);

    DatabaseResult result = SQLiteDatabaseExecute(_database, [NSString stringWithFormat:@"CREATE TABLE %@ (id INTEGER PRIMARY KEY NOT NULL, rule BLOB NOT NULL)", _tableName]);
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to create %@ database for extension %{private}@: %{public}@ (%d)", _tableName, _uniqueIdentifier, _database.lastErrorMessage, result);

    return result;
}

- (DatabaseResult)_resetDatabaseSchema
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(_database);

    DatabaseResult result = SQLiteDatabaseExecute(_database, [NSString stringWithFormat:@"DROP TABLE IF EXISTS %@", _tableName]);
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to reset %@ database schema for extension %{private}@: %{public}@ (%d)", _tableName, _uniqueIdentifier, _database.lastErrorMessage, result);

    return result;
}

- (BOOL)_isDatabaseEmpty
{
    dispatch_assert_queue(_databaseQueue);
    ASSERT(_database);

    _WKWebExtensionSQLiteRowEnumerator *rows = SQLiteDatabaseFetch(_database, [NSString stringWithFormat:@"SELECT COUNT(*) FROM %@", _tableName]);
    return ![[rows nextObject] int64AtIndex:0];
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
