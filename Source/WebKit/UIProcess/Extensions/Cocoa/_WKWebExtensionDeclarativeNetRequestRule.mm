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

#if ENABLE(WK_WEB_EXTENSIONS)

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "CocoaHelpers.h"
#import "WKContentRuleListInternal.h"
#import "WebExtensionUtilities.h"

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

- (instancetype)initWithDictionary:(NSDictionary *)ruleDictionary errorString:(NSString **)outErrorString
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

    // FIXME: <rdar://72159785> Make sure every rule ID is unique.
    _ruleID = objectForKey<NSNumber>(ruleDictionary, declarativeNetRequestRuleIDKey).integerValue;
    if (!_ruleID) {
        if (outErrorString)
            *outErrorString = @"Missing rule id.";

        return nil;
    }

    NSString *exceptionString;
    if (!validateDictionary(ruleDictionary, nil, requiredKeysInRuleDictionary, keyToExpectedValueTypeInRuleDictionary, &exceptionString)) {
        if (outErrorString)
            *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. %@", (long)_ruleID, exceptionString];

        return nil;
    }

    NSNumber *priorityAsNumber = objectForKey<NSNumber>(ruleDictionary, declarativeNetRequestRulePriorityKey);
    _priority = priorityAsNumber ? priorityAsNumber.integerValue : 1;

    if (_ruleID < 1) {
        if (outErrorString)
            *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. Rule id must be non-negative.", (long)_ruleID];

        return nil;
    }

    if (_priority < 1) {
        if (outErrorString)
            *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. Rule priority must be non-negative.", (long)_ruleID];

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
            *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. %@", (long)_ruleID, exceptionString];

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
            *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `%@` is not a supported action type.", (long)_ruleID, _action[declarativeNetRequestRuleActionTypeKey]];

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
    };

    if (!validateDictionary(_condition, nil, @[ ], keyToExpectedValueTypeInConditionDictionary, &exceptionString)) {
        if (outErrorString)
            *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. %@", (long)_ruleID, exceptionString];

        return nil;
    }

    if (_condition[declarativeNetRequestRuleConditionRegexFilterKey] && _condition[declarativeNetRequestRuleConditionURLFilterKey]) {
        if (outErrorString)
            *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. Define only one of the `regexFilter` or `urlFilter` keys.", (long)_ruleID];

        return nil;
    }

    if (NSString *regexFilter = _condition[declarativeNetRequestRuleConditionRegexFilterKey]) {
        if (![regexFilter canBeConvertedToEncoding:NSASCIIStringEncoding]) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `regexFilter` cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }

        if (![WKContentRuleList _supportsRegularExpression:regexFilter]) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `regexFilter` is not a supported regular expression.", (long)_ruleID];

            return nil;
        }
    }

    if (NSString *urlFilter = _condition[declarativeNetRequestRuleConditionURLFilterKey]) {
        if (![urlFilter canBeConvertedToEncoding:NSASCIIStringEncoding]) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `urlFilter` cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (_condition[declarativeNetRequestRuleConditionResourceTypeKey] && _condition[declarativeNetRequestRuleConditionExcludedResourceTypesKey]) {
        if (outErrorString)
            *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. Define only one of the `resourceTypes` or `excludedResourceTypes` keys.", (long)_ruleID];

        return nil;
    }

    NSArray<NSString *> *resourceTypes = _condition[declarativeNetRequestRuleConditionResourceTypeKey];
    if (resourceTypes) {
        if (!resourceTypes.count) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `resourceTypes` cannot be an empty array.", (long)_ruleID];

            return nil;
        }

        [self removeInvalidResourceTypesForKey:declarativeNetRequestRuleConditionResourceTypeKey];
    }

    if (NSArray<NSString *> *excludedResourceTypes = _condition[declarativeNetRequestRuleConditionExcludedResourceTypesKey])
        [self removeInvalidResourceTypesForKey:declarativeNetRequestRuleConditionResourceTypeKey];

    if ([_action[declarativeNetRequestRuleActionTypeKey] isEqualToString:declarativeNetRequestRuleActionTypeAllowAllRequests]) {
        if (!resourceTypes) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. A rule with the `allowAllRequests` action type must have the `resourceTypes` key.", (long)_ruleID];

            return nil;
        }

        for (NSString *resourceType in resourceTypes) {
            if (![resourceType isEqualToString:@"main_frame"] && ![resourceType isEqualToString:@"sub_frame"]) {
                if (outErrorString)
                    *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `%@` is not a valid resource type for the `allowAllRequests` action type.", (long)_ruleID, resourceType];

                return nil;
            }
        }
    }

    if (NSString *domainType = _condition[declarativeNetRequestRuleConditionDomainTypeKey]) {
        if (![[self _chromeDomainTypeToWebKitDomainType] objectForKey:domainType]) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `%@` is not a valid domain type.", (long)_ruleID, domainType];

            return nil;
        }
    }

    if (NSArray<NSString *> *domains = _condition[declarativeNetRequestRuleConditionDomainsKey]) {
        if (!isArrayOfDomainsValid(domains)) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `domains` must be non-empty and cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (NSArray<NSString *> *domains = _condition[ruleConditionRequestDomainsKey]) {
        if (!isArrayOfDomainsValid(domains)) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `requestDomains` must be non-empty and cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (NSArray<NSString *> *excludedDomains = _condition[declarativeNetRequestRuleConditionExcludedDomainsKey]) {
        if (!isArrayOfExcludedDomainsValid(excludedDomains)) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `excludedDomains` cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (NSArray<NSString *> *excludedDomains = _condition[ruleConditionExcludedRequestDomainsKey]) {
        if (!isArrayOfExcludedDomainsValid(excludedDomains)) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `excludedRequestDomains` cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (NSArray<NSString *> *initiatorDomains = _condition[ruleConditionInitiatorDomainsKey]) {
        if (!isArrayOfDomainsValid(initiatorDomains)) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `initiatorDomains` must be non-empty and cannot contain non-ASCII characters.", (long)_ruleID];

            return nil;
        }
    }

    if (NSArray<NSString *> *excludedInitiatorDomains = _condition[ruleConditionExcludedInitiatorDomainsKey]) {
        if (!isArrayOfExcludedDomainsValid(excludedInitiatorDomains)) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `excludedInitiatorDomains` cannot contain non-ASCII characters.", (long)_ruleID];

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
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `redirect` is missing either a `url`, `extensionPath`, `regexSubstitution`, or `transform` key.", (long)_ruleID];

            return nil;
        }

        // FIXME: rdar://105890168 (declarativeNetRequest: Only allow one of transform, url, extensionPath or regexSubstitution in redirect actions)

        if (urlString) {
            NSURL *url = [NSURL URLWithString:urlString];
            if (!url) {
                if (outErrorString)
                    *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `redirect` specified an invalid or empty `url`.", (long)_ruleID];

                return nil;
            }

            if (!URL(url).protocolIsInHTTPFamily()) {
                if (outErrorString)
                    *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `redirect` specified a non-HTTP `url`.", (long)_ruleID];

                return nil;
            }
        }

        if (regexSubstitutionString) {
            if (!_condition[declarativeNetRequestRuleConditionRegexFilterKey]) {
                if (outErrorString)
                    *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `redirect` specified a `regexSubstitution` without a `regexFilter` condition.", (long)_ruleID];

                return nil;
            }
        }

        if (extensionPathString && ![extensionPathString hasPrefix:@"/"]) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `redirect` specified an `extensionPath` without a '/' prefix.", (long)_ruleID];

            return nil;
        }

        if (transformDictionary && !transformDictionary.count) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `redirect` specified an invalid or empty `transform`.", (long)_ruleID];

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
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `redirect` specified an invalid `transform`. %@", (long)_ruleID, exceptionString];

            return nil;
        }

        NSDictionary<NSString *, id> *queryTransformDictionary = objectForKey<NSDictionary>(transformDictionary, declarativeNetRequestRuleURLTransformQueryTransform, false);
        if (queryTransformDictionary && !queryTransformDictionary.count) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `transform` specified an invalid or empty `queryTransform`.", (long)_ruleID];

            return nil;
        }

        static NSDictionary *keyToExpectedValueTypeInQueryTransformDictionary = @{
            declarativeNetRequestRuleQueryTransformAddOrReplaceParams: @[ NSDictionary.class ],
            declarativeNetRequestRuleQueryTransformRemoveParams: @[ NSString.class ],
        };

        if (queryTransformDictionary && !validateDictionary(queryTransformDictionary, nil, @[ ], keyToExpectedValueTypeInQueryTransformDictionary, &exceptionString)) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `transform` specified an invalid `queryTransform`. %@", (long)_ruleID, exceptionString];

            return nil;
        }

        NSArray<NSString *> *removeParamsArray = queryTransformDictionary[declarativeNetRequestRuleQueryTransformRemoveParams];
        if (removeParamsArray && !removeParamsArray.count) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `queryTransform` specified an invalid or empty `removeParams`.", (long)_ruleID];

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
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `queryTransform` specified an invalid or empty `addOrReplaceParams`.", (long)_ruleID];

            return nil;
        }

        for (NSDictionary<NSString *, id> *addOrReplaceParamsDictionary in addOrReplaceParamsArray) {
            if (!validateDictionary(addOrReplaceParamsDictionary, nil, requiredKeysInAddOrReplaceDictionary, keyToExpectedValueTypeInAddOrReplaceDictionary, &exceptionString)) {
                if (outErrorString)
                    *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. `queryTransform` specified an invalid `addOrReplaceParams`. %@", (long)_ruleID, exceptionString];

                return nil;
            }
        }
    }

    if ([_action[declarativeNetRequestRuleActionTypeKey] isEqual:declarativeNetRequestRuleActionTypeModifyHeaders]) {
        NSArray<NSDictionary *> *requestHeadersInfo = _action[declarativeNetRequestRuleRequestHeadersKey];
        NSArray<NSDictionary *> *responseHeadersInfo = _action[declarativeNetRequestRuleResponseHeadersKey];

        if (!requestHeadersInfo && !responseHeadersInfo) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. A modifyHeaders rule must have `requestHeaders` or `responseHeaders` set.", (long)_ruleID];

            return nil;
        }

        if ((requestHeadersInfo && !requestHeadersInfo.count) || (responseHeadersInfo && !responseHeadersInfo.count)) {
            if (outErrorString)
                *outErrorString = [NSString stringWithFormat:@"Rule with id %ld is invalid. The arrays specified by `requestHeaders` or `responseHeaders` must be non-empty.", (long)_ruleID];

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
        return [NSString stringWithFormat:@"Rule with id %ld is invalid. One of the headers dictionaries is not formatted correctly. %@", (long)_ruleID, exceptionString];

    NSString *operationType = headerInfo[declarativeNetRequestRuleHeaderOperationKey];
    BOOL isSetOperation = [operationType isEqual:declarativeNetRequestRuleHeaderOperationValueSet];
    BOOL isAppendOperation = [operationType isEqual:declarativeNetRequestRuleHeaderOperationValueAppend];
    BOOL isRemoveOperation = [operationType isEqual:declarativeNetRequestRuleHeaderOperationValueRemove];

    if (!isSetOperation && !isAppendOperation && !isRemoveOperation)
        return [NSString stringWithFormat:@"Rule with id %ld is invalid. `%@` is not a recognized header operation.", (long)_ruleID, operationType];

    NSString *headerName = headerInfo[declarativeNetRequestRuleHeaderKey];
    if (!isHeaderNameValid(headerName))
        return [NSString stringWithFormat:@"Rule with id %ld is invalid. The header `%@` is not recognized.", (long)_ruleID, headerName];

    NSString *headerValue = headerInfo[declarativeNetRequestRuleHeaderValueKey];
    if (isRemoveOperation && headerValue)
        return [NSString stringWithFormat:@"Rule with id %ld is invalid. Do not provide a value when removing a header.", (long)_ruleID];

    if ((isSetOperation || isAppendOperation) && !headerValue)
        return [NSString stringWithFormat:@"Rule with id %ld is invalid. You must provide a value when modifying a header.", (long)_ruleID];

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
        @"x-dns-prefetch-control",
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
        declarativeNetRequestRuleActionTypeAllow: @"ignore-previous-rules",
        declarativeNetRequestRuleActionTypeAllowAllRequests: @"ignore-previous-rules",
        declarativeNetRequestRuleActionTypeBlock: @"block",
        declarativeNetRequestRuleActionTypeModifyHeaders: @"modify-headers",
        declarativeNetRequestRuleActionTypeRedirect: @"redirect",
        declarativeNetRequestRuleActionTypeUpgradeScheme: @"make-https",
    };

    NSMutableArray<NSDictionary<NSString *, id> *> *convertedRules = [NSMutableArray array];

    NSString *webKitActionType = chromeActionTypesToWebKitActionTypes[_action[declarativeNetRequestRuleActionTypeKey]];
    NSString *chromeActionType = _action[declarativeNetRequestRuleActionTypeKey];
    if ([webKitActionType isEqualToString:@"make-https"]) {
        NSArray *rulesToMaintainOrderingOfUpgradeSchemeRule = [self _convertedRulesForWebKitActionType:@"ignore-previous-rules" chromeActionType:chromeActionType];
        [convertedRules addObjectsFromArray:rulesToMaintainOrderingOfUpgradeSchemeRule];
    }

    [convertedRules addObjectsFromArray:[self _convertedRulesForWebKitActionType:webKitActionType chromeActionType:chromeActionType]];

    if (_condition[ruleConditionInitiatorDomainsKey] && _condition[ruleConditionExcludedInitiatorDomainsKey]) {
        // If a rule specifies both initiatorDomains and excludedInitiatorDomains, we need to turn that into two rules. The first rule will have the excludedInitiatorDomains, and be implemented
        // as an ignore-previous-rules using if-frame-url (instead of unless-frame-url).
        // To do this, make a copy of the condition dictionary, make the initiatorDomains be the excludedInitiatorDomains of the original rule, and create an ignore-previous-rules rule.
        NSDictionary *originalCondition = [_condition copy];

        NSMutableDictionary *modifiedCondition = [_condition mutableCopy];

        modifiedCondition[ruleConditionInitiatorDomainsKey] = modifiedCondition[ruleConditionExcludedInitiatorDomainsKey];
        modifiedCondition[ruleConditionExcludedInitiatorDomainsKey] = nil;

        _condition = [modifiedCondition copy];
        [convertedRules addObjectsFromArray:[self _convertedRulesForWebKitActionType:@"ignore-previous-rules" chromeActionType:chromeActionType]];

        _condition = [originalCondition copy];
    }

    return [convertedRules copy];
}

