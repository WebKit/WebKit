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
#import "_WKWebExtensionDeclarativeNetRequestRule.h"

NSString * const sessionRulesetID = @"_session";
NSString * const dynamicRulesetID = @"_dynamic";

#if ENABLE(WK_WEB_EXTENSIONS)

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "CocoaHelpers.h"
#import "WKContentRuleListInternal.h"
#import "WebExtensionUtilities.h"
#import <WebCore/LocalizedStrings.h>

// If any changes are made to the rule translation logic here, make sure to increment currentDeclarativeNetRequestRuleTranslatorVersion in WebExtensionContextCocoa.

// Keys in the top level rule dictionary.
static NSString * const declarativeNetRequestRuleIDKey = @"id";
static NSString * const declarativeNetRequestRulePriorityKey = @"priority";
static NSString * const declarativeNetRequestRuleActionKey = @"action";
static NSString * const declarativeNetRequestRuleConditionKey = @"condition";

// Key and values in the `action` dictionary.
static NSString * const declarativeNetRequestRuleActionTypeKey = @"type";
static NSString * const declarativeNetRequestRuleActionTypeAllow = @"allow";
static NSString * const declarativeNetRequestRuleActionTypeAllowAllRequests = @"allowAllRequests";
static NSString * const declarativeNetRequestRuleActionTypeBlock = @"block";
static NSString * const declarativeNetRequestRuleActionTypeUpgradeScheme = @"upgradeScheme";

static NSString * const declarativeNetRequestRuleActionTypeRedirect = @"redirect";
static NSString * const declarativeNetRequestRuleActionRedirect = @"redirect";
static NSString * const declarativeNetRequestRuleRedirectURL = @"url";
static NSString * const declarativeNetRequestRuleRedirectRegexSubstitution = @"regexSubstitution";
static NSString * const declarativeNetRequestRuleRedirectExtensionPath = @"extensionPath";
static NSString * const declarativeNetRequestRuleRedirectTransform = @"transform";
static NSString * const declarativeNetRequestRuleURLTransformFragment = @"fragment";
static NSString * const declarativeNetRequestRuleURLTransformHost = @"host";
static NSString * const declarativeNetRequestRuleURLTransformPassword = @"password";
static NSString * const declarativeNetRequestRuleURLTransformPath = @"path";
static NSString * const declarativeNetRequestRuleURLTransformPort = @"port";
static NSString * const declarativeNetRequestRuleURLTransformQuery = @"query";
static NSString * const declarativeNetRequestRuleURLTransformQueryTransform = @"queryTransform";
static NSString * const declarativeNetRequestRuleURLTransformScheme = @"scheme";
static NSString * const declarativeNetRequestRuleURLTransformUsername = @"username";
static NSString * const declarativeNetRequestRuleQueryTransformAddOrReplaceParams = @"addOrReplaceParams";
static NSString * const declarativeNetRequestRuleQueryTransformRemoveParams = @"removeParams";
static NSString * const declarativeNetRequestRuleAddOrReplaceParamsKey = @"key";
static NSString * const declarativeNetRequestRuleAddOrReplaceParamsReplaceOnly = @"replaceOnly";
static NSString * const declarativeNetRequestRuleAddOrReplaceParamsValue = @"value";

static NSString * const declarativeNetRequestRuleActionTypeModifyHeaders = @"modifyHeaders";
static NSString * const declarativeNetRequestRuleResponseHeadersKey = @"responseHeaders";
static NSString * const declarativeNetRequestRuleRequestHeadersKey = @"requestHeaders";
static NSString * const declarativeNetRequestRuleHeaderKey = @"header";
static NSString * const declarativeNetRequestRuleHeaderOperationKey = @"operation";
static NSString * const declarativeNetRequestRuleHeaderOperationValueSet = @"set";
static NSString * const declarativeNetRequestRuleHeaderOperationValueAppend = @"append";
static NSString * const declarativeNetRequestRuleHeaderOperationValueRemove = @"remove";
static NSString * const declarativeNetRequestRuleHeaderValueKey = @"value";

// Keys in the `condition` dictionary.
static NSString * const declarativeNetRequestRuleConditionDomainTypeKey = @"domainType";
static NSString * const declarativeNetRequestRuleConditionDomainsKey = @"domains";
static NSString * const ruleConditionRequestDomainsKey = @"requestDomains";
static NSString * const declarativeNetRequestRuleConditionExcludedDomainsKey = @"excludedDomains";
static NSString * const ruleConditionExcludedRequestDomainsKey = @"excludedRequestDomains";
static NSString * const ruleConditionInitiatorDomainsKey = @"initiatorDomains";
static NSString * const ruleConditionExcludedInitiatorDomainsKey = @"excludedInitiatorDomains";
static NSString * const declarativeNetRequestRuleConditionExcludedResourceTypesKey = @"excludedResourceTypes";
static NSString * const declarativeNetRequestRuleConditionCaseSensitiveKey = @"isUrlFilterCaseSensitive";
static NSString * const declarativeNetRequestRuleConditionRegexFilterKey = @"regexFilter";
static NSString * const declarativeNetRequestRuleConditionResourceTypeKey = @"resourceTypes";
static NSString * const declarativeNetRequestRuleConditionURLFilterKey = @"urlFilter";
static NSString * const declarativeNetRequestRuleConditionRequestMethodsKey = @"requestMethods";
static NSString * const declarativeNetRequestRuleConditionExcludedRequestMethodsKey = @"excludedRequestMethods";

// The ordering of these values is important because it's used in sorting the content blocking rules.
typedef NS_ENUM(NSInteger, DeclarativeNetRequestRuleActionType) {
    DeclarativeNetRequestRuleActionTypeModifyHeaders,
    DeclarativeNetRequestRuleActionTypeRedirect,
    DeclarativeNetRequestRuleActionTypeUpgradeScheme,
    DeclarativeNetRequestRuleActionTypeBlock,
    DeclarativeNetRequestRuleActionTypeAllowAllRequests,
    DeclarativeNetRequestRuleActionTypeAllow,
};

using namespace WebKit;

@implementation _WKWebExtensionDeclarativeNetRequestRule

