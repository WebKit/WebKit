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
#import "APIData.h"
#import "CocoaHelpers.h"
#import "WKNSData.h"

namespace WebKit {

static NSString * const privacyPreservingDescriptionKey = @"privacyPreservingDescription";

template<>
NSArray *filterObjects<NSArray>(NSArray *array, bool NS_NOESCAPE (^block)(id key, id value))
{
    if (!array)
        return nil;

    switch (array.count) {
    case 0:
        return @[ ];

    case 1:
        return block(@(0), array[0]) ? [array copy] : @[ ];

    default:
        return [array objectsAtIndexes:[array indexesOfObjectsPassingTest:^BOOL (id object, NSUInteger index, BOOL *stop) {
            return block(@(index), object);
        }]];
    }
}

template<>
NSDictionary *filterObjects<NSDictionary>(NSDictionary *dictionary, bool NS_NOESCAPE (^block)(id key, id value))
{
    if (!dictionary)
        return nil;

    if (!dictionary.count)
        return @{ };

    NSMutableDictionary *filterResult = [NSMutableDictionary dictionaryWithCapacity:dictionary.count];

    [dictionary enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL *stop) {
        if (block(key, value))
            filterResult[key] = value;
    }];

    return [filterResult copy];
}

template<>
NSSet *filterObjects<NSSet>(NSSet *set, bool NS_NOESCAPE (^block)(id key, id value))
{
    if (!set)
        return nil;

    if (!set.count)
        return [NSSet set];

    return [set objectsPassingTest:^BOOL (id value, BOOL *stop) {
        return block(nil, value);
    }];
}

template<>
NSArray *mapObjects<NSArray>(NSArray *array, id NS_NOESCAPE (^block)(id key, id value))
{
    if (!array)
        return nil;

    switch (array.count) {
    case 0:
        return @[ ];

    case 1: {
        id result = block(@(0), array[0]);
        return result ? @[ result ] : @[ ];
    }

    default: {
        NSMutableArray *mapResult = [NSMutableArray arrayWithCapacity:array.count];

        [array enumerateObjectsUsingBlock:^(id value, NSUInteger index, BOOL *stop) {
            if (id result = block(@(index), value))
                [mapResult addObject:result];
        }];

        return [mapResult copy];
    }
    }
}

template<>
NSDictionary *mapObjects<NSDictionary>(NSDictionary *dictionary, id NS_NOESCAPE (^block)(id key, id value))
{
    if (!dictionary)
        return nil;

    if (!dictionary.count)
        return @{ };

    NSMutableDictionary *mapResult = [NSMutableDictionary dictionaryWithCapacity:dictionary.count];

    [dictionary enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL *stop) {
        if (id result = block(key, value))
            mapResult[key] = result;
    }];

    return [mapResult copy];
}

template<>
NSSet *mapObjects<NSSet>(NSSet *set, id NS_NOESCAPE (^block)(id key, id value))
{
    if (!set)
        return nil;

    switch (set.count) {
    case 0:
        return [NSSet set];

    case 1: {
        id result = block(nil, set.anyObject);
        return result ? [NSSet setWithObject:result] : [NSSet set];
    }

    default: {
        NSMutableSet *mapResult = [NSMutableSet setWithCapacity:set.count];

        [set enumerateObjectsUsingBlock:^(id value, BOOL *stop) {
            if (id result = block(nil, value))
                [mapResult addObject:result];
        }];

        return [mapResult copy];
    }
    }
}

template<>
NSString *objectForKey<NSString>(NSDictionary *dictionary, id key, bool nilIfEmpty, Class requiredClass)
{
    ASSERT(!requiredClass);
    NSString *result = dynamic_objc_cast<NSString>(dictionary[key]);
    return !nilIfEmpty || result.length ? result : nil;
}

template<>
NSArray *objectForKey<NSArray>(NSDictionary *dictionary, id key, bool nilIfEmpty, Class requiredClass)
{
    NSArray *result = dynamic_objc_cast<NSArray>(dictionary[key]);
    result = !nilIfEmpty || result.count ? result : nil;
    if (!result || !requiredClass)
        return result;
    return filterObjects(result, ^bool (id, id value) {
        return [value isKindOfClass:requiredClass];
    });
}

