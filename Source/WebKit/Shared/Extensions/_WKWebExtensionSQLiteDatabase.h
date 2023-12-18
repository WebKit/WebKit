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

NS_ASSUME_NONNULL_BEGIN

typedef int DatabaseResult;
typedef int _WKSQLiteErrorCode;
typedef int SchemaVersion;

typedef NS_ENUM(NSInteger, SQLiteDatabaseAccessType) {
    SQLiteDatabaseAccessTypeReadOnly,
    SQLiteDatabaseAccessTypeReadWrite,
    SQLiteDatabaseAccessTypeReadWriteCreate,
};

// This enum is only applicable on iOS and has no effect on macOS.
// SQLiteDatabaseProtectionTypeDefault sets the protection to class C.
typedef NS_ENUM(NSInteger, SQLiteDatabaseProtectionType) {
    SQLiteDatabaseProtectionTypeDefault,
    SQLiteDatabaseProtectionTypeCompleteUntilFirstUserAuthentication,
    SQLiteDatabaseProtectionTypeCompleteUnlessOpen,
    SQLiteDatabaseProtectionTypeComplete,
};

struct sqlite3;
struct sqlite3_stmt;

extern NSString * const _WKWebExtensionSQLiteErrorDomain;

@interface _WKWebExtensionSQLiteDatabase : NSObject

@property (readonly, nonatomic, nullable) sqlite3 *handle;
@property (readonly, nonatomic) NSURL *url;

@property (readonly, nonatomic) _WKSQLiteErrorCode lastErrorCode;
@property (readonly, nonatomic, nullable) NSString *lastErrorMessage;

@property (readonly, nonatomic) dispatch_queue_t queue;

- (instancetype)initWithURL:(NSURL *)url queue:(dispatch_queue_t)queue;

- (BOOL)enableWAL:(NSError * __autoreleasing  * _Nullable)error;

- (BOOL)openWithAccessType:(SQLiteDatabaseAccessType)accessType error:(NSError **)error;
- (BOOL)openWithAccessType:(SQLiteDatabaseAccessType)accessType vfs:(nullable NSString *)vfs error:(NSError **)error;
- (BOOL)openWithAccessType:(SQLiteDatabaseAccessType)accessType protectionType:(SQLiteDatabaseProtectionType)protectionType vfs:(nullable NSString *)vfs error:(NSError **)error;

- (BOOL)reportErrorWithCode:(_WKSQLiteErrorCode)errorCode query:(nullable NSString *)query error:(NSError **)error;
- (BOOL)reportErrorWithCode:(_WKSQLiteErrorCode)errorCode statement:(sqlite3_stmt*)statement error:(NSError **)error;

- (_WKSQLiteErrorCode)close;

+ (NSURL *)inMemoryDatabaseURL;

@end

NS_ASSUME_NONNULL_END