- (instancetype)initWithDictionary:(NSDictionary *)ruleDictionary rulesetID:(NSString *)rulesetID errorString:(NSString **)outErrorString
{
    if (!(self = [super init]))
        return nil;

    static NSArray *requiredKeysInRuleDictionary = @[
        declarativeNetRequestRuleIDKey,
        declarativeNetRequestRuleActionKey,
        declarativeNetRequestRuleConditionKey,
    ];

    static NSDictionary *keyToExpectedValueTypeInRuleDictionary = @{
        declarativeNetRequestRuleIDKey: NSNumber.class,
        declarativeNetRequestRulePriorityKey: NSNumber.class,
        declarativeNetRequestRuleActionKey: NSDictionary.class,
        declarativeNetRequestRuleConditionKey: NSDictionary.class,
    };

    _ruleID = objectForKey<NSNumber>(ruleDictionary, declarativeNetRequestRuleIDKey).integerValue;
    if (!_ruleID) {
        if (outErrorString)
            *outErrorString = @"Missing rule id.";

        return nil;
    }

    _rulesetID = rulesetID;

    NSString *exceptionString;
    if (!validateDictionary(ruleDictionary, nil, requiredKeysInRuleDictionary, keyToExpectedValueTypeInRuleDictionary, &exceptionString)) {
        if (outErrorString)
            *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. %@", (long)_ruleID, exceptionString];

        return nil;
    }

    NSNumber *priorityAsNumber = objectForKey<NSNumber>(ruleDictionary, declarativeNetRequestRulePriorityKey);
    _priority = priorityAsNumber ? priorityAsNumber.integerValue : 1;

    if (_ruleID < 1) {
        if (outErrorString)
            *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. Rule id must be non-negative.", (long)_ruleID];

        return nil;
    }

    if (_priority < 1) {
        if (outErrorString)
            *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. Rule priority must be non-negative.", (long)_ruleID];

        return nil;
    }

    _action = objectForKey<NSDictionary>(ruleDictionary, declarativeNetRequestRuleActionKey);

    NSArray *requiredKeysInActionDictionary = @[
        declarativeNetRequestRuleActionTypeKey,
    ];

    NSDictionary *keyToExpectedValueTypeInActionDictionary = @{
        declarativeNetRequestRuleActionTypeKey: NSString.class,
        declarativeNetRequestRuleActionRedirect: NSDictionary.class,
        declarativeNetRequestRuleRequestHeadersKey: @[ NSDictionary.class ],
        declarativeNetRequestRuleResponseHeadersKey: @[ NSDictionary.class ],
    };

    if (!validateDictionary(_action, nil, requiredKeysInActionDictionary, keyToExpectedValueTypeInActionDictionary, &exceptionString)) {
        if (outErrorString)
            *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. %@", (long)_ruleID, exceptionString];

        return nil;
    }

    static NSSet *supportedActionTypes = [NSSet setWithArray:@[
        declarativeNetRequestRuleActionTypeAllow,
        declarativeNetRequestRuleActionTypeAllowAllRequests,
        declarativeNetRequestRuleActionTypeBlock,
        declarativeNetRequestRuleActionTypeRedirect,
        declarativeNetRequestRuleActionTypeModifyHeaders,
        declarativeNetRequestRuleActionTypeUpgradeScheme,
    ]];

    if (![supportedActionTypes containsObject:_action[declarativeNetRequestRuleActionTypeKey]]) {
        if (outErrorString)
            *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `%@` is not a supported action type.", (long)_ruleID, _action[declarativeNetRequestRuleActionTypeKey]];

        return nil;
    }

    _condition = objectForKey<NSDictionary>(ruleDictionary, declarativeNetRequestRuleConditionKey);

    static NSDictionary *keyToExpectedValueTypeInConditionDictionary = @{
        declarativeNetRequestRuleConditionDomainTypeKey: NSString.class,
        declarativeNetRequestRuleConditionDomainsKey: @[ NSString.class ],
        ruleConditionRequestDomainsKey: @[ NSString.class ],
        declarativeNetRequestRuleConditionExcludedDomainsKey: @[ NSString.class ],
        ruleConditionExcludedRequestDomainsKey: @[ NSString.class ],
        declarativeNetRequestRuleConditionCaseSensitiveKey: @YES.class,
        declarativeNetRequestRuleConditionRegexFilterKey: NSString.class,
        declarativeNetRequestRuleConditionResourceTypeKey: @[ NSString.class ],
        declarativeNetRequestRuleConditionURLFilterKey: NSString.class,
        ruleConditionInitiatorDomainsKey: @[ NSString.class ],
        ruleConditionExcludedInitiatorDomainsKey: @[ NSString.class ],
        declarativeNetRequestRuleConditionRequestMethodsKey: @[ NSString.class ],
        declarativeNetRequestRuleConditionExcludedRequestMethodsKey: @[ NSString.class ],
    };

    if (!validateDictionary(_condition, nil, @[ ], keyToExpectedValueTypeInConditionDictionary, &exceptionString)) {
        if (outErrorString)
            *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. %@", (long)_ruleID, exceptionString];

        return nil;
    }

    if (_condition[declarativeNetRequestRuleConditionRegexFilterKey] && _condition[declarativeNetRequestRuleConditionURLFilterKey]) {
        if (outErrorString)
            *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. Define only one of the `regexFilter` or `urlFilter` keys.", (long)_ruleID];

        return nil;
    }

    if (NSString *regexFilter = _condition[declarativeNetRequestRuleConditionRegexFilterKey]) {
        if (![regexFilter canBeConvertedToEncoding:NSASCIIStringEncoding]) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `regexFilter` cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }

        if (![WKContentRuleList _supportsRegularExpression:regexFilter]) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `regexFilter` is not a supported regular expression.", (long)_ruleID];

            return nil;
        }
    }

    if (NSString *urlFilter = _condition[declarativeNetRequestRuleConditionURLFilterKey]) {
        if (![urlFilter canBeConvertedToEncoding:NSASCIIStringEncoding]) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `urlFilter` cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (_condition[declarativeNetRequestRuleConditionResourceTypeKey] && _condition[declarativeNetRequestRuleConditionExcludedResourceTypesKey]) {
        if (outErrorString)
            *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. Define only one of the `resourceTypes` or `excludedResourceTypes` keys.", (long)_ruleID];

        return nil;
    }

    NSArray<NSString *> *resourceTypes = _condition[declarativeNetRequestRuleConditionResourceTypeKey];
    if (resourceTypes) {
        if (!resourceTypes.count) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `resourceTypes` cannot be an empty array.", (long)_ruleID];

            return nil;
        }

        [self removeInvalidResourceTypesForKey:declarativeNetRequestRuleConditionResourceTypeKey];
        resourceTypes = _condition[declarativeNetRequestRuleConditionResourceTypeKey];

        if (!resourceTypes.count) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. The value in the `resourceTypes` array is invalid.", (long)_ruleID];

            return nil;
        }
    }

    if (_condition[declarativeNetRequestRuleConditionExcludedResourceTypesKey])
        [self removeInvalidResourceTypesForKey:declarativeNetRequestRuleConditionResourceTypeKey];

    if ([_action[declarativeNetRequestRuleActionTypeKey] isEqualToString:declarativeNetRequestRuleActionTypeAllowAllRequests]) {
        if (!resourceTypes) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. A rule with the `allowAllRequests` action type must have the `resourceTypes` key.", (long)_ruleID];

            return nil;
        }

        for (NSString *resourceType in resourceTypes) {
            if (![resourceType isEqualToString:@"main_frame"] && ![resourceType isEqualToString:@"sub_frame"]) {
                if (outErrorString)
                    *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `%@` is not a valid resource type for the `allowAllRequests` action type.", (long)_ruleID, resourceType];

                return nil;
            }
        }
    }

    if (NSString *domainType = _condition[declarativeNetRequestRuleConditionDomainTypeKey]) {
        if (![[self _chromeDomainTypeToWebKitDomainType] objectForKey:domainType]) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `%@` is not a valid domain type.", (long)_ruleID, domainType];

            return nil;
        }
    }

    if (NSArray<NSString *> *domains = _condition[declarativeNetRequestRuleConditionDomainsKey]) {
        if (!isArrayOfDomainsValid(domains)) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `domains` must be non-empty and cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (NSArray<NSString *> *domains = _condition[ruleConditionRequestDomainsKey]) {
        if (!isArrayOfDomainsValid(domains)) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `requestDomains` must be non-empty and cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (NSArray<NSString *> *excludedDomains = _condition[declarativeNetRequestRuleConditionExcludedDomainsKey]) {
        if (!isArrayOfExcludedDomainsValid(excludedDomains)) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `excludedDomains` cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (NSArray<NSString *> *excludedDomains = _condition[ruleConditionExcludedRequestDomainsKey]) {
        if (!isArrayOfExcludedDomainsValid(excludedDomains)) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `excludedRequestDomains` cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (NSArray<NSString *> *initiatorDomains = _condition[ruleConditionInitiatorDomainsKey]) {
        if (!isArrayOfDomainsValid(initiatorDomains)) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `initiatorDomains` must be non-empty and cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (NSArray<NSString *> *excludedInitiatorDomains = _condition[ruleConditionExcludedInitiatorDomainsKey]) {
        if (!isArrayOfExcludedDomainsValid(excludedInitiatorDomains)) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `excludedInitiatorDomains` cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (NSArray<NSString *> *requestMethods = _condition[declarativeNetRequestRuleConditionRequestMethodsKey]) {
        if (!isArrayOfRequestMethodsValid(requestMethods)) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `requestMethods` must be non-empty and can only contain %@.", (long)_ruleID, validRequestMethodsString()];

            return nil;
        }
    }

    if (NSArray<NSString *> *excludedRequestMethods = _condition[declarativeNetRequestRuleConditionExcludedRequestMethodsKey]) {
        if (!isArrayOfRequestMethodsValid(excludedRequestMethods)) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `excludedRequestMethods` must be non-empty and can only contain %@.", (long)_ruleID, validRequestMethodsString()];

            return nil;
        }
    }

    if ([_action[declarativeNetRequestRuleActionTypeKey] isEqualToString:declarativeNetRequestRuleActionTypeRedirect]) {
        NSDictionary<NSString *, id> *redirectDictionary = _action[declarativeNetRequestRuleActionRedirect];
        NSString *urlString = objectForKey<NSString>(redirectDictionary, declarativeNetRequestRuleRedirectURL, true);
        NSString *extensionPathString = objectForKey<NSString>(redirectDictionary, declarativeNetRequestRuleRedirectExtensionPath, true);
        NSString *regexSubstitutionString = objectForKey<NSString>(redirectDictionary, declarativeNetRequestRuleRedirectRegexSubstitution, true);
        NSDictionary<NSString *, id> *transformDictionary = objectForKey<NSDictionary>(redirectDictionary, declarativeNetRequestRuleRedirectTransform, false);

        if (!urlString && !extensionPathString && !transformDictionary && !regexSubstitutionString) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `redirect` is missing either a `url`, `extensionPath`, `regexSubstitution`, or `transform` key.", (long)_ruleID];

            return nil;
        }

        // FIXME: rdar://105890168 (declarativeNetRequest: Only allow one of transform, url, extensionPath or regexSubstitution in redirect actions)

        if (urlString) {
            NSURL *url = [NSURL URLWithString:urlString];
            if (!url) {
                if (outErrorString)
                    *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `redirect` specified an invalid or empty `url`.", (long)_ruleID];

                return nil;
            }

            if (!URL(url).protocolIsInHTTPFamily()) {
                if (outErrorString)
                    *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `redirect` specified a non-HTTP `url`.", (long)_ruleID];

                return nil;
            }
        }

        if (regexSubstitutionString) {
            if (!_condition[declarativeNetRequestRuleConditionRegexFilterKey]) {
                if (outErrorString)
                    *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `redirect` specified a `regexSubstitution` without a `regexFilter` condition.", (long)_ruleID];

                return nil;
            }
        }

        if (extensionPathString && ![extensionPathString hasPrefix:@"/"]) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `redirect` specified an `extensionPath` without a '/' prefix.", (long)_ruleID];

            return nil;
        }

        if (transformDictionary && !transformDictionary.count) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `redirect` specified an invalid or empty `transform`.", (long)_ruleID];

            return nil;
        }

        static NSDictionary *keyToExpectedValueTypeInTransformDictionary = @{
            declarativeNetRequestRuleURLTransformFragment: NSString.class,
            declarativeNetRequestRuleURLTransformHost: NSString.class,
            declarativeNetRequestRuleURLTransformPassword: NSString.class,
            declarativeNetRequestRuleURLTransformPath: NSString.class,
            declarativeNetRequestRuleURLTransformPort: NSString.class,
            declarativeNetRequestRuleURLTransformQuery: NSString.class,
            declarativeNetRequestRuleURLTransformQueryTransform: NSDictionary.class,
            declarativeNetRequestRuleURLTransformScheme: NSString.class,
            declarativeNetRequestRuleURLTransformUsername: NSString.class,
        };

        if (transformDictionary && !validateDictionary(transformDictionary, nil, @[ ], keyToExpectedValueTypeInTransformDictionary, &exceptionString)) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `redirect` specified an invalid `transform`. %@", (long)_ruleID, exceptionString];

            return nil;
        }

        NSDictionary<NSString *, id> *queryTransformDictionary = objectForKey<NSDictionary>(transformDictionary, declarativeNetRequestRuleURLTransformQueryTransform, false);
        if (queryTransformDictionary && !queryTransformDictionary.count) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `transform` specified an invalid or empty `queryTransform`.", (long)_ruleID];

            return nil;
        }

        static NSDictionary *keyToExpectedValueTypeInQueryTransformDictionary = @{
            declarativeNetRequestRuleQueryTransformAddOrReplaceParams: @[ NSDictionary.class ],
            declarativeNetRequestRuleQueryTransformRemoveParams: @[ NSString.class ],
        };

        if (queryTransformDictionary && !validateDictionary(queryTransformDictionary, nil, @[ ], keyToExpectedValueTypeInQueryTransformDictionary, &exceptionString)) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `transform` specified an invalid `queryTransform`. %@", (long)_ruleID, exceptionString];

            return nil;
        }

        NSArray<NSString *> *removeParamsArray = queryTransformDictionary[declarativeNetRequestRuleQueryTransformRemoveParams];
        if (removeParamsArray && !removeParamsArray.count) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `queryTransform` specified an invalid or empty `removeParams`.", (long)_ruleID];

            return nil;
        }

        static NSArray *requiredKeysInAddOrReplaceDictionary = @[
            declarativeNetRequestRuleAddOrReplaceParamsKey,
            declarativeNetRequestRuleAddOrReplaceParamsValue,
        ];

        static NSDictionary *keyToExpectedValueTypeInAddOrReplaceDictionary = @{
            declarativeNetRequestRuleAddOrReplaceParamsKey: NSString.class,
            declarativeNetRequestRuleAddOrReplaceParamsValue: NSString.class,
            declarativeNetRequestRuleAddOrReplaceParamsReplaceOnly: @YES.class,
        };

        NSArray<NSDictionary<NSString *, id> *> *addOrReplaceParamsArray = queryTransformDictionary[declarativeNetRequestRuleQueryTransformAddOrReplaceParams];
        if (addOrReplaceParamsArray && !addOrReplaceParamsArray.count) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `queryTransform` specified an invalid or empty `addOrReplaceParams`.", (long)_ruleID];

            return nil;
        }

        for (NSDictionary<NSString *, id> *addOrReplaceParamsDictionary in addOrReplaceParamsArray) {
            if (!validateDictionary(addOrReplaceParamsDictionary, nil, requiredKeysInAddOrReplaceDictionary, keyToExpectedValueTypeInAddOrReplaceDictionary, &exceptionString)) {
                if (outErrorString)
                    *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `queryTransform` specified an invalid `addOrReplaceParams`. %@", (long)_ruleID, exceptionString];

                return nil;
            }
        }
    }

    if ([_action[declarativeNetRequestRuleActionTypeKey] isEqual:declarativeNetRequestRuleActionTypeModifyHeaders]) {
        NSArray<NSDictionary *> *requestHeadersInfo = _action[declarativeNetRequestRuleRequestHeadersKey];
        NSArray<NSDictionary *> *responseHeadersInfo = _action[declarativeNetRequestRuleResponseHeadersKey];

        if (!requestHeadersInfo && !responseHeadersInfo) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. A modifyHeaders rule must have `requestHeaders` or `responseHeaders` set.", (long)_ruleID];

            return nil;
        }

        if ((requestHeadersInfo && !requestHeadersInfo.count) || (responseHeadersInfo && !responseHeadersInfo.count)) {
            if (outErrorString)
                *outErrorString = [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. The arrays specified by `requestHeaders` or `responseHeaders` must be non-empty.", (long)_ruleID];

            return nil;
        }

        for (NSDictionary *headerInfo in requestHeadersInfo) {
            NSString *errorString = [self _validateHeaderInfoDictionary:headerInfo];
            if (errorString) {
                if (outErrorString)
                    *outErrorString = errorString;

                return nil;
            }
        }

        for (NSDictionary *headerInfo in responseHeadersInfo) {
            NSString *errorString = [self _validateHeaderInfoDictionary:headerInfo];
            if (errorString) {
                if (outErrorString)
                    *outErrorString = errorString;

                return nil;
            }
        }
    }

    return self;
}

