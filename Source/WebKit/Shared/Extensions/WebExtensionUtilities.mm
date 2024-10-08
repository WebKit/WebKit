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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionUtilities.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "JSWebExtensionWrapper.h"
#import "Logging.h"
#import "WebExtensionAPITabs.h"
#import "WebExtensionMessageSenderParameters.h"
#import <objc/runtime.h>

namespace WebKit {

static NSString *classToClassString(Class classType, bool plural = false)
{
    static NSMapTable<Class, NSString *> *classTypeToSingularClassString;
    static NSMapTable<Class, NSString *> *classTypeToPluralClassString;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        classTypeToSingularClassString = [NSMapTable strongToStrongObjectsMapTable];
        [classTypeToSingularClassString setObject:@"a boolean" forKey:@YES.class];
        [classTypeToSingularClassString setObject:@"a number" forKey:NSNumber.class];
        [classTypeToSingularClassString setObject:@"a string" forKey:NSString.class];
        [classTypeToSingularClassString setObject:@"a value" forKey:JSValue.class];
        [classTypeToSingularClassString setObject:@"null" forKey:NSNull.class];
        [classTypeToSingularClassString setObject:@"an array" forKey:NSArray.class];
        [classTypeToSingularClassString setObject:@"an object" forKey:NSDictionary.class];

        classTypeToPluralClassString = [NSMapTable strongToStrongObjectsMapTable];
        [classTypeToPluralClassString setObject:@"booleans" forKey:@YES.class];
        [classTypeToPluralClassString setObject:@"numbers" forKey:NSNumber.class];
        [classTypeToPluralClassString setObject:@"strings" forKey:NSString.class];
        [classTypeToPluralClassString setObject:@"values" forKey:JSValue.class];
        [classTypeToPluralClassString setObject:@"null values" forKey:NSNull.class];
        [classTypeToPluralClassString setObject:@"arrays" forKey:NSArray.class];
        [classTypeToPluralClassString setObject:@"objects" forKey:NSDictionary.class];
    });

    NSMapTable<Class, NSString *> *classTypeToClassString = plural ? classTypeToPluralClassString : classTypeToSingularClassString;

    if (NSString *result = [classTypeToClassString objectForKey:classType])
        return result;

    for (Class superclass in classTypeToClassString) {
        if ([classType isSubclassOfClass:superclass])
            return [classTypeToClassString objectForKey:superclass];
    }

    ASSERT_NOT_REACHED();
    return @"unknown";
}

static NSString *valueToTypeString(NSObject *value, bool plural = false)
{
    if (auto *number = dynamic_objc_cast<NSNumber>(value)) {
        if (std::isnan(number.doubleValue))
            return plural ? @"NaN values" : @"NaN";

        if (std::isinf(number.doubleValue))
            return plural ? @"Infinity values" : @"Infinity";
    } else if (auto *scriptValue = dynamic_objc_cast<JSValue>(value)) {
        if (scriptValue.isUndefined)
            return plural ? @"undefined values" : @"undefined";

        if (scriptValue._isRegularExpression)
            return plural ? @"regular expressions" : @"a regular expression";

        if (scriptValue._isThenable)
            return plural ? @"promises" : @"a promise";

        if (scriptValue._isFunction)
            return plural ? @"functions" : @"a function";
    }

    return classToClassString(value.class, plural);
}

static NSString *constructExpectedMessage(NSString *key, NSString *expected, NSString *found, bool inArray = false)
{
    ASSERT(expected);
    ASSERT(found);

    if (inArray && key.length)
        return [NSString stringWithFormat:@"'%@' is expected to be %@, but %@ was provided in the array", key, expected, found];
    if (inArray && !key.length)
        return [NSString stringWithFormat:@"%@ is expected, but %@ was provided in the array", expected, found];

    if (key.length)
        return [NSString stringWithFormat:@"'%@' is expected to be %@, but %@ was provided", key, expected, found];
    return [NSString stringWithFormat:@"%@ is expected, but %@ was provided", expected, found];
}