- (NSArray<NSDictionary *> *)_convertedRulesForWebKitActionType:(NSString *)webKitActionType chromeActionType:(NSString *)chromeActionType
{
    NSMutableArray *convertedRules = [NSMutableArray array];

    NSMutableArray<NSString *> *chromeResourceTypes = [[self _allChromeResourceTypes] mutableCopy];
    BOOL ruleBlocksMainFrame = [chromeResourceTypes containsObject:@"main_frame"];
    BOOL ruleBlocksSubFrame = [chromeResourceTypes containsObject:@"sub_frame"];
    NSArray<NSString *> *frameResourceTypes;
    if (ruleBlocksMainFrame && ruleBlocksSubFrame)
        frameResourceTypes = @[ @"main_frame", @"sub_frame" ];
    else if (ruleBlocksMainFrame)
        frameResourceTypes = @[ @"main_frame" ];
    else if (ruleBlocksSubFrame)
        frameResourceTypes = @[ @"sub_frame" ];

    if (frameResourceTypes)
        [convertedRules addObject:[self _webKitRuleWithWebKitActionType:webKitActionType chromeActionType:chromeActionType chromeResourceTypes:frameResourceTypes]];

    [chromeResourceTypes removeObjectsInArray:@[ @"main_frame", @"sub_frame" ]];
    if (chromeResourceTypes.count)
        [convertedRules addObject:[self _webKitRuleWithWebKitActionType:webKitActionType chromeActionType:chromeActionType chromeResourceTypes:chromeResourceTypes]];

    return [convertedRules copy];
}

