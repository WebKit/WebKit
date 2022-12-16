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
#import "_WKWebExtensionWebNavigationURLFilter.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "_WKWebExtensionUtilities.h"
#import <WebKit/WebNSURLExtras.h>
#import <wtf/RetainPtr.h>

typedef NS_ENUM(NSInteger, PredicateType) {
    PredicateTypeHostContains,
    PredicateTypeHostEquals,
    PredicateTypeHostPrefix,
    PredicateTypeHostSuffix,
    PredicateTypePathContains,
    PredicateTypePathEquals,
    PredicateTypePathPrefix,
    PredicateTypePathSuffix,
    PredicateTypeQueryContains,
    PredicateTypeQueryEquals,
    PredicateTypeQueryPrefix,
    PredicateTypeQuerySuffix,
    PredicateTypeURLContains,
    PredicateTypeURLEquals,
    PredicateTypeURLMatches,
    PredicateTypeOriginAndPathMatches,
    PredicateTypeURLPrefix,
    PredicateTypeURLSuffix,
    PredicateTypeSchemes,
    PredicateTypePorts,
};

static NSString *urlKey = @"url";

static NSString *hostContainsKey = @"hostContains";
static NSString *hostEqualsKey = @"hostEquals";
static NSString *hostPrefixKey = @"hostPrefix";
static NSString *hostSuffixKey = @"hostSuffix";
static NSString *pathContainsKey = @"pathContains";
static NSString *pathEqualsKey = @"pathEquals";
static NSString *pathPrefixKey = @"pathPrefix";
static NSString *pathSuffixKey = @"pathSuffix";
static NSString *queryContainsKey = @"queryContains";
static NSString *queryEqualsKey = @"queryEquals";
static NSString *queryPrefixKey = @"queryPrefix";
static NSString *querySuffixKey = @"querySuffix";
static NSString *urlContainsKey = @"urlContains";
static NSString *urlEqualsKey = @"urlEquals";
static NSString *urlMatchesKey = @"urlMatches";
static NSString *originAndPathMatchesKey = @"originAndPathMatches";
static NSString *urlPrefixKey = @"urlPrefix";
static NSString *urlSuffixKey = @"urlSuffix";
static NSString *schemesKey = @"schemes";
static NSString *portsKey = @"ports";

@interface _WKWebExtensionWebNavigationURLPredicate : NSObject
- (instancetype)init NS_UNAVAILABLE;
@end

@implementation _WKWebExtensionWebNavigationURLPredicate {
    PredicateType _type;
    id _value;
}