static bool validateSingleObject(NSString *key, NSObject *value, Class expectedValueType, NSString **outExceptionString)
{
    ASSERT([expectedValueType respondsToSelector:@selector(isSubclassOfClass:)]);

    auto *number = dynamic_objc_cast<NSNumber>(value);

    if (number && std::isnan(number.doubleValue)) {
        if (outExceptionString)
            *outExceptionString = constructExpectedMessage(key, classToClassString(expectedValueType), valueToTypeString(value));
        return false;
    }

    if (number && std::isinf(number.doubleValue)) {
        if (outExceptionString)
            *outExceptionString = constructExpectedMessage(key, classToClassString(expectedValueType), valueToTypeString(value));
        return false;
    }

    if (![value isKindOfClass:expectedValueType]) {
        if (outExceptionString)
            *outExceptionString = constructExpectedMessage(key, classToClassString(expectedValueType), valueToTypeString(value));
        return false;
    }

    if (number && expectedValueType != @YES.class && [number isKindOfClass:@YES.class]) {
        if (outExceptionString)
            *outExceptionString = constructExpectedMessage(key, classToClassString(expectedValueType), valueToTypeString(value));
        return false;
    }

    return true;
}

static bool validateArray(NSString *key, NSObject *value, NSArray<Class> *validClassesArray, NSString **outExceptionString)
{
    ASSERT(validClassesArray.count == 1);

    Class expectedElementType = validClassesArray.firstObject;
    NSArray *arrayValue = dynamic_objc_cast<NSArray>(value);
    if (!arrayValue) {
        if (outExceptionString)
            *outExceptionString = constructExpectedMessage(key, [NSString stringWithFormat:@"an array of %@", classToClassString(expectedElementType, true)], valueToTypeString(value));
        return false;
    }

    for (NSObject *element in arrayValue) {
        if (!validateSingleObject(nil, element, expectedElementType, nullptr)) {
            if (outExceptionString)
                *outExceptionString = constructExpectedMessage(key, [NSString stringWithFormat:@"an array of %@", classToClassString(expectedElementType, true)], valueToTypeString(element), true);
            return false;
        }
    }

    return true;
}

static bool validateSet(NSString *key, NSObject *value, NSOrderedSet *validClassesSet, NSString **outExceptionString)
{
    if ([validClassesSet containsObject:value.class])
        return true;

    __block NSMutableString *expectedValuesString = [NSMutableString string];

    auto addExpectedValueString = ^(Class expectedClass, Class expectedSubclass) {
        if (expectedValuesString.length)
            [expectedValuesString appendString:@" or "];

        [expectedValuesString appendString:classToClassString(expectedClass)];

        if (!expectedSubclass)
            return;

        [expectedValuesString appendString:@" of "];
        [expectedValuesString appendString:classToClassString(expectedSubclass, true)];
    };

    bool arrayIsValid = false;
    for (id classOrArray in validClassesSet) {
        if ([classOrArray respondsToSelector:@selector(isSubclassOfClass:)]) {
            ASSERT(classOrArray != NSArray.class);

            addExpectedValueString(classOrArray, Nil);

            if (validateSingleObject(key, value, classOrArray, nullptr))
                return true;

            continue;
        }

        ASSERT([classOrArray isKindOfClass:NSArray.class]);
        arrayIsValid = true;

        addExpectedValueString(NSArray.class, [classOrArray firstObject]);
        if (validateArray(key, value, classOrArray, nullptr))
            return true;
    }

    if (outExceptionString) {
        auto *foundString = arrayIsValid && [value isKindOfClass:NSArray.class] ? @"an array of other values" : valueToTypeString(value);
        *outExceptionString = constructExpectedMessage(key, expectedValuesString, foundString);
    }

    return false;
};