- (NSDictionary *)_webKitRuleWithWebKitActionType:(NSString *)webKitActionType chromeActionType:(NSString *)chromeActionType chromeResourceTypes:(NSArray *)chromeResourceTypes
{
    NSString *filter;
    NSArray *webKitResourceTypes;
    BOOL isRuleForAllowAllRequests = [chromeActionType isEqualToString:declarativeNetRequestRuleActionTypeAllowAllRequests];
    if (isRuleForAllowAllRequests) {
        filter = @".*";
        // Intentionally leave the resourceTypes array empty to use all of WebKit's resource types.
    } else {
        filter = [self _regexURLFilterForChromeURLFilter:_condition[declarativeNetRequestRuleConditionURLFilterKey]] ?: _condition[declarativeNetRequestRuleConditionRegexFilterKey] ?: @".*";
        webKitResourceTypes = [self _convertedResourceTypesForChromeResourceTypes:chromeResourceTypes];
    }

    NSMutableDictionary *actionDictionary = [@{ @"type": webKitActionType } mutableCopy];
    NSMutableDictionary *triggerDictionary = [NSMutableDictionary dictionary];
    NSNumber *isCaseSensitive = _condition[declarativeNetRequestRuleConditionCaseSensitiveKey] ?: @NO;
    if (filter) {
        triggerDictionary[@"url-filter"] = filter;

        // WebKit defaults `url-filter-is-case-sensitive` to `NO`, so we only need to include for `YES`.
        if (isCaseSensitive.boolValue)
            triggerDictionary[@"url-filter-is-case-sensitive"] = isCaseSensitive;
    }

    NSDictionary<NSString *, id> *convertedRule = @{
        @"action": actionDictionary,
        @"trigger": triggerDictionary,
    };

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

    if (webKitResourceTypes) {
        NSDictionary<NSString *, NSString *> *chromeResourceTypeToWebKitLoadContext = [self _chromeResourceTypeToWebKitLoadContext];
        NSArray *loadContextsArray = mapObjects(chromeResourceTypes, ^id(id key, NSString *resourceType) {
            return chromeResourceTypeToWebKitLoadContext[resourceType];
        });

        if (loadContextsArray.count) {
            triggerDictionary[@"load-context"] = loadContextsArray;
            triggerDictionary[@"resource-type"] = @[ @"document" ];
        } else
            triggerDictionary[@"resource-type"] = webKitResourceTypes;
    }

    if (NSString *domainType = _condition[declarativeNetRequestRuleConditionDomainTypeKey])
        triggerDictionary[@"load-type"] = @[ [self _chromeDomainTypeToWebKitDomainType][domainType] ];

    id (^includeSubdomainConversionBlock)(id, NSString *) = ^(id key, NSString *domain) {
        if ([domain hasPrefix:@"*"])
            return domain;

        return [@"*" stringByAppendingString:domain];
    };

    if (NSArray *domains = _condition[declarativeNetRequestRuleConditionDomainsKey])
        triggerDictionary[@"if-domain"] = mapObjects(domains, includeSubdomainConversionBlock);
    else if (NSArray *domains = _condition[ruleConditionRequestDomainsKey])
        triggerDictionary[@"if-domain"] = mapObjects(domains, includeSubdomainConversionBlock);
    else if (NSArray *excludedDomains = _condition[declarativeNetRequestRuleConditionExcludedDomainsKey])
        triggerDictionary[@"unless-domain"] = mapObjects(excludedDomains, includeSubdomainConversionBlock);
    else if (NSArray *excludedDomains = _condition[ruleConditionExcludedRequestDomainsKey])
        triggerDictionary[@"unless-domain"] = mapObjects(excludedDomains, includeSubdomainConversionBlock);


    id (^convertToURLRegexBlock)(id, NSString *) = ^(id key, NSString *domain) {
        static NSString *regexDomainString = @"^[^:]+://+([^:/]+\\.)?";
        return [[regexDomainString stringByAppendingString:escapeCharactersInString(domain, @"*?+[(){}^$|\\.")] stringByAppendingString:@"/.*"];
    };

    // If `initiatorDomains` and `excludedInitiatorDomains` are specified, we will have already created a rule honoring the `excludedInitiatorDomains` with an `ignore-previous-action` (see rdar://139515419).
    // Therefore, honor the `initiatorDomains` first and drop the excluded ones on the floor for this rule.
    if (NSArray *domains = _condition[ruleConditionInitiatorDomainsKey])
        triggerDictionary[@"if-frame-url"] = mapObjects(domains, convertToURLRegexBlock);
    else if (NSArray *excludedDomains = _condition[ruleConditionExcludedInitiatorDomainsKey])
        triggerDictionary[@"unless-frame-url"] = mapObjects(excludedDomains, convertToURLRegexBlock);


    // FIXME: <rdar://72203692> Support 'allowAllRequests' when the resource type is 'sub_frame'.
    if (isRuleForAllowAllRequests)
        triggerDictionary[@"if-top-url"] = @[ [self _regexURLFilterForChromeURLFilter:_condition[declarativeNetRequestRuleConditionURLFilterKey]] ?: _condition[declarativeNetRequestRuleConditionRegexFilterKey] ?: @"" ];

    return [convertedRule copy];
}

