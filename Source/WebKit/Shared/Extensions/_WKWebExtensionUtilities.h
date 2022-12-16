/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import <WebKit/WKFoundation.h>

NS_ASSUME_NONNULL_BEGIN

WK_EXTERN
@interface _WKWebExtensionUtilities : NSObject

/// Verifies that a dictionary:
///  - Contains a required set of string keys, as listed in `requiredKeys`.
///  - Has no unexpected keys beyond the required keys and an additional set of optional keys, listed in `optionalKeys`.
///  - Has values that are the appropriate type for each key, as specified by `keyTypes`. The keys in this dictionary
///    correspond to keys in the original dictionary being validated, and the values in `keyTypes` may be:
///     - An Objective-C class, that the value in the original dictionary must be a kind of.
///     - An array containing one class, specifying that the value in the original dictionary must be an array with
///      elements that are a kind of the specified class.
/// If the dictionary is valid, returns YES. Otherwise returns NO and sets `outExceptionString` to a message describing
/// what validation failed.
+ (BOOL)validateContentsOfDictionary:(NSDictionary<NSString *, id> *)dictionary requiredKeys:(nullable NSArray<NSString *> *)requiredKeys optionalKeys:(nullable NSArray<NSString *> *)optionalKeys keyToExpectedValueType:(NSDictionary<NSString *, id> *)keyTypes outExceptionString:(NSString * _Nullable * _Nonnull)outExceptionString;

@end

NS_ASSUME_NONNULL_END