static bool validate(NSString *key, NSObject *value, id expectedValueType, NSString **outExceptionString)
{
    if (NSArray<Class> *validClassesArray = dynamic_objc_cast<NSArray>(expectedValueType))
        return validateArray(key, value, validClassesArray, outExceptionString);

    if (NSOrderedSet *validClassesSet = dynamic_objc_cast<NSOrderedSet>(expectedValueType))
        return validateSet(key, value, validClassesSet, outExceptionString);

    return validateSingleObject(key, value, expectedValueType, outExceptionString);
}

static NSString *formatList(NSArray<NSString *> *list)
{
    auto count = list.count;
    if (!count)
        return @"";

    if (count == 1)
        return [NSString stringWithFormat:@"'%@'", list.firstObject];

    if (count == 2)
        return [NSString stringWithFormat:@"'%@' and '%@'", list.firstObject, list.lastObject];

    auto *allButLast = [list subarrayWithRange:NSMakeRange(0, count - 1)];
    auto *formattedInitialItems = [allButLast componentsJoinedByString:@"', '"];
    return [NSString stringWithFormat:@"'%@', and '%@'", formattedInitialItems, list.lastObject];
}

bool validateDictionary(NSDictionary<NSString *, id> *dictionary, NSString *sourceKey, NSArray<NSString *> *requiredKeys, NSDictionary<NSString *, id> *keyTypes, NSString **outExceptionString)
{
    ASSERT(keyTypes.count);

    NSMutableSet<NSString *> *remainingRequiredKeys = [[NSMutableSet alloc] initWithArray:requiredKeys];
    NSSet<NSString *> *requiredKeysSet = [[NSSet alloc] initWithArray:requiredKeys];

    NSMutableSet<NSString *> *optionalKeysSet = [[NSMutableSet alloc] initWithArray:keyTypes.allKeys];
    [optionalKeysSet minusSet:requiredKeysSet];

    __block NSString *errorString;

    [dictionary enumerateKeysAndObjectsUsingBlock:^(NSString *key, NSObject *value, BOOL* stop) {
        // This should never be hit, since the dictionary comes from JavaScript and all keys are strings.
        ASSERT([key isKindOfClass:NSString.class]);

        if (![requiredKeysSet containsObject:key] && ![optionalKeysSet containsObject:key])
            return;

        id expectedValueType = keyTypes[key];
        ASSERT(expectedValueType);

        if (!validate(key, value, expectedValueType, &errorString)) {
            *stop = YES;
            return;
        }

        if ([requiredKeysSet containsObject:key])
            [remainingRequiredKeys removeObject:key];
    }];

    // Prioritize type errors over missing required key errors, since the dictionary *might* actually have
    // all the required keys, but we stopped checking. We do know for sure that the type is wrong though.
    if (remainingRequiredKeys.count && !errorString)
        errorString = [NSString stringWithFormat:@"it is missing required keys: %@", formatList(remainingRequiredKeys.allObjects)];

    if (errorString && outExceptionString)
        *outExceptionString = toErrorString(nil, sourceKey, errorString);

    return !errorString;
}

bool validateObject(NSObject *object, NSString *sourceKey, id expectedValueType, NSString **outExceptionString)
{
    ASSERT(expectedValueType);

    NSString *errorString;
    validate(nil, object, expectedValueType, &errorString);

    if (errorString && outExceptionString)
        *outExceptionString = toErrorString(nil, sourceKey, errorString);

    return !errorString;
}

static inline NSString* lowercaseFirst(NSString *input)
{
    return input.length ? [[input substringToIndex:1].lowercaseString stringByAppendingString:[input substringFromIndex:1]] : input;
}

static inline NSString* uppercaseFirst(NSString *input)
{
    return input.length ? [[input substringToIndex:1].uppercaseString stringByAppendingString:[input substringFromIndex:1]] : input;
}

inline NSString* trimTrailingPeriod(NSString *input)
{
    return [input hasSuffix:@"."] ? [input substringToIndex:input.length - 1] : input;
}