- (NSDictionary *)_chromeResourceTypeToWebKitLoadContext
{
    static NSDictionary *chromeResourceTypeToWebKitLoadContext = @{
        @"main_frame": @"top-frame",
        @"sub_frame": @"child-frame",
    };

    return chromeResourceTypeToWebKitLoadContext;
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
        @"main_frame": @"document",
        @"media": @"media",
        @"other": @"other",
        @"ping": @"ping",
        @"script": @"script",
        @"stylesheet": @"style-sheet",
        @"sub_frame": @"document",
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

- (NSArray<NSString *> *)_allChromeResourceTypes
{
    NSArray<NSString *> *includedResourceTypes = _condition[declarativeNetRequestRuleConditionResourceTypeKey];
    NSArray<NSString *> *excludedResourceTypes = _condition[declarativeNetRequestRuleConditionExcludedResourceTypesKey];
    if (!includedResourceTypes && !excludedResourceTypes)
        return [self _resourcesToTargetWhenNoneAreSpecifiedInRule];

    if (includedResourceTypes)
        return includedResourceTypes;

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
        return NSOrderedAscending;
    if (_priority > rule.priority)
        return NSOrderedDescending;

    if (priorityForRuleType(_action[declarativeNetRequestRuleActionTypeKey]) < priorityForRuleType(rule.action[declarativeNetRequestRuleActionTypeKey]))
        return NSOrderedAscending;
    if (priorityForRuleType(_action[declarativeNetRequestRuleActionTypeKey]) > priorityForRuleType(rule.action[declarativeNetRequestRuleActionTypeKey]))
        return NSOrderedDescending;

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
    return [NSString stringWithFormat:@"<%@:%p %@>", self.class, self, @{
        @"id" : @(_ruleID),
        @"priority" : @(_priority),
        @"action": _action,
        @"condition" : _condition,
    }];
}

@end

#else

@implementation _WKWebExtensionDeclarativeNetRequestRule

- (instancetype)initWithDictionary:(NSDictionary *)ruleDictionary errorString:(NSString **)outErrorString
{
    return nil;
}

- (NSComparisonResult)compare:(_WKWebExtensionDeclarativeNetRequestRule *)rule
{
    return NSOrderedSame;
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
