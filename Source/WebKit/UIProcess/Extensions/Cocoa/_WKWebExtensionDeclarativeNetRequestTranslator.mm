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

#import "config.h"
#import "_WKWebExtensionDeclarativeNetRequestTranslator.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "_WKWebExtensionDeclarativeNetRequestRule.h"

NSString * const currentDeclarativeNetRequestHashVersion = @"3";

static const NSUInteger maximumNumberOfDeclarativeNetRequestErrorsToSurface = 50;

using namespace WebKit;

@implementation _WKWebExtensionDeclarativeNetRequestTranslator

+ (NSArray<NSDictionary<NSString *, id> *> *)translateRules:(NSArray<NSArray<NSDictionary *> *> *)jsonObjects errorStrings:(NSArray **)outErrorStrings
{
    NSMutableArray<_WKWebExtensionDeclarativeNetRequestRule *> *allValidatedRules = [NSMutableArray array];
    NSMutableArray<NSString *> *errorStrings = [NSMutableArray array];
    NSUInteger totalErrorCount = 0;
    for (NSArray *json in jsonObjects) {
        for (NSDictionary *ruleJSON in json) {
            NSString *errorString;
            _WKWebExtensionDeclarativeNetRequestRule *rule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:ruleJSON errorString:&errorString];

            if (rule)
                [allValidatedRules addObject:rule];
            else if (errorString) {
                totalErrorCount++;

                if (errorStrings.count < maximumNumberOfDeclarativeNetRequestErrorsToSurface)
                    [errorStrings addObject:errorString];
            }
        }
    }

    if (totalErrorCount > maximumNumberOfDeclarativeNetRequestErrorsToSurface)
        [errorStrings addObject:@"Error limit hit. No longer omitting errors."];

    if (outErrorStrings)
        *outErrorStrings = [errorStrings copy];

    allValidatedRules = [allValidatedRules sortedArrayUsingComparator:^NSComparisonResult(_WKWebExtensionDeclarativeNetRequestRule *a, _WKWebExtensionDeclarativeNetRequestRule *b) {
        return [a compare:b];
    }].mutableCopy;

    NSMutableArray<NSDictionary<NSString *, id> *> *translatedRules = [NSMutableArray array];
    for (_WKWebExtensionDeclarativeNetRequestRule *rule in allValidatedRules) {
        NSArray<NSDictionary<NSString *, id> *> *translatedRule = rule.ruleInWebKitFormat;
        [translatedRules addObjectsFromArray:translatedRule];
    }

    return translatedRules;
}

+ (NSArray<NSArray<NSDictionary *> *> *)jsonObjectsFromData:(NSArray<NSData *> *)jsonDataArray errorStrings:(NSArray<NSString *> **)outErrorStrings
{
    NSMutableArray *allJSONObjects = [NSMutableArray array];
    NSMutableArray<NSString *> *errors = [NSMutableArray array];
    for (NSData *jsonData in jsonDataArray) {
        NSError *error;
        NSArray<NSDictionary *> *json = parseJSON(jsonData, { }, &error);
        if (json)
            [allJSONObjects addObject:json];

        if (error)
            [errors addObject:error.userInfo[NSDebugDescriptionErrorKey]];
    }

    if (outErrorStrings)
        *outErrorStrings = [errors copy];

    return allJSONObjects;
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
