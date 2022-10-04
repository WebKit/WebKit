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
#import "CocoaHelpers.h"

namespace WebKit {

template<>
NSArray *filterObjects<NSArray>(NSArray *array, bool NS_NOESCAPE (^block)(__kindof id key, __kindof id value))
{
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
NSDictionary *filterObjects<NSDictionary>(NSDictionary *dictionary, bool NS_NOESCAPE (^block)(__kindof id key, __kindof id value))
{
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
NSSet *filterObjects<NSSet>(NSSet *set, bool NS_NOESCAPE (^block)(__kindof id key, __kindof id value))
{
    if (!set.count)
        return [NSSet set];

    return [set objectsPassingTest:^BOOL (id value, BOOL *stop) {
        return block(nil, value);
    }];
}

template<>
NSArray *mapObjects<NSArray>(NSArray *array, __kindof id NS_NOESCAPE (^block)(__kindof id key, __kindof id value))
{
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
NSDictionary *mapObjects<NSDictionary>(NSDictionary *dictionary, __kindof id NS_NOESCAPE (^block)(__kindof id key, __kindof id value))
{
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
NSSet *mapObjects<NSSet>(NSSet *set, __kindof id NS_NOESCAPE (^block)(__kindof id key, __kindof id value))
{
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

} // namespace WebKit