template<>
NSDictionary *objectForKey<NSDictionary>(NSDictionary *dictionary, id key, bool nilIfEmpty, Class requiredClass)
{
    NSDictionary *result = dynamic_objc_cast<NSDictionary>(dictionary[key]);
    result = !nilIfEmpty || result.count ? result : nil;
    if (!result || !requiredClass)
        return result;
    return filterObjects(result, ^bool (id, id value) {
        return [value isKindOfClass:requiredClass];
    });
}

template<>
NSSet *objectForKey<NSSet>(NSDictionary *dictionary, id key, bool nilIfEmpty, Class requiredClass)
{
    NSSet *result = dynamic_objc_cast<NSSet>(dictionary[key]);
    result = !nilIfEmpty || result.count ? result : nil;
    if (!result || !requiredClass)
        return result;
    return filterObjects(result, ^bool (id, id value) {
        return [value isKindOfClass:requiredClass];
    });
}

// MARK: JSON Helpers

static inline NSJSONReadingOptions toReadingImpl(JSONOptionSet options)
{
    NSJSONReadingOptions result = 0;
    if (options.contains(JSONOptions::FragmentsAllowed))
        result |= NSJSONReadingFragmentsAllowed;
    return result;
}

static inline NSJSONWritingOptions toWritingImpl(JSONOptionSet options)
{
    NSJSONWritingOptions result = 0;
    if (options.contains(JSONOptions::FragmentsAllowed))
        result |= NSJSONWritingFragmentsAllowed;
    return result;
}

bool isValidJSONObject(id object, JSONOptionSet options)
{
    if (!object)
        return false;

    if (options.contains(JSONOptions::FragmentsAllowed))
        return [object isKindOfClass:NSString.class] || [object isKindOfClass:NSNumber.class] || [object isKindOfClass:NSNull.class] || [NSJSONSerialization isValidJSONObject:object];

    // NSJSONSerialization allows top-level arrays, but we only support dictionaries when not using FragmentsAllowed.
    return [object isKindOfClass:NSDictionary.class] && [NSJSONSerialization isValidJSONObject:object];
}

id parseJSON(NSData *json, JSONOptionSet options, NSError **error)
{
    if (!json)
        return nil;

    id result = [NSJSONSerialization JSONObjectWithData:json options:toReadingImpl(options) error:error];
    if (options.contains(JSONOptions::FragmentsAllowed))
        return result;

    return dynamic_objc_cast<NSDictionary>(result);
}

id parseJSON(NSString *json, JSONOptionSet options, NSError **error)
{
    return parseJSON([json dataUsingEncoding:NSUTF8StringEncoding], options, error);
}

id parseJSON(API::Data& json, JSONOptionSet options, NSError **error)
{
    return parseJSON(wrapper(json), options, error);
}

NSString *encodeJSONString(id object, JSONOptionSet options, NSError **error)
{
    if (auto *data = encodeJSONData(object, options, error))
        return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    return nil;
}

NSData *encodeJSONData(id object, JSONOptionSet options, NSError **error)
{
    if (!object)
        return nil;

    ASSERT(isValidJSONObject(object, options));

    if (!options.contains(JSONOptions::FragmentsAllowed) && ![object isKindOfClass:NSDictionary.class])
        return nil;

    return [NSJSONSerialization dataWithJSONObject:object options:toWritingImpl(options) error:error];
}

// MARK: NSDictionary Helpers

NSDictionary *dictionaryWithLowercaseKeys(NSDictionary *dictionary)
{
    if (!dictionary.count)
        return @{ };

    NSMutableDictionary *newDictionary = [NSMutableDictionary dictionaryWithCapacity:dictionary.count];
    for (NSString *key in dictionary.allKeys) {
        if (![key isKindOfClass:NSString.class]) {
            ASSERT_NOT_REACHED();
            continue;
        }

        newDictionary[key.lowercaseString] = dictionary[key];
    }

    return [newDictionary copy];
}