NSString *toErrorString(NSString *callingAPIName, NSString *sourceKey, NSString *underlyingErrorString, ...)
{
    ASSERT(underlyingErrorString.length);

    va_list arguments;
    va_start(arguments, underlyingErrorString);

    ALLOW_NONLITERAL_FORMAT_BEGIN
    NSString *formattedUnderlyingErrorString = trimTrailingPeriod([[NSString alloc] initWithFormat:underlyingErrorString arguments:arguments]);
    ALLOW_NONLITERAL_FORMAT_END

    va_end(arguments);

    if (UNLIKELY(callingAPIName.length && sourceKey.length && [formattedUnderlyingErrorString containsString:@"value is invalid"])) {
        ASSERT_NOT_REACHED_WITH_MESSAGE("Overly nested error string, use a `nil` sourceKey for this call instead.");
        sourceKey = nil;
    }

    if (callingAPIName.length && sourceKey.length)
        return [NSString stringWithFormat:@"Invalid call to %@. The '%@' value is invalid, because %@.", callingAPIName, sourceKey, lowercaseFirst(formattedUnderlyingErrorString)];

    if (!callingAPIName.length && sourceKey.length)
        return [NSString stringWithFormat:@"The '%@' value is invalid, because %@.", sourceKey, lowercaseFirst(formattedUnderlyingErrorString)];

    if (callingAPIName.length)
        return [NSString stringWithFormat:@"Invalid call to %@. %@.", callingAPIName, uppercaseFirst(formattedUnderlyingErrorString)];

    return formattedUnderlyingErrorString;
}

JSObjectRef toJSError(JSContextRef context, NSString *callingAPIName, NSString *sourceKey, NSString *underlyingErrorString)
{
    return toJSError(context, toErrorString(callingAPIName, sourceKey, underlyingErrorString));
}

JSObjectRef toJSRejectedPromise(JSContextRef context, NSString *callingAPIName, NSString *sourceKey, NSString *underlyingErrorString)
{
    auto *error = toJSValue(context, toJSError(context, callingAPIName, sourceKey, underlyingErrorString));
    auto *promise = [JSValue valueWithNewPromiseRejectedWithReason:error inContext:toJSContext(context)];
    return JSValueToObject(context, promise.JSValueRef, nullptr);
}

NSString *toWebAPI(NSLocale *locale)
{
    if (!locale.languageCode.length)
        return @"und";

    if (locale.countryCode.length)
        return [NSString stringWithFormat:@"%@-%@", locale.languageCode, locale.countryCode];
    return locale.languageCode;
}

size_t storageSizeOf(NSString *keyOrValue)
{
    // The size of a key is the length of the key string.
    // The size of a value is the length of the JSON representation of the value.
    // If keyOrValue represents a value we assume that the string is the JSON representation.
    return [keyOrValue lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
}

size_t storageSizeOf(NSDictionary<NSString *, NSString *> *keysAndValues)
{
    __block double storageSize = 0;
    [keysAndValues enumerateKeysAndObjectsUsingBlock:^(NSString *key, NSString *value, BOOL *stop) {
        storageSize += storageSizeOf(key) + storageSizeOf(value);
    }];

    return storageSize;
}

bool anyItemsExceedQuota(NSDictionary *items, size_t quota, NSString **outKeyWithError)
{
    __block bool itemExceededQuota = false;
    __block NSString *keyWithError;

    [items enumerateKeysAndObjectsUsingBlock:^(NSString *key, NSString *value, BOOL *stop) {
        size_t sizeOfCurrentItem = storageSizeOf(key) + storageSizeOf(value);
        if (sizeOfCurrentItem > quota) {
            itemExceededQuota = true;
            keyWithError = key;
            *stop = YES;
        }
    }];

    ASSERT(!itemExceededQuota || (itemExceededQuota && keyWithError));

    if (outKeyWithError)
        *outKeyWithError = keyWithError;

    return itemExceededQuota;
}

Markable<WTF::UUID> toDocumentIdentifier(WebFrame& frame)
{
    RefPtr coreFrame = frame.coreLocalFrame();
    RefPtr document = coreFrame ? coreFrame->document() : nullptr;
    if (!document)
        return { };
    return document->identifier().object();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
