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
#import "_WKWebExtensionUtilities.h"

#import "CocoaHelpers.h"

@implementation _WKWebExtensionUtilities

#if ENABLE(WK_WEB_EXTENSIONS)

static NSString *classToClassString(Class classType, BOOL plural = NO)
{
    static NSMapTable<Class, NSString *> *classTypeToSingularClassString;
    static NSMapTable<Class, NSString *> *classTypeToPluralClassString;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        classTypeToSingularClassString = [NSMapTable strongToStrongObjectsMapTable];
        [classTypeToSingularClassString setObject:@"a boolean value" forKey:[@YES class]];
        [classTypeToSingularClassString setObject:@"a number value" forKey:[NSNumber class]];
        [classTypeToSingularClassString setObject:@"a string value" forKey:[NSString class]];
        [classTypeToSingularClassString setObject:@"an array" forKey:[NSArray class]];
        [classTypeToSingularClassString setObject:@"an object" forKey:[NSDictionary class]];

        classTypeToPluralClassString = [NSMapTable strongToStrongObjectsMapTable];
        [classTypeToPluralClassString setObject:@"boolean values" forKey:[@YES class]];
        [classTypeToPluralClassString setObject:@"number values" forKey:[NSNumber class]];
        [classTypeToPluralClassString setObject:@"string values" forKey:[NSString class]];
        [classTypeToPluralClassString setObject:@"arrays" forKey:[NSArray class]];
        [classTypeToPluralClassString setObject:@"objects" forKey:[NSDictionary class]];
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

+ (BOOL)validateContentsOfDictionary:(NSDictionary<NSString *, id> *)dictionary requiredKeys:(NSArray<NSString *> *)requiredKeys optionalKeys:(NSArray<NSString *> *)optionalKeys keyToExpectedValueType:(NSDictionary<NSString *, id> *)keyTypes outExceptionString:(NSString **)outExceptionString
{
    NSMutableSet<NSString *> *remainingRequiredKeys = [[NSMutableSet alloc] initWithArray:requiredKeys];
    NSSet<NSString *> *requiredKeysSet = [[NSSet alloc] initWithArray:requiredKeys];
    NSSet<NSString *> *optionalKeysSet = [[NSSet alloc] initWithArray:optionalKeys];

    __block NSString *errorString;
    [dictionary enumerateKeysAndObjectsUsingBlock:^(NSString *key, NSObject *value, BOOL* stop) {
        // This should never be hit, since the dictionary comes from JavaScript and all keys are strings.
        ASSERT([key isKindOfClass:[NSString class]]);

        if (![requiredKeysSet containsObject:key] && ![optionalKeysSet containsObject:key]) {
            // FIXME: Add logging here?
            return;
        }

        id expectedValueType = keyTypes[key];
        NSArray<Class> *validClassesArray = dynamic_objc_cast<NSArray>(expectedValueType);
        if (validClassesArray) {
            ASSERT(validClassesArray.count == 1UL);
            Class expectedElementType = validClassesArray.firstObject;
            NSArray<id> *arrayValue = dynamic_objc_cast<NSArray>(value);
            if (!arrayValue) {
                *stop = YES;
                errorString = [NSString stringWithFormat:@"Expected an array for '%@'.", key];
                return;
            }

            for (NSObject *element in arrayValue) {
                if (![element isKindOfClass:expectedElementType]) {
                    *stop = YES;
                    errorString = [NSString stringWithFormat:@"Expected %@ in the array for '%@', found %@ instead.", classToClassString(expectedElementType, YES), key, classToClassString(element.class)];
                    return;
                }
            }
        } else if (![value isKindOfClass:expectedValueType]) {
            *stop = YES;
            errorString = [NSString stringWithFormat:@"Expected %@ for '%@', found %@ instead.", classToClassString(expectedValueType), key, classToClassString(value.class)];
            return;
        }

        if ([requiredKeysSet containsObject:key])
            [remainingRequiredKeys removeObject:key];
    }];

    // Prioritize type errors over missing required key errors, since the dictionary *might* actually have
    // all the required keys, but we stopped checking. We do know for sure that the type is wrong though.
    if (remainingRequiredKeys.count && !errorString) {
        NSString *missingRequiredKeys = [[remainingRequiredKeys allObjects] componentsJoinedByString:@", "];
        errorString = [NSString stringWithFormat:@"Missing required keys: %@.", missingRequiredKeys];
    }

    if (errorString)
        *outExceptionString = errorString;
    return !errorString;
}


#else

+ (BOOL)validateContentsOfDictionary:(NSDictionary<NSString *, id> *)dictionary requiredKeys:(NSArray<NSString *> *)requiredKeys optionalKeys:(NSArray<NSString *> *)optionalKeys keyToExpectedValueType:(NSDictionary<NSString *, id> *)keyTypes outExceptionString:(NSString **)outExceptionString
{
    return NO;
}

#endif // ENABLE(WEB_EXTENSIONS)

@end
