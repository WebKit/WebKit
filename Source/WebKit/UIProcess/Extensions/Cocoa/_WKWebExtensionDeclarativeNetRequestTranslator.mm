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
#import <WebCore/LocalizedStrings.h>

static const NSUInteger maximumNumberOfDeclarativeNetRequestErrorsToSurface = 50;

using namespace WebKit;

@implementation _WKWebExtensionDeclarativeNetRequestTranslator

+ (NSArray<NSDictionary<NSString *, id> *> *)translateRules:(NSDictionary<NSString *, NSArray<NSDictionary *> *> *)jsonObjects errorStrings:(NSArray **)outErrorStrings
{
    NSMutableArray<_WKWebExtensionDeclarativeNetRequestRule *> *allValidatedRules = [NSMutableArray array];
    NSMutableDictionary<NSString *, NSMutableSet<NSNumber *> *> *rulesetIDsToRuleIDs = [NSMutableDictionary dictionary];
    NSMutableArray<NSString *> *errorStrings = [NSMutableArray array];
    NSUInteger totalErrorCount = 0;

    for (NSString *rulesetID in jsonObjects.allKeys) {
        for (NSDictionary *ruleJSON in jsonObjects[rulesetID]) {
            NSString *errorString;
            _WKWebExtensionDeclarativeNetRequestRule *rule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:ruleJSON rulesetID:rulesetID errorString:&errorString];

            if (!rulesetIDsToRuleIDs[rulesetID])
                rulesetIDsToRuleIDs[rulesetID] = [NSMutableSet set];

            if ([rulesetIDsToRuleIDs[rulesetID] containsObject:@(rule.ruleID)]) {
                totalErrorCount++;
                ALLOW_NONLITERAL_FORMAT_BEGIN
                [errorStrings addObject:[NSString stringWithFormat:WEB_UI_STRING("`declarative_net_request` ruleset with id `%@` duplicates the rule id `%@`.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for a duplicated rule id").createNSString().get(), rulesetID, @(rule.ruleID)]];
                ALLOW_NONLITERAL_FORMAT_END
            }

            [rulesetIDsToRuleIDs[rulesetID] addObject:@(rule.ruleID)];

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

+ (NSDictionary<NSString *, NSArray<NSDictionary *> *> *)jsonObjectsFromData:(NSDictionary<NSString *, NSData *> *)jsonData errorStrings:(NSArray **)outErrorStrings
{
    NSMutableDictionary *allJSONObjects = [NSMutableDictionary dictionary];
    NSMutableArray<NSString *> *errors = [NSMutableArray array];

    for (NSString *rulesetID in jsonData.allKeys) {
        NSError *error;
        NSArray<NSDictionary *> *json = parseJSON(jsonData[rulesetID], JSONOptions::FragmentsAllowed, &error);

        // The top level of a declarativeNetRequest ruleset should be an array.
        if (json && [json isKindOfClass:NSArray.class])
            allJSONObjects[rulesetID] = json;

        if (error)
            [errors addObject:error.userInfo[NSDebugDescriptionErrorKey]];
    }

    if (outErrorStrings)
        *outErrorStrings = [errors copy];

    return allJSONObjects;
}

@end

#else

@implementation _WKWebExtensionDeclarativeNetRequestTranslator

+ (NSArray<NSDictionary<NSString *, id> *> *)translateRules:(NSArray<NSArray<NSDictionary *> *> *)jsonObjects errorStrings:(NSArray **)outErrorStrings
{
    return nil;
}

+ (NSArray<NSArray<NSDictionary *> *> *)jsonObjectsFromData:(NSDictionary<NSString *, NSData *> *)jsonData errorStrings:(NSArray **)outErrorStrings
{
    return nil;
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