NSDictionary *mergeDictionaries(NSDictionary *dictionaryA, NSDictionary *dictionaryB)
{
    if (!dictionaryB.count)
        return dictionaryA;

    if (!dictionaryA.count)
        return dictionaryB;

    NSMutableDictionary *mergedDictionary = [dictionaryA mutableCopy];

    for (id key in dictionaryB.allKeys) {
        if (!dictionaryA[key])
            mergedDictionary[key] = dictionaryB[key];
    }

    return mergedDictionary;
}

NSDictionary *mergeDictionariesAndSetValues(NSDictionary *dictionaryA, NSDictionary *dictionaryB)
{
    if (!dictionaryB.count)
        return dictionaryA;

    if (!dictionaryA.count)
        return dictionaryB;

    NSMutableDictionary *newDictionary = [dictionaryA mutableCopy];

    for (id key in dictionaryB.allKeys)
        newDictionary[key] = dictionaryB[key];

    return [newDictionary copy];
}

// MARK: NSError Helpers

NSString *privacyPreservingDescription(NSError *error)
{
    NSString *privacyPreservingDescription = objectForKey<NSString>(error.userInfo, privacyPreservingDescriptionKey);
    if (!privacyPreservingDescription) {
        NSString *domain = error.domain;
        if (domain.length) {
            id (^valueProvider)(NSError *err, NSString *userInfoKey) = [NSError userInfoValueProviderForDomain:domain];
            if (valueProvider)
                privacyPreservingDescription = valueProvider(error, privacyPreservingDescriptionKey);
        }
    }

    if (privacyPreservingDescription)
        return [NSString stringWithFormat:@"Error Domain=%@ Code=%ld \"%@\"", error.domain, (long)error.code, privacyPreservingDescription];

    return [NSError errorWithDomain:error.domain ?: @"" code:error.code userInfo:nil].description;
}

NSString *escapeCharactersInString(NSString *string, NSString *charactersToEscape)
{
    ASSERT(string);
    ASSERT(charactersToEscape);

    if (!string.length || !charactersToEscape.length)
        return string;

    NSCharacterSet *characterSet = [NSCharacterSet characterSetWithCharactersInString:charactersToEscape];
    NSRange range = [string rangeOfCharacterFromSet:characterSet];

    if (!range.length)
        return string;

    NSMutableString *result = [string mutableCopy];
    while (range.length > 0) {
        [result insertString:@"\\" atIndex:range.location];

        if (NSMaxRange(range) + 1 >= result.length)
            break;

        range = [result rangeOfCharacterFromSet:characterSet options:0 range:NSMakeRange(NSMaxRange(range) + 1, result.length - NSMaxRange(range) - 1)];
    }

    return result;
}

NSDate *toAPI(const WallTime& time)
{
    if (time.isNaN())
        return nil;

    if (time.isInfinity())
        return NSDate.distantFuture;

    return [NSDate dateWithTimeIntervalSince1970:time.secondsSinceEpoch().value()];
}

WallTime toImpl(NSDate *date)
{
    if (!date)
        return WallTime::nan();

    if ([date isEqualToDate:NSDate.distantFuture])
        return WallTime::infinity();

    return WallTime::fromRawSeconds(date.timeIntervalSince1970);
}

NSSet *toAPI(HashSet<String>& set)
{
    NSMutableSet *result = [[NSMutableSet alloc] initWithCapacity:set.size()];
    for (auto& element : set)
        [result addObject:static_cast<NSString *>(element)];

    return [result copy];
}

NSArray *toAPIArray(HashSet<String>& set)
{
    NSMutableArray *result = [[NSMutableArray alloc] initWithCapacity:set.size()];
    for (auto& element : set)
        [result addObject:static_cast<NSString *>(element)];

    return [result copy];
}

Vector<String> toImpl(NSArray *array)
{
    return Vector<String>(array.count, [array](size_t i) {
        return (NSString *)array[i];
    });
}

HashSet<String> toImplSet(NSArray *array)
{
    HashSet<String> result;
    result.reserveInitialCapacity(array.count);

    for (NSString *element in array)
        result.addVoid(element);

    return result;
}

} // namespace WebKit
