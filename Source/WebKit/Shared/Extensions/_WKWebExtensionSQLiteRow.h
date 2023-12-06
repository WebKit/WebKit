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

@class _WKWebExtensionSQLiteRowEnumerator;
@class _WKWebExtensionSQLiteStatement;

typedef int _WKSQLiteErrorCode;

@interface _WKWebExtensionSQLiteRow : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithStatement:(_WKWebExtensionSQLiteStatement *)statement NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCurrentRowOfEnumerator:(_WKWebExtensionSQLiteRowEnumerator *)rowEnumerator;

- (nullable NSString *)stringAtIndex:(NSUInteger)index;
- (int)intAtIndex:(NSUInteger)index;
- (int64_t)int64AtIndex:(NSUInteger)index;
- (double)doubleAtIndex:(NSUInteger)index;
- (BOOL)boolAtIndex:(NSUInteger)index;
- (nullable NSData *)dataAtIndex:(NSUInteger)index;
- (nullable NSObject *)objectAtIndex:(NSUInteger)index;

- (nullable NSData *)uncopiedDataAtIndex:(NSUInteger)index;

struct RawData {
    const bool isNull;
    const void* bytes;
    const int length;
};

- (struct RawData)uncopiedRawDataAtIndex:(NSUInteger)index;

@end

@interface _WKWebExtensionSQLiteRowEnumerator : NSEnumerator

- (instancetype)initWithResultsOfStatement:(_WKWebExtensionSQLiteStatement *)statement;

@property (readonly, nonatomic) _WKWebExtensionSQLiteStatement *statement;
@property (readonly, nonatomic) _WKSQLiteErrorCode lastResultCode;

@end

NS_ASSUME_NONNULL_END
