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

#pragma once

#import "_WKWebExtensionSQLiteStore.h"

NS_ASSUME_NONNULL_BEGIN

namespace WebKit {
enum class WebExtensionDataType : uint8_t;
}

@interface _WKWebExtensionStorageSQLiteStore : _WKWebExtensionSQLiteStore

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)initWithUniqueIdentifier:(NSString *)uniqueIdentifier storageType:(WebKit::WebExtensionDataType)storageType directory:(NSString *)directory usesInMemoryDatabase:(BOOL)useInMemoryDatabase;

- (void)getValuesForKeys:(NSArray<NSString *> *)keys completionHandler:(void (^)(NSDictionary<NSString *, NSString *> *results, NSString * _Nullable errorMessage))completionHandler;
- (void)getStorageSizeForKeys:(NSArray<NSString *> *)keys completionHandler:(void (^)(size_t storageSize, NSString * _Nullable errorMessage))completionHandler;
- (void)getStorageSizeForAllKeysIncludingKeyedData:(NSDictionary<NSString *, NSString *> *)additionalKeyedData withCompletionHandler:(void (^)(size_t storageSize, NSUInteger numberOfKeysIncludingAdditionalKeyedData, NSDictionary<NSString *, NSString *> *existingKeysAndValues, NSString * _Nullable errorMessage))completionHandler;
- (void)setKeyedData:(NSDictionary<NSString *, NSString *> *)keyedData completionHandler:(void (^)(NSArray<NSString *> * _Nullable keysSuccessfullySet, NSString * _Nullable errorMessage))completionHandler;
- (void)deleteValuesForKeys:(NSArray<NSString *> *)keys completionHandler:(void (^)(NSString * _Nullable errorMessage))completionHandler;

@end

NS_ASSUME_NONNULL_END