- (instancetype)initWithTypeString:(NSString *)typeString value:(id)rawValue outErrorMessage:(NSString **)outErrorMessage
{
    if (!(self = [super init]))
        return nil;

    static NSDictionary<NSString *, NSNumber *> *stringToTypeMap = @{
        hostContainsKey: @(PredicateTypeHostContains),
        hostEqualsKey: @(PredicateTypeHostEquals),
        hostPrefixKey: @(PredicateTypeHostPrefix),
        hostSuffixKey: @(PredicateTypeHostSuffix),
        pathContainsKey: @(PredicateTypePathContains),
        pathEqualsKey: @(PredicateTypePathEquals),
        pathPrefixKey: @(PredicateTypePathPrefix),
        pathSuffixKey: @(PredicateTypePathSuffix),
        queryContainsKey: @(PredicateTypeQueryContains),
        queryEqualsKey: @(PredicateTypeQueryEquals),
        queryPrefixKey: @(PredicateTypeQueryPrefix),
        querySuffixKey: @(PredicateTypeQuerySuffix),
        urlContainsKey: @(PredicateTypeURLContains),
        urlEqualsKey: @(PredicateTypeURLEquals),
        urlMatchesKey: @(PredicateTypeURLMatches),
        originAndPathMatchesKey: @(PredicateTypeOriginAndPathMatches),
        urlPrefixKey: @(PredicateTypeURLPrefix),
        urlSuffixKey: @(PredicateTypeURLSuffix),
        schemesKey: @(PredicateTypeSchemes),
        portsKey: @(PredicateTypePorts),
    };

    NSNumber *typeAsNumber = stringToTypeMap[typeString];
    ASSERT(typeAsNumber);

    _type = (PredicateType)typeAsNumber.integerValue;

    switch (_type) {
    case PredicateTypeHostContains:
    case PredicateTypeHostEquals:
    case PredicateTypeHostPrefix:
    case PredicateTypeHostSuffix:
    case PredicateTypePathContains:
    case PredicateTypePathEquals:
    case PredicateTypePathPrefix:
    case PredicateTypePathSuffix:
    case PredicateTypeQueryContains:
    case PredicateTypeQueryEquals:
    case PredicateTypeQueryPrefix:
    case PredicateTypeQuerySuffix:
    case PredicateTypeURLContains:
    case PredicateTypeURLEquals:
    case PredicateTypeURLPrefix:
    case PredicateTypeURLSuffix:
        ASSERT([rawValue isKindOfClass:[NSString class]]);
        _value = [rawValue copy];
        break;

    case PredicateTypeURLMatches:
    case PredicateTypeOriginAndPathMatches:
        ASSERT([rawValue isKindOfClass:[NSString class]]);
        _value = [NSRegularExpression regularExpressionWithPattern:rawValue options:0 error:nil];
        if (!_value) {
            *outErrorMessage = [NSString stringWithFormat:@"\"%@\" is not a valid regular expression.", rawValue];
            return nil;
        }

        break;

    case PredicateTypeSchemes:
        ASSERT([rawValue isKindOfClass:[NSArray class]]);
        _value = [rawValue copy];
        break;

    case PredicateTypePorts: {
        NSMutableIndexSet *ports = [[NSMutableIndexSet alloc] init];
        ASSERT([rawValue isKindOfClass:[NSArray class]]);
        const NSInteger maximumPortNumber = 65535;
        for (id portOrRange in rawValue) {
            if (NSNumber *number = dynamic_objc_cast<NSNumber>(portOrRange)) {
                NSInteger integerValue = number.integerValue;
                if ([number isKindOfClass:@YES.class]) {
                    *outErrorMessage = [NSString stringWithFormat:@"Array elements of '%@\' filters must be integers or arrays.", typeString];
                    return nil;
                }

                if (integerValue < 0 || integerValue > maximumPortNumber) {
                    *outErrorMessage = [NSString stringWithFormat:@"%zd is not a valid port number.", integerValue];
                    return nil;
                }

                [ports addIndex:(NSUInteger)integerValue];
            } else if (NSArray<NSNumber *> *rangeArray = dynamic_objc_cast<NSArray>(portOrRange)) {
                if (rangeArray.count != 2) {
                    *outErrorMessage = @"Port range arrays must contain 2 integers.";
                    return nil;
                }

                for (NSNumber *number in rangeArray) {
                    if (![number isKindOfClass:NSNumber.class] || [number isKindOfClass:@YES.class]) {
                        *outErrorMessage = @"Port range arrays must contain 2 integers.";
                        return nil;
                    }

                    NSInteger integerValue = number.integerValue;
                    if (integerValue < 0 || integerValue > maximumPortNumber) {
                        *outErrorMessage = [NSString stringWithFormat:@"%zd is not a valid port number.", integerValue];
                        return nil;
                    }
                }

                NSUInteger firstPort = rangeArray[0].unsignedIntegerValue;
                NSUInteger lastPort = rangeArray[1].unsignedIntegerValue;
                if (firstPort > lastPort) {
                    *outErrorMessage = [NSString stringWithFormat:@"%zd-%zu is not a valid port range.", firstPort, lastPort];
                    return nil;
                }

                [ports addIndexesInRange:NSMakeRange(firstPort, lastPort - firstPort + 1)];
            } else {
                *outErrorMessage = [NSString stringWithFormat:@"Values in '%@' array must be integers or arrays of integers.", typeString];
                return nil;
            }
        }

        _value = [ports copy];
        break;
    }
    }

    return self;
}