- (NSString *)_validateHeaderInfoDictionary:(NSDictionary *)headerInfo
{
    static NSArray *requiredKeysInModifyHeadersDictionary = @[
        declarativeNetRequestRuleHeaderKey,
        declarativeNetRequestRuleHeaderOperationKey,
    ];

    static NSDictionary *keyToExpectedValueTypeInHeadersDictionary = @{
        declarativeNetRequestRuleHeaderKey: NSString.class,
        declarativeNetRequestRuleHeaderOperationKey: NSString.class,
        declarativeNetRequestRuleHeaderValueKey: NSString.class,
    };

    NSString *exceptionString;
    if (!validateDictionary(headerInfo, nil, requiredKeysInModifyHeadersDictionary, keyToExpectedValueTypeInHeadersDictionary, &exceptionString))
        return [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. One of the headers dictionaries is not formatted correctly. %@", (long)_ruleID, exceptionString];

    NSString *operationType = headerInfo[declarativeNetRequestRuleHeaderOperationKey];
    BOOL isSetOperation = [operationType isEqual:declarativeNetRequestRuleHeaderOperationValueSet];
    BOOL isAppendOperation = [operationType isEqual:declarativeNetRequestRuleHeaderOperationValueAppend];
    BOOL isRemoveOperation = [operationType isEqual:declarativeNetRequestRuleHeaderOperationValueRemove];

    if (!isSetOperation && !isAppendOperation && !isRemoveOperation)
        return [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. `%@` is not a recognized header operation.", (long)_ruleID, operationType];

    NSString *headerName = headerInfo[declarativeNetRequestRuleHeaderKey];
    if (!isHeaderNameValid(headerName))
        return [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. The header `%@` is not recognized.", (long)_ruleID, headerName];

    NSString *headerValue = headerInfo[declarativeNetRequestRuleHeaderValueKey];
    if (isRemoveOperation && headerValue)
        return [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. Do not provide a value when removing a header.", (long)_ruleID];

    if ((isSetOperation || isAppendOperation) && !headerValue)
        return [[NSString alloc] initWithFormat:@"Rule with id %ld is invalid. You must provide a value when modifying a header.", (long)_ruleID];

    return nil;
}

static BOOL isHeaderNameValid(NSString *headerName)
{
    static NSArray<NSString *> *acceptedHeaderNames = @[
        @"accept",
        @"accept-charset",
        @"accept-language",
        @"accept-encoding",
        @"accept-ranges",
        @"access-control-allow-credentials",
        @"access-control-allow-headers",
        @"access-control-allow-methods",
        @"access-control-allow-origin",
        @"access-control-expose-headers",
        @"access-control-max-age",
        @"access-control-request-headers",
        @"access-control-request-method",
        @"age",
        @"authorization",
        @"cache-control",
        @"connection",
        @"content-disposition",
        @"content-encoding",
        @"content-language",
        @"content-length",
        @"content-location",
        @"content-security-policy",
        @"content-security-policy-report-only",
        @"content-type",
        @"content-range",
        @"cookie",
        @"cookie2",
        @"cross-origin-embedder-policy",
        @"cross-origin-embedder-policy-report-only",
        @"cross-origin-opener-policy",
        @"cross-origin-opener-policy-report-only",
        @"cross-origin-resource-policy",
        @"date",
        @"dnt",
        @"default-style",
        @"etag",
        @"expect",
        @"expires",
        @"host",
        @"if-match",
        @"if-modified-since",
        @"if-none-match",
        @"if-range",
        @"if-unmodified-since",
        @"keep-alive",
        @"last-event-id",
        @"last-modified",
        @"link",
        @"location",
        @"origin",
        @"ping-from",
        @"ping-to",
        @"purpose",
        @"pragma",
        @"proxy-authorization",
        @"range",
        @"referer",
        @"referrer-policy",
        @"refresh",
        @"report-to",
        @"reporting-endpoints",
        @"sec-fetch-dest",
        @"sec-fetch-mode",
        @"sec-gpc",
        @"sec-websocket-accept",
        @"sec-websocket-extensions",
        @"sec-websocket-key",
        @"sec-websocket-protocol",
        @"sec-websocket-version",
        @"server-timing",
        @"service-worker",
        @"service-worker-allowed",
        @"service-worker-navigation-preload",
        @"set-cookie",
        @"set-cookie2",
        @"sourcemap",
        @"te",
        @"timing-allow-origin",
        @"trailer",
        @"transfer-encoding",
        @"upgrade",
        @"upgrade-insecure-requests",
        @"user-agent",
        @"vary",
        @"via",
        @"x-content-type-options",
        @"x-frame-options",
        @"x-sourcemap",
        @"x-xss-protection",
        @"x-temp-tablet",
        @"icy-metaint",
        @"icy-metadata",
    ];

    return [acceptedHeaderNames containsObject:headerName.lowercaseString];
}

static BOOL isArrayOfDomainsValid(NSArray<NSString *> *domains)
{
    if (!domains.count)
        return NO;

    for (NSString *domain in domains) {
        if (![domain canBeConvertedToEncoding:NSASCIIStringEncoding])
            return NO;
    }

    return YES;
}

static BOOL isArrayOfExcludedDomainsValid(NSArray<NSString *> *excludedDomains)
{
    for (NSString *excludedDomain in excludedDomains) {
        if (![excludedDomain canBeConvertedToEncoding:NSASCIIStringEncoding])
            return NO;
    }

    return YES;
}

static NSSet<NSString *> *validRequestMethods(void)
{
    static NSSet<NSString *> *validRequestMethods = [NSSet setWithObjects:@"get", @"head", @"options", @"trace", @"put", @"delete", @"post", @"patch", @"connect", nil];
    return validRequestMethods;
}

static NSString *validRequestMethodsString(void)
{
    static NSString *validRequestMethodsString = [] {
        NSArray *validRequestMethodsArray = validRequestMethods().allObjects;
        NSString* validRequestMethodsString = [[validRequestMethodsArray subarrayWithRange:NSMakeRange(0, validRequestMethodsArray.count - 1)] componentsJoinedByString:@", "];
        return [NSString stringWithFormat:@"%@, and %@", validRequestMethodsString, validRequestMethodsArray.lastObject];
    }();
    return validRequestMethodsString;
}

static BOOL isArrayOfRequestMethodsValid(NSArray<NSString *> *requestMethods)
{
    if (!requestMethods.count)
        return NO;

    for (NSString *requestMethod in requestMethods) {
        if (![validRequestMethods() containsObject:requestMethod])
            return NO;
    }

    return YES;
}

- (void)removeInvalidResourceTypesForKey:(NSString *)ruleConditionKey
{
    NSArray *actualResourceTypes = _condition[ruleConditionKey];
    NSArray *filteredResourceTypes = mapObjects(actualResourceTypes, ^NSString *(id key, NSString *resourceType) {
        return [[self _chromeResourceTypeToWebKitResourceType] objectForKey:resourceType] ?: nil;
    });

    if (filteredResourceTypes.count != actualResourceTypes.count) {
        NSMutableDictionary *modifiedCondition = [_condition mutableCopy];
        modifiedCondition[ruleConditionKey] = filteredResourceTypes;
        _condition = modifiedCondition;
    }
}

- (NSArray<NSDictionary<NSString *, id> *> *)ruleInWebKitFormat
{
    static NSDictionary *chromeActionTypesToWebKitActionTypes = @{
        declarativeNetRequestRuleActionTypeAllow: @"ignore-following-rules",
        declarativeNetRequestRuleActionTypeAllowAllRequests: @"ignore-following-rules",
        declarativeNetRequestRuleActionTypeBlock: @"block",
        declarativeNetRequestRuleActionTypeModifyHeaders: @"modify-headers",
        declarativeNetRequestRuleActionTypeRedirect: @"redirect",
        declarativeNetRequestRuleActionTypeUpgradeScheme: @"make-https",
    };

    NSMutableArray<NSDictionary<NSString *, id> *> *convertedRules = [NSMutableArray array];

    NSString *chromeActionType = _action[declarativeNetRequestRuleActionTypeKey];
    NSString *webKitActionType = chromeActionTypesToWebKitActionTypes[chromeActionType];
    BOOL isRuleForAllowAllRequests = [chromeActionType isEqualToString:declarativeNetRequestRuleActionTypeAllowAllRequests];

    NSDictionary *(^createModifiedConditionsForURLFilter)(NSDictionary *, NSString *) = ^NSDictionary *(NSDictionary *condition, NSString *urlFilter) {
        NSMutableDictionary *modifiedCondition = [condition mutableCopy];
        modifiedCondition[declarativeNetRequestRuleConditionURLFilterKey] = urlFilter;
        modifiedCondition[declarativeNetRequestRuleConditionRegexFilterKey] = nil;
        modifiedCondition[ruleConditionRequestDomainsKey] = nil;
        modifiedCondition[ruleConditionExcludedRequestDomainsKey] = nil;
        return modifiedCondition;
    };

    NSDictionary *(^createModifiedConditionsForRequestMethod)(NSDictionary *, NSString *) = ^NSDictionary *(NSDictionary *condition, NSString *requestMethod) {
        NSMutableDictionary *modifiedCondition = [condition mutableCopy];
        modifiedCondition[declarativeNetRequestRuleConditionRequestMethodsKey] = requestMethod;
        modifiedCondition[declarativeNetRequestRuleConditionExcludedRequestMethodsKey] = nil;
        return modifiedCondition;
    };

    NSArray *requestDomains = _condition[ruleConditionRequestDomainsKey];
    NSArray *excludedRequestDomains = _condition[ruleConditionExcludedRequestDomainsKey];
    NSArray *requestMethods = _condition[declarativeNetRequestRuleConditionRequestMethodsKey];
    NSArray *excludedRequestMethods = _condition[declarativeNetRequestRuleConditionExcludedRequestMethodsKey];

    // We also have to create one ignore-following-rules rule per excluded request domain and excluded request method...
    if (excludedRequestDomains) {
        for (NSString *excludedRequestDomain in excludedRequestDomains) {
            NSDictionary *modifiedConditionsForURLFilter = createModifiedConditionsForURLFilter(_condition, excludedRequestDomain);

            if (excludedRequestMethods && !isRuleForAllowAllRequests) {
                for (NSString *excludedRequestMethod in excludedRequestMethods)
                    [convertedRules addObjectsFromArray:[self _convertRulesWithModifiedCondition:createModifiedConditionsForRequestMethod(modifiedConditionsForURLFilter, excludedRequestMethod) webKitActionType:@"ignore-following-rules" chromeActionType:chromeActionType includeIdentifier:NO]];
            } else
                [convertedRules addObjectsFromArray:[self _convertRulesWithModifiedCondition:modifiedConditionsForURLFilter webKitActionType:@"ignore-following-rules" chromeActionType:chromeActionType includeIdentifier:NO]];
        }
    } else if (excludedRequestMethods && !isRuleForAllowAllRequests) {
        for (NSString *excludedRequestMethod in excludedRequestMethods)
            [convertedRules addObjectsFromArray:[self _convertRulesWithModifiedCondition:createModifiedConditionsForRequestMethod(_condition, excludedRequestMethod) webKitActionType:@"ignore-following-rules" chromeActionType:chromeActionType includeIdentifier:NO]];
    }

    // ... and we also have to create one rule per request domain (unless we have a regex filter) and request method.
    if (requestDomains && !_condition[declarativeNetRequestRuleConditionRegexFilterKey]) {
        for (NSString *requestDomain in requestDomains) {
            NSString *combinedRequestDomainAndURLFilter = [self _combineRequestDomain:requestDomain withURLFilter:_condition[declarativeNetRequestRuleConditionURLFilterKey]];
            NSDictionary *modifiedConditionsForURLFilter = createModifiedConditionsForURLFilter(_condition, combinedRequestDomainAndURLFilter);

            if (requestMethods && !isRuleForAllowAllRequests) {
                for (NSString *requestMethod in requestMethods)
                    [convertedRules addObjectsFromArray:[self _convertRulesWithModifiedCondition:createModifiedConditionsForRequestMethod(modifiedConditionsForURLFilter, requestMethod) webKitActionType:webKitActionType chromeActionType:chromeActionType includeIdentifier:YES]];
            } else
                [convertedRules addObjectsFromArray:[self _convertRulesWithModifiedCondition:modifiedConditionsForURLFilter webKitActionType:webKitActionType chromeActionType:chromeActionType includeIdentifier:YES]];
        }
    } else if (requestMethods && !isRuleForAllowAllRequests) {
        for (NSString *requestMethod in requestMethods)
            [convertedRules addObjectsFromArray:[self _convertRulesWithModifiedCondition:createModifiedConditionsForRequestMethod(_condition, requestMethod) webKitActionType:webKitActionType chromeActionType:chromeActionType includeIdentifier:YES]];
    } else
        [convertedRules addObjectsFromArray:[self _convertRulesWithModifiedCondition:_condition webKitActionType:webKitActionType chromeActionType:chromeActionType includeIdentifier:YES]];

    return convertedRules;
}

- (NSArray<NSDictionary *> *)_convertRulesWithModifiedCondition:(NSDictionary *)condition webKitActionType:(NSString *)webKitActionType chromeActionType:(NSString *)chromeActionType includeIdentifier:(BOOL)includeIdentifier
{
    NSMutableArray<NSDictionary<NSString *, id> *> *convertedRules = [NSMutableArray array];

    if ((condition[ruleConditionInitiatorDomainsKey] && condition[ruleConditionExcludedInitiatorDomainsKey]) && ![chromeActionType isEqualToString:declarativeNetRequestRuleActionTypeAllowAllRequests]) {
        // If a rule specifies both initiatorDomains and excludedInitiatorDomains, we need to turn that into two rules.
        // We create the first rule as an ignore-following-rules rule using if-frame-url instead of unless-frame-url.
        // The second rule will have the initiatorDomains implemented as normal (using if-frame-url).
        NSMutableDictionary *modifiedCondition = [condition mutableCopy];
        modifiedCondition[ruleConditionInitiatorDomainsKey] = modifiedCondition[ruleConditionExcludedInitiatorDomainsKey];
        modifiedCondition[ruleConditionExcludedInitiatorDomainsKey] = nil;
        [convertedRules addObject:[self _webKitRuleWithWebKitActionType:@"ignore-following-rules" chromeActionType:chromeActionType condition:modifiedCondition includeIdentifier:NO]];
    }

    [convertedRules addObject:[self _webKitRuleWithWebKitActionType:webKitActionType chromeActionType:chromeActionType condition:condition includeIdentifier:includeIdentifier]];

    if ([webKitActionType isEqualToString:@"make-https"])
        [convertedRules addObject:[self _webKitRuleWithWebKitActionType:@"ignore-following-rules" chromeActionType:chromeActionType condition:condition includeIdentifier:NO]];

    return [convertedRules copy];
}

- (NSDictionary *)_webKitRuleWithWebKitActionType:(NSString *)webKitActionType chromeActionType:(NSString *)chromeActionType condition:(NSDictionary *)condition includeIdentifier:(BOOL)includeIdentifier
{
    NSMutableDictionary *actionDictionary = [@{ @"type": webKitActionType } mutableCopy];
    NSMutableDictionary *triggerDictionary = [NSMutableDictionary dictionary];
    NSNumber *isCaseSensitive = condition[declarativeNetRequestRuleConditionCaseSensitiveKey] ?: @NO;
    NSMutableDictionary<NSString *, id> *convertedRule = [@{
        @"action": actionDictionary,
        @"trigger": triggerDictionary,
    } mutableCopy];

#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
    if (includeIdentifier) {
        convertedRule[@"_identifier"] = @(_ruleID);
        convertedRule[@"_rulesetIdentifier"] = _rulesetID;
    }
#endif

    if ([chromeActionType isEqualToString:declarativeNetRequestRuleActionTypeAllowAllRequests]) {
        triggerDictionary[@"url-filter"] = @".*";

        if ([condition[declarativeNetRequestRuleConditionResourceTypeKey] containsObject:@"main_frame"])
            triggerDictionary[@"if-top-url"] = @[ [self _regexURLFilterForChromeURLFilter:condition[declarativeNetRequestRuleConditionURLFilterKey]] ?: condition[declarativeNetRequestRuleConditionRegexFilterKey] ?: @".*" ];
        else {
            // FIXME: rdar://154124673 (dNR: fix sub_frame resourceType allowAllRequests rules)
            triggerDictionary[@"if-frame-url"] = @[ [self _regexURLFilterForChromeURLFilter:condition[declarativeNetRequestRuleConditionURLFilterKey]] ?: condition[declarativeNetRequestRuleConditionRegexFilterKey] ?: @".*" ];
            triggerDictionary[@"load-context"] = @[ @"child-frame" ];
        }

        return [convertedRule copy];
    }

    NSString *filter = [self _regexURLFilterForChromeURLFilter:condition[declarativeNetRequestRuleConditionURLFilterKey]] ?: condition[declarativeNetRequestRuleConditionRegexFilterKey] ?: @".*";
    NSArray *webKitResourceTypes = [self _convertedResourceTypesForChromeResourceTypes:[self _allChromeResourceTypesForCondition:condition]];

    if (filter) {
        triggerDictionary[@"url-filter"] = filter;

        // WebKit defaults `url-filter-is-case-sensitive` to `NO`, so we only need to include for `YES`.
        if (isCaseSensitive.boolValue)
            triggerDictionary[@"url-filter-is-case-sensitive"] = isCaseSensitive;
    }

    if ([chromeActionType isEqualToString:declarativeNetRequestRuleActionTypeModifyHeaders]) {
        NSArray<NSDictionary *> *requestHeadersInfo = _action[declarativeNetRequestRuleRequestHeadersKey];
        NSArray<NSDictionary *> *responseHeadersInfo = _action[declarativeNetRequestRuleResponseHeadersKey];

        actionDictionary[@"request-headers"] = requestHeadersInfo;
        actionDictionary[@"response-headers"] = responseHeadersInfo;
        actionDictionary[@"priority"] = @(_priority);
    }

    if ([chromeActionType isEqualToString:declarativeNetRequestRuleActionTypeRedirect]) {
        NSDictionary<NSString *, id> *inputRedirectDictonary = _action[declarativeNetRequestRuleActionRedirect];
        NSMutableDictionary<NSString *, id> *outputRedirectDictonary = [NSMutableDictionary dictionary];

        outputRedirectDictonary[@"url"] = inputRedirectDictonary[declarativeNetRequestRuleRedirectURL];
        outputRedirectDictonary[@"regex-substitution"] = inputRedirectDictonary[declarativeNetRequestRuleRedirectRegexSubstitution];
        outputRedirectDictonary[@"extension-path"] = inputRedirectDictonary[declarativeNetRequestRuleRedirectExtensionPath];

        NSMutableDictionary<NSString *, id> *transformDictonary = [inputRedirectDictonary[declarativeNetRequestRuleRedirectTransform] mutableCopy];
        NSMutableDictionary<NSString *, id> *queryTransformDictonary = [transformDictonary[declarativeNetRequestRuleURLTransformQueryTransform] mutableCopy];
        NSArray *addOrReplaceParamsArray = queryTransformDictonary[declarativeNetRequestRuleQueryTransformAddOrReplaceParams];
        NSArray *removeParamsArray = queryTransformDictonary[declarativeNetRequestRuleQueryTransformRemoveParams];

        transformDictonary[@"query-transform"] = queryTransformDictonary;
        transformDictonary[declarativeNetRequestRuleURLTransformQueryTransform] = nil;

        addOrReplaceParamsArray = mapObjects(addOrReplaceParamsArray, ^id(id key, NSDictionary<NSString *, id> *addOrReplaceParamsDictionary) {
            NSNumber *replaceOnly = addOrReplaceParamsDictionary[declarativeNetRequestRuleAddOrReplaceParamsReplaceOnly];
            if (!replaceOnly)
                return addOrReplaceParamsDictionary;

            NSMutableDictionary<NSString *, id> *modifiedCopy = [addOrReplaceParamsDictionary mutableCopy];

            modifiedCopy[@"replace-only"] = replaceOnly;
            modifiedCopy[declarativeNetRequestRuleAddOrReplaceParamsReplaceOnly] = nil;

            return modifiedCopy;
        });

        queryTransformDictonary[@"add-or-replace-parameters"] = addOrReplaceParamsArray;
        queryTransformDictonary[declarativeNetRequestRuleQueryTransformAddOrReplaceParams] = nil;

        queryTransformDictonary[@"remove-parameters"] = removeParamsArray;
        queryTransformDictonary[declarativeNetRequestRuleQueryTransformRemoveParams] = nil;

        outputRedirectDictonary[@"transform"] = transformDictonary;

        actionDictionary[@"redirect"] = outputRedirectDictonary;
    }

    if (webKitResourceTypes)
        triggerDictionary[@"resource-type"] = webKitResourceTypes;

    if (NSString *domainType = condition[declarativeNetRequestRuleConditionDomainTypeKey])
        triggerDictionary[@"load-type"] = @[ [self _chromeDomainTypeToWebKitDomainType][domainType] ];

    id (^includeSubdomainConversionBlock)(id, NSString *) = ^(id key, NSString *domain) {
        if ([domain hasPrefix:@"*"])
            return domain;

        return [@"*" stringByAppendingString:domain];
    };

    if (NSArray *domains = condition[declarativeNetRequestRuleConditionDomainsKey])
        triggerDictionary[@"if-domain"] = mapObjects(domains, includeSubdomainConversionBlock);
    else if (NSArray *excludedDomains = condition[declarativeNetRequestRuleConditionExcludedDomainsKey])
        triggerDictionary[@"unless-domain"] = mapObjects(excludedDomains, includeSubdomainConversionBlock);

    id (^convertToURLRegexBlock)(id, NSString *) = ^(id key, NSString *domain) {
        static NSString *regexDomainString = @"^[^:]+://+([^:/]+\\.)?";
        return [[regexDomainString stringByAppendingString:escapeCharactersInString(domain, @"*?+[(){}^$|\\.")] stringByAppendingString:@"/.*"];
    };

    if (NSArray *domains = condition[ruleConditionInitiatorDomainsKey])
        triggerDictionary[@"if-frame-url"] = mapObjects(domains, convertToURLRegexBlock);
    else if (NSArray *excludedDomains = condition[ruleConditionExcludedInitiatorDomainsKey])
        triggerDictionary[@"unless-frame-url"] = mapObjects(excludedDomains, convertToURLRegexBlock);

    if (NSString *requestMethod = condition[declarativeNetRequestRuleConditionRequestMethodsKey])
        triggerDictionary[@"request-method"] = requestMethod;

    return [convertedRule copy];
}

- (NSDictionary *)_chromeDomainTypeToWebKitDomainType
{
    static NSDictionary *domainTypes = @{
        @"firstParty": @"first-party",
        @"thirdParty": @"third-party",
    };

    return domainTypes;
}

- (NSDictionary *)_chromeResourceTypeToWebKitResourceType
{
    static NSDictionary *resourceTypes = @{
        @"font": @"font",
        @"image": @"image",
        @"main_frame": @"top-document",
        @"media": @"media",
        @"other": @"other",
        @"ping": @"ping",
        @"script": @"script",
        @"stylesheet": @"style-sheet",
        @"sub_frame": @"child-document",
        @"websocket": @"websocket",
        @"xmlhttprequest": @"fetch",
    };

    return resourceTypes;
}

- (NSArray<NSString *> *)_resourcesToTargetWhenNoneAreSpecifiedInRule
{
    static NSArray *resourceTypesExceptMainFrame;
    if (!resourceTypesExceptMainFrame) {
        NSMutableDictionary *allResourceTypes = [[self _chromeResourceTypeToWebKitResourceType] mutableCopy];
        [allResourceTypes removeObjectForKey:@"main_frame"];
        resourceTypesExceptMainFrame = allResourceTypes.allKeys;
    }

    return resourceTypesExceptMainFrame;
}

- (NSArray<NSString *> *)_allChromeResourceTypesForCondition:(NSDictionary *)condition
{
    NSArray<NSString *> *includedResourceTypes = condition[declarativeNetRequestRuleConditionResourceTypeKey];
    NSArray<NSString *> *excludedResourceTypes = condition[declarativeNetRequestRuleConditionExcludedResourceTypesKey];
    if (!includedResourceTypes && !excludedResourceTypes)
        return [self _resourcesToTargetWhenNoneAreSpecifiedInRule];

    if (includedResourceTypes)
        return [NSSet setWithArray:includedResourceTypes].allObjects;

    NSMutableDictionary *allResourceTypesExceptExcludedTypes = [[self _chromeResourceTypeToWebKitResourceType] mutableCopy];
    for (NSString *resourceType in excludedResourceTypes)
        [allResourceTypesExceptExcludedTypes removeObjectForKey:resourceType];

    return allResourceTypesExceptExcludedTypes.allKeys;
}

- (NSArray *)_convertedResourceTypesForChromeResourceTypes:(NSArray *)chromeResourceTypes
{
    /* FIXME: Handle all the other resource types:
         <rdar://71868297> Adopt WebKit's version of csp_report.
         <rdar://75042795> Adopt WebKit's version of object.
     */
    NSDictionary<NSString *, NSString *> *chromeResourceTypeToWebKitResourceType = [self _chromeResourceTypeToWebKitResourceType];
    return mapObjects(chromeResourceTypes, ^id(id key, NSString *resourceType) {
        return chromeResourceTypeToWebKitResourceType[resourceType];
    });
}

- (NSString *)_findLongestCommonSubstringWithString:(NSString *)string1 andString:(NSString *)string2
{
    if (!string1.length || !string2.length)
        return nil;

    NSString *longestCommonSubstring = @"";

    for (NSUInteger i = 0; i < string1.length; i++) {
        for (NSUInteger j = 0; j < string2.length; j++) {
            if ([string1 characterAtIndex:i] == [string2 characterAtIndex:j]) {
                NSUInteger k = 1;

                while (i + k < string1.length && j + k < string2.length && [string1 characterAtIndex:i + k] == [string2 characterAtIndex:j + k])
                    k++;

                if (k > 0) {
                    NSString *currentSubstring = [string1 substringWithRange:NSMakeRange(i, k)];

                    if (currentSubstring.length > longestCommonSubstring.length)
                        longestCommonSubstring = currentSubstring;
                }
            }
        }
    }

    return longestCommonSubstring.length ? longestCommonSubstring : nil;
}

- (NSString *)_combineRequestDomain:(NSString *)requestDomain withURLFilter:(NSString *)urlFilter
{
    // If we don't have a URL filter, just return the request domain; there's nothing to combine here.
    // Also add the domain name anchor to make sure that the request domain only matches domains though.
    if (!urlFilter)
        return [@"||" stringByAppendingString:requestDomain];

    // If the URL filter contains the request domain, we can just use the URL filter.
    //
    // E.g.
    // requestDomain = com, urlFilter = .com/foo/bar, result = \\.com/foo/bar
    NSString *trimmedDomain = requestDomain;
    NSString *trimmedFilter = urlFilter;
    while ([trimmedDomain hasPrefix:@"."])
        trimmedDomain = [trimmedDomain substringFromIndex:1];
    while ([trimmedFilter hasPrefix:@"."])
        trimmedFilter = [trimmedFilter substringFromIndex:1];

    if ([trimmedFilter hasPrefix:requestDomain])
        return [urlFilter hasPrefix:@"||"] ? urlFilter : [@"||" stringByAppendingString:urlFilter];

    // If part of the request domain is in the URL filter, insert the entire request domain into the URL filter at that place.
    //
    // E.g.
    // requestDomain = foo.com, urlFilter = foo.*/bar/baz, result = foo.com*/bar/baz
    // requstDomain = foo.com, urlFilter = bar-*.com^, result = bar-*foo.com^
    if (NSString *longestCommonSubstring = [self _findLongestCommonSubstringWithString:requestDomain andString:urlFilter]) {
        if (longestCommonSubstring.length > 1) {
            NSString *modifiedURLFilter = [urlFilter hasPrefix:@"||"] ? urlFilter : [@"||" stringByAppendingString:urlFilter];
            return [modifiedURLFilter stringByReplacingCharactersInRange:[modifiedURLFilter rangeOfString:longestCommonSubstring] withString:requestDomain];
        }
    }

    // If the URL filter is prefixed with the domain name anchor or ://, we need to append/insert the request domain into the URL filter.
    //
    // E.g.
    // requestDomain = com, urlFilter = ||foo, result = ||foo*com
    // requestDomain = com, urlFilter = ://www., result = ://www.*com
    // requestDomain = com, urlFilter = ://www.*/foo/bar, result = ://www.*com/foo/bar
    if ([urlFilter hasPrefix:@"||"] || [urlFilter hasPrefix:@"://"]) {
        if ([[urlFilter stringByReplacingOccurrencesOfString:@"://" withString:@""] containsString:@"/"]) {
            NSMutableArray<NSString *> *urlFilterParts = [[[urlFilter substringFromIndex:3] componentsSeparatedByString:@"/"] mutableCopy];

            if (![urlFilterParts.firstObject hasSuffix:@"*"])
                [urlFilterParts replaceObjectAtIndex:0 withObject:[[urlFilterParts.firstObject stringByAppendingString:@"*"] stringByAppendingString:requestDomain]];
            else
                [urlFilterParts replaceObjectAtIndex:0 withObject:[urlFilterParts.firstObject stringByAppendingString:requestDomain]];

            return [@"://" stringByAppendingString:[urlFilterParts componentsJoinedByString:@"/"]];
        }

        return [urlFilter hasSuffix:@"*"] ?
            [urlFilter stringByAppendingString:requestDomain] :
            [[urlFilter stringByAppendingString:@"*"] stringByAppendingString:requestDomain];
    }

    // Otherwise, we can just combine the request domain and URL filter with a wildcard character sandwiched in between.
    //
    // E.g.
    // requestDomain = com, urlFilter = /foo/bar, result = com*/foo/bar
    // requestDomain = com, urlFilter = &foo=bar|, result = com*&foo=bar|
    return [@"||" stringByAppendingString:[[requestDomain stringByAppendingString:@"*"] stringByAppendingString:urlFilter]];
}

- (NSString *)_regexURLFilterForChromeURLFilter:(NSString *)chromeURLFilter
{
    if (!chromeURLFilter.length)
        return nil;

    // Documentation: https://developer.chrome.com/docs/extensions/reference/declarativeNetRequest/

    // Supported special charcters:
    // '*' : Wildcard: Matches any number of characters.
    // '|' : Left/right anchor: If used at either end of the pattern, specifies the beginning/end of the url respectively.
    // '||' : Domain name anchor: If used at the beginning of the pattern, specifies the start of a (sub-)domain of the URL.
    // '^' : Separator character: This matches anything except a letter, a digit or one of the following: _ - . %.

    // Therefore urlFilter is composed of the following parts: (optional Left/Domain name anchor) + pattern + (optional Right anchor).
    // All other regex special charaters are escaped in the pattern.

    BOOL hasDomainNameAnchor = [chromeURLFilter hasPrefix:@"||"];
    if (hasDomainNameAnchor)
        chromeURLFilter = [chromeURLFilter substringFromIndex:2];

    BOOL hasStartAnchor = !hasDomainNameAnchor && [chromeURLFilter hasPrefix:@"|"];
    if (hasStartAnchor)
        chromeURLFilter = [chromeURLFilter substringFromIndex:1];

    BOOL hasEndAnchor = [chromeURLFilter hasSuffix:@"|"];
    if (hasEndAnchor)
        chromeURLFilter = [chromeURLFilter substringToIndex:chromeURLFilter.length - 1];

    NSString *regexFilter = escapeCharactersInString(chromeURLFilter, @"?+[(){}$|\\.");

    regexFilter = [regexFilter stringByReplacingOccurrencesOfString:@"*" withString:@".*"];
    regexFilter = [regexFilter stringByReplacingOccurrencesOfString:@"^" withString:@"[^a-zA-Z0-9_.%-]"];

    if (hasDomainNameAnchor)
        regexFilter = [@"^[^:]+://+([^:/]+\\.)?" stringByAppendingString:regexFilter];

    if (hasStartAnchor)
        regexFilter = [@"^" stringByAppendingString:regexFilter];

    if (hasEndAnchor)
        regexFilter = [regexFilter stringByAppendingString:@"$"];

    return regexFilter;
}

- (NSComparisonResult)compare:(_WKWebExtensionDeclarativeNetRequestRule *)rule
{
    if (_priority < rule.priority)
        return NSOrderedDescending;
    if (_priority > rule.priority)
        return NSOrderedAscending;

    if (priorityForRuleType(_action[declarativeNetRequestRuleActionTypeKey]) < priorityForRuleType(rule.action[declarativeNetRequestRuleActionTypeKey]))
        return NSOrderedDescending;
    if (priorityForRuleType(_action[declarativeNetRequestRuleActionTypeKey]) > priorityForRuleType(rule.action[declarativeNetRequestRuleActionTypeKey]))
        return NSOrderedAscending;

    return NSOrderedSame;
}

static NSInteger priorityForRuleType(NSString *ruleType)
{
    if ([ruleType isEqualToString:declarativeNetRequestRuleActionTypeAllow])
        return DeclarativeNetRequestRuleActionTypeAllow;

    if ([ruleType isEqualToString:declarativeNetRequestRuleActionTypeAllowAllRequests])
        return DeclarativeNetRequestRuleActionTypeAllowAllRequests;

    if ([ruleType isEqualToString:declarativeNetRequestRuleActionTypeBlock])
        return DeclarativeNetRequestRuleActionTypeBlock;

    if ([ruleType isEqualToString:declarativeNetRequestRuleActionTypeUpgradeScheme])
        return DeclarativeNetRequestRuleActionTypeUpgradeScheme;

    if ([ruleType isEqualToString:declarativeNetRequestRuleActionTypeRedirect])
        return DeclarativeNetRequestRuleActionTypeRedirect;

    if ([ruleType isEqualToString:declarativeNetRequestRuleActionTypeModifyHeaders])
        return DeclarativeNetRequestRuleActionTypeModifyHeaders;

    ASSERT_NOT_REACHED();
    return -1;
}

- (NSString *)description
{
    return [[NSString alloc] initWithFormat:@"<%@:%p %@>", self.class, self, @{
        @"id" : @(_ruleID),
        @"priority" : @(_priority),
        @"action": _action,
        @"condition" : _condition,
    }];
}

@end

#else

@implementation _WKWebExtensionDeclarativeNetRequestRule

- (instancetype)initWithDictionary:(NSDictionary *)ruleDictionary rulesetID:(NSString *)rulesetID errorString:(NSString **)outErrorString
{
    return nil;
}

- (NSComparisonResult)compare:(_WKWebExtensionDeclarativeNetRequestRule *)rule
{
    return NSOrderedSame;
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
