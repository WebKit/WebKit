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

OBJC_CLASS _WKWebExtensionSQLiteDatabase;

NS_ASSUME_NONNULL_BEGIN

@interface _WKWebExtensionSQLiteStore : NSObject {
@protected
    NSString *_uniqueIdentifier;
    NSURL *_directory;
    _WKWebExtensionSQLiteDatabase *_database;
    dispatch_queue_t _databaseQueue;
    BOOL _useInMemoryDatabase;
}

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)initWithUniqueIdentifier:(NSString *)uniqueIdentifier directory:(NSString *)directory usesInMemoryDatabase:(BOOL)useInMemoryDatabase;

@property (nonatomic, readonly) BOOL useInMemoryDatabase;

- (void)close;

- (void)deleteDatabaseWithCompletionHandler:(void (^)(NSString * _Nullable errorMessage))completionHandler;

- (BOOL)_openDatabaseIfNecessaryReturningErrorMessage:(NSString * _Nullable * _Nonnull)outErrorMessage;
- (nullable NSString *)_deleteDatabaseIfEmpty;

- (void)createSavepointWithCompletionHandler:(void (^)(NSUUID * _Nullable savepointIdentifer, NSString * _Nullable errorMessage))completionHandler;
- (void)commitSavepoint:(NSUUID *)savepointIdentifier completionHandler:(void (^)(NSString * _Nullable errorMessage))completionHandler;
- (void)rollbackToSavepoint:(NSUUID *)savepointIdentifier completionHandler:(void (^)(NSString * _Nullable errorMessage))completionHandler;

@end

NS_ASSUME_NONNULL_END
