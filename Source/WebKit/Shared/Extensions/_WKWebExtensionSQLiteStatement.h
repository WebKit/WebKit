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

#import "_WKWebExtensionSQLiteDatabase.h"

NS_ASSUME_NONNULL_BEGIN

@class _WKWebExtensionSQLiteRow;
@class _WKWebExtensionSQLiteRowEnumerator;

@interface _WKWebExtensionSQLiteStatement : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (nullable instancetype)initWithDatabase:(_WKWebExtensionSQLiteDatabase *)database query:(NSString *)query;
- (nullable instancetype)initWithDatabase:(_WKWebExtensionSQLiteDatabase *)database query:(NSString *)query error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (void)bindString:(nullable NSString *)string atParameterIndex:(NSUInteger)index;
- (void)bindInt:(int)n atParameterIndex:(NSUInteger)index;
- (void)bindInt64:(int64_t)n atParameterIndex:(NSUInteger)index;
- (void)bindDouble:(double)n atParameterIndex:(NSUInteger)index;
- (void)bindData:(nullable NSData *)data atParameterIndex:(NSUInteger)index;
- (void)bindNullAtParameterIndex:(NSUInteger)index;

- (_WKSQLiteErrorCode)execute;
- (BOOL)execute:(NSError **)error;
- (nullable _WKWebExtensionSQLiteRowEnumerator *)fetch;
- (BOOL)fetchWithEnumerationBlock:(void (^)(_WKWebExtensionSQLiteRow *row, BOOL* stop))enumerationBlock error:(NSError **)error;

@property (readonly, nonatomic) NSArray *columnNames;
@property (readonly, nonatomic) NSDictionary *columnNamesToIndexes;

- (void)reset;
- (void)invalidate;

@property (readonly, nonatomic) sqlite3_stmt *handle;
@property (readonly, nonatomic) _WKWebExtensionSQLiteDatabase *database;

@end

NS_ASSUME_NONNULL_END
