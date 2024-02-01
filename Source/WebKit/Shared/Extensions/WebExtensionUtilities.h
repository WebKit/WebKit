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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#import <JavaScriptCore/JSBase.h>

namespace WebKit {

/// Verifies that a dictionary:
///  - Contains a required set of string keys, as listed in `requiredKeys`, all other types specified in `keyTypes` are optional keys.
///  - Has values that are the appropriate type for each key, as specified by `keyTypes`. The keys in this dictionary
///    correspond to keys in the original dictionary being validated, and the values in `keyTypes` may be:
///     - A `Class`, that the value in the original dictionary must be a kind of.
///     - An `NSArray` containing one class, specifying that the value in the original dictionary must be an array with elements that are a kind of the specified class.
///     - An `NSOrderedSet` containing one or more classes or arrays, specifying the value in the dictionary should be of any classes in the set or their subclasses.
///  - The `callingAPIName` and `sourceKey` parameters are used to reference the object within a larger context. When an error occurs, this key helps identify the source of the problem in the `outExceptionString`.
/// If the dictionary is valid, returns `YES`. Otherwise returns `NO` and sets `outExceptionString` to a message describing what validation failed.
bool validateDictionary(NSDictionary<NSString *, id> *, NSString *sourceKey, NSArray<NSString *> *requiredKeys, NSDictionary<NSString *, id> *keyTypes, NSString **outExceptionString);

/// Verifies a single object against a certain type criteria:
///  - Checks that the object matches the type defined in `valueTypes`. The `valueTypes` can be:
///     - A `Class`, indicating the object should be of this class or its subclass.
///     - An `NSArray` containing one class, meaning the object must be an array with elements that are a kind of the specified class.
///     - An `NSOrderedSet` containing one or more classes or arrays, specifying that the object should be of any class in the set or their subclasses.
///  - The `callingAPIName` and `sourceKey` parameters are used to reference the object within a larger context. When an error occurs, this key helps identify the source of the problem in the `outExceptionString`.
/// If the object is valid, returns `YES`. Otherwise returns `NO` and sets `outExceptionString` to a message describing what validation failed.
bool validateObject(NSObject *, NSString *sourceKey, id valueTypes, NSString **outExceptionString);

/// Returns a concatenated error string that combines the provided information into a single, descriptive message.
NSString *toErrorString(NSString *callingAPIName, NSString *sourceKey, NSString *underlyingErrorString, ...) NS_FORMAT_ARGUMENT(3);

/// Returns an error object that combines the provided information into a single, descriptive message.
JSObjectRef toJSError(JSContextRef, NSString *callingAPIName, NSString *sourceKey, NSString *underlyingErrorString);

NSString *toWebAPI(NSLocale *);

/// This matches the maximum message length enforced by Chromium in its `MessageFromJSONString()` function.
constexpr size_t webExtensionMaxMessageLength = 1024 * 1024 * 64;

/// Returns the storage size of a string.
size_t storageSizeOf(NSString *);

/// Returns the storage size of all of the key value pairs in a dictionary.
size_t storageSizeOf(NSDictionary<NSString *, NSString *> *);

/// Returns true if the size of any item in the dictionary exceeds the given quota.
bool anyItemsExceedQuota(NSDictionary *, size_t quota, NSString **outKeyWithError = nullptr);

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