- (BOOL)matchesURL:(NSURL *)url
{
    NSString *stringValue = _value;
    NSArray<NSString *> *stringArrayValue = _value;
    NSIndexSet *indexSetValue = _value;
    NSRegularExpression *regexValue = _value;

    switch (_type) {
    case PredicateTypeHostContains:
        // This filter type adds an implicit '.' before the URL's host.
        return [[@"." stringByAppendingString:url.host ?: @""] containsString:stringValue];
    case PredicateTypeHostEquals:
        return [url.host isEqualToString:stringValue];
    case PredicateTypeHostPrefix:
        return [url.host hasPrefix:stringValue];
    case PredicateTypeHostSuffix:
        return [url.host hasSuffix:stringValue];
    case PredicateTypePathContains:
        return [url.path containsString:stringValue];
    case PredicateTypePathEquals:
        return [url.path isEqualToString:stringValue];
    case PredicateTypePathPrefix:
        return [url.path hasPrefix:stringValue];
    case PredicateTypePathSuffix:
        return [url.path hasSuffix:stringValue];
    case PredicateTypeQueryContains:
        return [url.query containsString:stringValue];
    case PredicateTypeQueryEquals:
        return [url.query isEqualToString:stringValue];
    case PredicateTypeQueryPrefix:
        return [url.query hasPrefix:stringValue];
    case PredicateTypeQuerySuffix:
        return [url.query hasSuffix:stringValue];
    case PredicateTypeURLContains:
        return [url._webkit_URLByRemovingFragment.absoluteString containsString:stringValue];
    case PredicateTypeURLEquals:
        return [url._webkit_URLByRemovingFragment.absoluteString isEqualToString:stringValue];
    case PredicateTypeURLMatches: {
        NSString *stringForMatching = url._webkit_URLByRemovingFragment.absoluteString;
        return [regexValue rangeOfFirstMatchInString:stringForMatching options:0 range:NSMakeRange(0, stringForMatching.length)].location != NSNotFound;
    }
    case PredicateTypeOriginAndPathMatches: {
        NSURLComponents *components = [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
        components.fragment = nil;
        components.query = nil;
        NSString *stringForMatching = components.string;
        return [regexValue rangeOfFirstMatchInString:stringForMatching options:0 range:NSMakeRange(0, stringForMatching.length)].location != NSNotFound;
    }
    case PredicateTypeURLPrefix:
        return [url._webkit_URLByRemovingFragment.absoluteString hasPrefix:stringValue];
    case PredicateTypeURLSuffix:
        return [url._webkit_URLByRemovingFragment.absoluteString hasSuffix:stringValue];
    case PredicateTypeSchemes:
        return [stringArrayValue containsObject:url.scheme];
    case PredicateTypePorts:
        return [indexSetValue containsIndex:url.port.unsignedIntegerValue];
    }

    ASSERT_NOT_REACHED();
    return NO;
}

@end

@implementation _WKWebExtensionWebNavigationURLFilter {
    NSArray<NSArray<_WKWebExtensionWebNavigationURLPredicate *> *> *_predicateGroups;
}

- (instancetype)initWithDictionary:(NSDictionary<NSString *, id> *)dictionary outErrorMessage:(NSString **)outErrorMessage
{
    static NSArray<NSString *> *requiredKeys = @[
        urlKey,
    ];

    static NSDictionary<NSString *, id> *types = @{
        urlKey: @[ [NSDictionary class] ],
    };

    if (![_WKWebExtensionUtilities validateContentsOfDictionary:dictionary requiredKeys:requiredKeys optionalKeys:nil keyToExpectedValueType:types outExceptionString:outErrorMessage])
        return nil;

    static NSArray<NSString *> *optionalURLDictionaryKeys = @[
        hostContainsKey,
        hostEqualsKey,
        hostPrefixKey,
        hostSuffixKey,
        pathContainsKey,
        pathEqualsKey,
        pathPrefixKey,
        pathSuffixKey,
        queryContainsKey,
        queryEqualsKey,
        queryPrefixKey,
        querySuffixKey,
        urlContainsKey,
        urlEqualsKey,
        urlMatchesKey,
        originAndPathMatchesKey,
        urlPrefixKey,
        urlSuffixKey,
        schemesKey,
        portsKey,
    ];

    static NSDictionary<NSString *, id> *URLDictionaryTypes = @{
        hostContainsKey: [NSString class],
        hostEqualsKey: [NSString class],
        hostPrefixKey: [NSString class],
        hostSuffixKey: [NSString class],
        pathContainsKey: [NSString class],
        pathEqualsKey: [NSString class],
        pathPrefixKey: [NSString class],
        pathSuffixKey: [NSString class],
        queryContainsKey: [NSString class],
        queryEqualsKey: [NSString class],
        queryPrefixKey: [NSString class],
        querySuffixKey: [NSString class],
        urlContainsKey: [NSString class],
        urlEqualsKey: [NSString class],
        urlMatchesKey: [NSString class],
        originAndPathMatchesKey: [NSString class],
        urlPrefixKey: [NSString class],
        urlSuffixKey: [NSString class],
        schemesKey: @[ [NSString class] ],
        portsKey: [NSArray class], // Array of (integer or (array of integer))
    };

    NSMutableArray<NSArray<_WKWebExtensionWebNavigationURLPredicate *> *> *predicateGroups = [[NSMutableArray alloc] init];

    for (NSDictionary<NSString *, id> *urlDictionary in dictionary[urlKey]) {
        if (![_WKWebExtensionUtilities validateContentsOfDictionary:urlDictionary requiredKeys:nil optionalKeys:optionalURLDictionaryKeys keyToExpectedValueType:URLDictionaryTypes outExceptionString:outErrorMessage])
            return nil;

        NSMutableArray<_WKWebExtensionWebNavigationURLPredicate *> *predicates = [[NSMutableArray alloc] init];

        __block NSString *errorMessage;
        [urlDictionary enumerateKeysAndObjectsUsingBlock:^(NSString *key, id obj, BOOL* stop) {
            _WKWebExtensionWebNavigationURLPredicate *predicate = [[_WKWebExtensionWebNavigationURLPredicate alloc] initWithTypeString:key value:obj outErrorMessage:&errorMessage];
            if (predicate)
                [predicates addObject:predicate];
            else
                *stop = YES;
        }];

        if (errorMessage) {
            *outErrorMessage = errorMessage;
            return nil;
        }

        [predicateGroups addObject:predicates];
    }

    if (!(self = [super init]))
        return nil;

    _predicateGroups = predicateGroups.count ? predicateGroups : nil;

    return self;
}

- (BOOL)matchesURL:(NSURL *)url
{
    if (!_predicateGroups)
        return YES;

    for (NSArray<_WKWebExtensionWebNavigationURLPredicate *> *predicateGroup in _predicateGroups) {
        BOOL allPredicatesInGroupMatch = YES;
        for (_WKWebExtensionWebNavigationURLPredicate *predicate in predicateGroup) {
            if (![predicate matchesURL:url]) {
                allPredicatesInGroupMatch = NO;
                break;
            }
        }

        if (allPredicatesInGroupMatch)
            return YES;
    }

    return NO;
}

@end

#else

@implementation _WKWebExtensionWebNavigationURLFilter

- (nullable instancetype)initWithDictionary:(NSDictionary<NSString *, id> *)dictionary outErrorMessage:(NSString **)outErrorMessage
{
    return nil;
}

- (BOOL)matchesURL:(NSURL *)url
{
    return NO;
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
