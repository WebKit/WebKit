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

#import "config.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionUtilities.h"

#import <WebKit/_WKWebExtensionDeclarativeNetRequestRule.h>
#import <WebKit/_WKWebExtensionDeclarativeNetRequestTranslator.h>

namespace TestWebKitAPI {

TEST(WKWebExtensionAPIDeclarativeNetRequest, RequiredAndOptionalKeys)
{
    NSDictionary *rule1 = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule1 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule1 errorString:nil];
    EXPECT_NOT_NULL(validatedRule1);

    NSDictionary *rule2 = @{
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };
    _WKWebExtensionDeclarativeNetRequestRule *validatedRule2 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule2 errorString:nil];
    EXPECT_NULL(validatedRule2);

    NSDictionary *rule3 = @{
        @"id": @1,
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };
    _WKWebExtensionDeclarativeNetRequestRule *validatedRule3 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule3 errorString:nil];
    EXPECT_NULL(validatedRule3);

    NSDictionary *rule4 = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
    };
    _WKWebExtensionDeclarativeNetRequestRule *validatedRule4 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule4 errorString:nil];
    EXPECT_NULL(validatedRule4);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, PropertiesHaveCorrectType)
{
    NSDictionary *rule1 = @{
        @"id": @"rule_id",
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule1 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule1 errorString:nil];
    EXPECT_NULL(validatedRule1);

    NSDictionary *rule2 = @{
        @"id": @1,
        @"priority": @1,
        @"action": @"block",
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };
    _WKWebExtensionDeclarativeNetRequestRule *validatedRule2 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule2 errorString:nil];
    EXPECT_NULL(validatedRule2);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NumbersArePositiveIntegers)
{
    NSDictionary *rule1 = @{
        @"id": @-1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule1 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule1 errorString:nil];
    EXPECT_NULL(validatedRule1);

    NSDictionary *rule2 = @{
        @"id": @1,
        @"priority": @0,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule2 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule2 errorString:nil];
    EXPECT_NULL(validatedRule2);

    NSDictionary *ruleWithNonIntegerPriority = @{
        @"id": @80,
        @"priority": @5.8,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRuleWithNonIntegerPriority = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:ruleWithNonIntegerPriority errorString:nil];
    EXPECT_NOT_NULL(validatedRuleWithNonIntegerPriority);
    EXPECT_EQ(validatedRuleWithNonIntegerPriority.ruleID, 80);
    EXPECT_EQ(validatedRuleWithNonIntegerPriority.priority, 5);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnlyOneOfResourceTypesAndExcludedResourceTypesIsSpecified)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"boatnerd.com",
            @"resourceTypes": @[ @"font" ],
            @"excludedResourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RegexRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"regexFilter": @".*\\.com",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @".*\\.com",
            @"resource-type": @[ @"script" ],
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, BasicValidRuleParsing)
{
    NSDictionary *conditionDictionary = @{
        @"urlFilter": @"crouton.net",
        @"resourceTypes": @[ @"script" ],
    };

    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @3,
        @"action": @{ @"type": @"block" },
        @"condition": conditionDictionary,
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NOT_NULL(validatedRule);
    EXPECT_EQ(validatedRule.ruleID, 1);
    EXPECT_EQ(validatedRule.priority, 3);
    EXPECT_NS_EQUAL(validatedRule.action, @{ @"type": @"block" });
    EXPECT_NS_EQUAL(validatedRule.condition, conditionDictionary);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, BasicRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"font" ],
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, MainFrameResourceRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"main_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"document" ],
            @"load-context": @[ @"top-frame" ],
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, SubFrameResourceRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"allow" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"sub_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"ignore-previous-rules",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"document" ],
            @"load-context": @[ @"child-frame" ],

        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RepeatedMainFrameResourceRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"allow" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"main_frame", @"main_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"ignore-previous-rules",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"document" ],
            @"load-context": @[ @"top-frame" ],

        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, CaseSensitiveConversion)
{
    NSDictionary *rule1 = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"isUrlFilterCaseSensitive": @YES,
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule1 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule1 errorString:nil];
    NSDictionary *convertedRule1 = validatedRule1.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule1);

    NSDictionary *correctRuleConversion1 = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"url-filter-is-case-sensitive": @YES,
            @"resource-type": @[ @"font" ],
        },
    };
    EXPECT_NS_EQUAL(convertedRule1, correctRuleConversion1);

    NSDictionary *rule2 = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"isUrlFilterCaseSensitive": @NO,
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule2 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule2 errorString:nil];
    NSDictionary *convertedRule2 = validatedRule2.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule2);

    NSDictionary *correctRuleConversion2 = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"font" ],
        },
    };
    EXPECT_NS_EQUAL(convertedRule2, correctRuleConversion2);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, ConvertingMultipleResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script", @"stylesheet", @"ping" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"script", @"style-sheet", @"ping" ],
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, ConvertingXHRWebSocketAndOtherTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"xmlhttprequest", @"websocket", @"other" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"fetch", @"websocket", @"other" ],
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, UpgradeSchemeRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"upgradeScheme" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"image" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSArray<NSDictionary *> *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_EQ(convertedRules.count, 2ul);

    NSDictionary *sortingRule = @{
        @"action": @{
            @"type": @"ignore-previous-rules",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"image" ],
        },
    };

    NSDictionary *makeHTTPSRule = @{
        @"action": @{
            @"type": @"make-https",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"image" ],
        },
    };

    EXPECT_NS_EQUAL(convertedRules[0], sortingRule);
    EXPECT_NS_EQUAL(convertedRules[1], makeHTTPSRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, UpgradeSchemeForMainFrameRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"upgradeScheme" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"main_frame", @"image" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSArray<NSDictionary *> *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_EQ(convertedRules.count, 4ul);

    NSDictionary *sortingRuleForMainFrame = @{
        @"action": @{
            @"type": @"ignore-previous-rules",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"document" ],
            @"load-context": @[ @"top-frame" ],
        },
    };
    EXPECT_NS_EQUAL(convertedRules[0], sortingRuleForMainFrame);

    NSDictionary *sortingRuleForImage = @{
        @"action": @{
            @"type": @"ignore-previous-rules",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"image" ],
        },
    };
    EXPECT_NS_EQUAL(convertedRules[1], sortingRuleForImage);

    NSDictionary *makeHTTPSRuleForMainFrame = @{
        @"action": @{
            @"type": @"make-https",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"document" ],
            @"load-context": @[ @"top-frame" ],
        },
    };
    EXPECT_NS_EQUAL(convertedRules[2], makeHTTPSRuleForMainFrame);

    NSDictionary *makeHTTPSRuleForImage = @{
        @"action": @{
            @"type": @"make-https",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"image" ],
        },
    };
    EXPECT_NS_EQUAL(convertedRules[3], makeHTTPSRuleForImage);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleWithoutAPriority)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"upgradeScheme" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NOT_NULL(validatedRule);
    EXPECT_EQ(validatedRule.priority, 1);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleWithInvalidDomainType)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allow" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"domainType": @"secondParty",
            @"resourceTypes": @[ @"ping" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithDomainType)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"boatnerd.com",
            @"resourceTypes": @[ @"script", @"image", @"stylesheet" ],
            @"domainType": @"firstParty",
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"boatnerd\\.com",
            @"resource-type": @[ @"script", @"image", @"style-sheet" ],
            @"load-type": @[ @"first-party" ],
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithNoSpecifiedResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allow" },
        @"condition": @{
            @"regexFilter": @".*",
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSArray<NSDictionary *> *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_EQ(convertedRules.count, 2ul);

    NSDictionary *documentRule = @{
        @"action": @{
            @"type": @"ignore-previous-rules",
        },
        @"trigger": @{
            @"url-filter": @".*",
            @"resource-type": @[ @"document" ],
            @"load-context": @[ @"child-frame" ],
        },
    };
    EXPECT_NS_EQUAL(convertedRules[0], documentRule);

    NSDictionary *otherResourceTypeRules = @{
        @"action": @{
            @"type": @"ignore-previous-rules",
        },
        @"trigger": @{
            @"url-filter": @".*",
            @"resource-type": @[ @"fetch", @"font", @"image", @"media", @"other", @"ping", @"script", @"style-sheet", @"websocket" ],
        },
    };

    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRules[1][@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:otherResourceTypeRules[@"trigger"][@"resource-type"]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithUnsupportedResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"resourceTypes": @[ @"script", @"not_a_real_resource_type", @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRule[@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:@[ @"script", @"font" ]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithExactlyOneUnsupportedResourceType)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"resourceTypes": @[ @"not_a_real_resource_type" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NULL(convertedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, EmptyResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"resourceTypes": @[ ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithExcludedResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"excludedResourceTypes": @[ @"main_frame", @"sub_frame", @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRule[@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:@[ @"fetch", @"image", @"media", @"other", @"ping", @"script", @"style-sheet", @"websocket" ]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithUnsupportedExcludedResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"*",
            @"excludedResourceTypes": @[ @"image", @"🦠" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSArray<NSDictionary *> *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_EQ(convertedRules.count, 2ul);

    NSDictionary *documentRule = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @".*",
            @"resource-type": @[ @"document" ],
            @"load-context": @[ @"top-frame", @"child-frame" ],
        },
    };
    EXPECT_NS_EQUAL(convertedRules[0], documentRule);

    NSDictionary *otherResourceTypeRules = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @".*",
            @"resource-type": @[ @"fetch", @"font", @"media", @"other", @"ping", @"script", @"style-sheet", @"websocket" ],
        },
    };

    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRules[1][@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:otherResourceTypeRules[@"trigger"][@"resource-type"]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithEmptyExcludedResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"*",
            @"excludedResourceTypes": @[ ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSArray<NSDictionary *> *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_EQ(convertedRules.count, 2ul);

    NSDictionary *documentRule = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @".*",
            @"resource-type": @[ @"document" ],
            @"load-context": @[ @"top-frame", @"child-frame" ],
        },
    };
    EXPECT_NS_EQUAL(convertedRules[0], documentRule);

    NSDictionary *otherResourceTypeRules = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @".*",
            @"resource-type": @[ @"fetch", @"font", @"image", @"media", @"other", @"ping", @"script", @"style-sheet", @"websocket" ],
        },
    };

    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRules[1][@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:otherResourceTypeRules[@"trigger"][@"resource-type"]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, EmptyDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"domains": @[ ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NonASCIIDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"domains": @[ @"🙃🙃🙃", @"apple.com" ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"domains": @[ @"apple.com", @"facebook.com", @"google.com" ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"if-domain": @[ @"*apple.com", @"*facebook.com", @"*google.com" ],
            @"resource-type": @[ @"font" ],
            @"url-filter": @".*",
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithRequestDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            // Some extensions will have added leading * to match subdomains to work around Safari's bug in the past (rdar://113865040), make sure we still handle that case correctly.
            @"requestDomains": @[ @"apple.com", @"facebook.com", @"*google.com" ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"if-domain": @[ @"*apple.com", @"*facebook.com", @"*google.com" ],
            @"resource-type": @[ @"font" ],
            @"url-filter": @".*",
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithEmptyExcludedDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"excludedDomains": @[ ],
            @"resourceTypes": @[ @"media" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"unless-domain": @[ ],
            @"resource-type": @[ @"media" ],
            @"url-filter": @".*",
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NonASCIIExcludedDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"domains": @[ @"🧭" ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithExcludedDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"excludedDomains": @[ @"apple.com" ],
            @"resourceTypes": @[ @"media" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"unless-domain": @[ @"*apple.com" ],
            @"resource-type": @[ @"media" ],
            @"url-filter": @".*",
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithExcludedRequestDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"excludedRequestDomains": @[ @"apple.com" ],
            @"resourceTypes": @[ @"media" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"unless-domain": @[ @"*apple.com" ],
            @"resource-type": @[ @"media" ],
            @"url-filter": @".*",
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NonASCIIURLFilter)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"🔮.com",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NonASCIIRegexFilter)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"regexFilter": @"〽️.com",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, URLFilterSpecialCharacters)
{
    __auto_type testPattern = ^(NSString *chromePattern, NSString *expectedRegexPattern) {
        NSDictionary *rule = @{
            @"id": @1,
            @"action": @{ @"type": @"block" },
            @"condition": @{
                @"urlFilter": chromePattern,
                @"resourceTypes": @[ @"script" ],
            },
        };

        _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
        NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
        EXPECT_NOT_NULL(convertedRule);

        NSDictionary *correctRuleConversion = @{
            @"action": @{
                @"type": @"block",
            },
            @"trigger": @{
                @"url-filter": expectedRegexPattern,
                @"resource-type": @[ @"script" ],
            },
        };

        EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
    };

    // Testing domain anchor.
    testPattern(@"||apple.com", @"^[^:]+://+([^:/]+\\.)?apple\\.com");
    testPattern(@"apple||.com", @"apple\\|\\|\\.com");

    // Testing start and end anchors.
    testPattern(@"|apple|.com|", @"^apple\\|\\.com$");
    testPattern(@"|apple.com", @"^apple\\.com");
    testPattern(@"apple.com|", @"apple\\.com$");
    testPattern(@"apple|com", @"apple\\|com");

    // Testing wildcard.
    testPattern(@"*apple.com", @".*apple\\.com");
    testPattern(@"*apple*com", @".*apple.*com");
    testPattern(@"apple.com*", @"apple\\.com.*");

    // Testing separator character.
    testPattern(@"apple^com", @"apple[^a-zA-Z0-9_.%-]com");
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, UnacceptableResourceTypeForAllowAllRequests)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allowAllRequests" },
        @"condition": @{
            @"urlFilter": @"apple.com",
            @"resourceTypes": @[ @"main_frame", @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, ExcludedResourceTypeForAllowAllRequests)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allowAllRequests" },
        @"condition": @{
            @"urlFilter": @"apple.com",
            @"excludedResourceTypes": @[ @"sub_frame", @"stylesheet", @"script", @"image", @"font", @"object", @"xmlhttprequest", @"ping", @"csp_report", @"media", @"websocket", @"other"],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NoResourceTypeForAllowAllRequests)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allowAllRequests" },
        @"condition": @{
            @"urlFilter": @"apple.com",
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithAllowAllRequests)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allowAllRequests" },
        @"condition": @{
            @"urlFilter": @"apple.com",
            @"resourceTypes": @[ @"main_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"ignore-previous-rules",
        },
        @"trigger": @{
            @"url-filter": @".*",
            @"if-top-url": @[ @"apple\\.com" ],
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithRedirect)
{
    __auto_type testRedirect = ^(NSDictionary<NSString *, id> *inputRedirect, NSDictionary<NSString *, id> *expectedRedirect) {
        NSDictionary *rule = @{
            @"id": @1,
            @"action": @{
                @"type": @"redirect",
                @"redirect": inputRedirect,
            },
            @"condition": @{
                @"regexFilter": @".*",
                @"resourceTypes": @[ @"script" ],
            },
        };

        _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
        NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;

        if (!expectedRedirect) {
            EXPECT_NULL(convertedRule);
            return;
        }

        EXPECT_NOT_NULL(convertedRule);

        NSDictionary *correctRuleConversion = @{
            @"action": @{
                @"type": @"redirect",
                @"redirect": expectedRedirect,
            },
            @"trigger": @{
                @"url-filter": @".*",
                @"resource-type": @[ @"script" ],
            },
        };

        EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
    };

    testRedirect(@{ }, nil);

    testRedirect(@{ @"url": @"" }, nil);
    testRedirect(@{ @"url": @[ ] }, nil);
    testRedirect(@{ @"url": @"ftp://example.com" }, nil);
    testRedirect(@{ @"url": @"https://example.com" }, @{ @"url": @"https://example.com" });

    testRedirect(@{ @"extensionPath": @"" }, nil);
    testRedirect(@{ @"extensionPath": @[ ] }, nil);
    testRedirect(@{ @"extensionPath": @"foo.js" }, nil);
    testRedirect(@{ @"extensionPath": @"/foo.js" }, @{ @"extension-path": @"/foo.js" });

    testRedirect(@{ @"regexSubstitution": @"" }, nil);
    testRedirect(@{ @"regexSubstitution": @[ ] }, nil);
    testRedirect(@{ @"regexSubstitution": @"http://example.com/\\1" }, @{ @"regex-substitution": @"http://example.com/\\1" });

    testRedirect(@{ @"transform": @"" }, nil);
    testRedirect(@{ @"transform": @[ ] }, nil);
    testRedirect(@{ @"transform": @{ } }, nil);
    testRedirect(@{ @"transform": @{ @"scheme": @"https", @"host": @"new.example.com" } }, @{ @"transform": @{ @"scheme": @"https", @"host": @"new.example.com" } });
    testRedirect(@{ @"transform": @{ @"username": @"foo", @"password": @"bar" } }, @{ @"transform": @{ @"username": @"foo", @"password": @"bar" } });
    testRedirect(@{ @"transform": @{ @"query": @"foo" } }, @{ @"transform": @{ @"query": @"foo" } });

    testRedirect(@{ @"transform": @{ @"queryTransform": @"" } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @[ ] } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @"" } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ ] } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ @{ } ] } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ @{ @"key": @"foo" } ] } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ @{ @"replaceOnly": @"" } ] } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"removeParams": @"" } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"removeParams": @[ ] } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ @{ @"key": @"foo", @"value": @"bar" } ] } } }, @{ @"transform": @{ @"query-transform": @{ @"add-or-replace-parameters": @[ @{ @"key": @"foo", @"value": @"bar" } ] } } });
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ @{ @"key": @"foo", @"value": @"bar", @"replaceOnly": @YES } ] } } }, @{ @"transform": @{ @"query-transform": @{ @"add-or-replace-parameters": @[ @{ @"key": @"foo", @"value": @"bar", @"replace-only": @YES } ] } } });
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"removeParams": @[ @"foo" ] } } }, @{ @"transform": @{ @"query-transform": @{ @"remove-parameters": @[ @"foo" ] } } });
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithModifyHeaders)
{
    __auto_type testModifyHeaders = ^(NSString *inputHeaderType, NSArray<NSDictionary *> *inputModifyHeadersInfo, NSString *expectedHeaderType, NSArray<NSDictionary *> *expectedModifyHeadersInfo) {
        NSDictionary *rule = @{
            @"id": @10,
            @"priority": @2,
            @"action": @{
                @"type": @"modifyHeaders",
                inputHeaderType: inputModifyHeadersInfo,
            },
            @"condition": @{
                @"urlFilter": @"*",
                @"resourceTypes": @[ @"script" ],
            },
        };

        _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
        NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;

        if (!expectedModifyHeadersInfo) {
            EXPECT_NULL(convertedRule);
            return;
        }

        EXPECT_NOT_NULL(convertedRule);

        NSDictionary *correctRuleConversion = @{
            @"action": @{
                @"type": @"modify-headers",
                @"priority": @2,
                expectedHeaderType: expectedModifyHeadersInfo,
            },
            @"trigger": @{
                @"url-filter": @".*",
                @"resource-type": @[ @"script" ],
            },
        };

        EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
    };

    testModifyHeaders(@"responseHeaders", @[ ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ } ], nil, nil);
    testModifyHeaders(@"requestHeaders", @[ @{ } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"wrong dictionary structure": @[ @{ } ] } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"header": @"accept", @"operation": @"invalid header operation", @"value": @"v1" } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"operation": @"set", @"value": @"v1" } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"header": @"accept", @"operation": @"set" } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"header": @"accept", @"operation": @"append" } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"header": @"accept", @"operation": @"remove", @"value": @"v1" } ], nil, nil);
    testModifyHeaders(@"requestHeaders", @[ @{ @"operation": @"set", @"value": @"v1" } ], nil, nil);
    testModifyHeaders(@"requestHeaders", @[ @{ @"header": @"ellie's custom header", @"operation": @"set", @"value": @"v1" } ], nil, nil);

    testModifyHeaders(@"requestHeaders", @[ @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ], @"request-headers", @[ @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ]);
    testModifyHeaders(@"requestHeaders", @[ @{ @"header": @"accept", @"operation": @"remove" } ],  @"request-headers", @[ @{ @"header": @"accept", @"operation": @"remove" } ]);
    testModifyHeaders(@"requestHeaders", @[ @{ @"header": @"accept", @"operation": @"append", @"value": @"v1" } ], @"request-headers", @[ @{ @"header": @"accept", @"operation": @"append", @"value": @"v1" } ]);
    testModifyHeaders(@"requestHeaders", @[ @{ @"header": @"accept", @"operation": @"append", @"value": @"v1" }, @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ], @"request-headers", @[ @{ @"header": @"accept", @"operation": @"append", @"value": @"v1" }, @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ]);
    testModifyHeaders(@"responseHeaders", @[ @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ], @"response-headers", @[ @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ]);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithModifyHeadersWithBothResponseAndRequestHeaders)
{
    NSDictionary *rule = @{
        @"id": @10,
        @"action": @{
            @"type": @"modifyHeaders",
            @"responseHeaders": @[
                @{ @"header": @"age", @"operation": @"set", @"value": @"5" },
                @{ @"header": @"date", @"operation": @"remove" },
            ],
            @"requestHeaders": @[
                @{ @"header": @"cache-control", @"operation": @"set", @"value": @"something" },
                @{ @"header": @"Cookie", @"operation": @"remove" },
            ],
        },
        @"condition": @{
            @"urlFilter": @"*",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"modify-headers",
            @"priority": @1,
            @"response-headers": @[
                @{ @"header": @"age", @"operation": @"set", @"value": @"5" },
                @{ @"header": @"date", @"operation": @"remove" },
            ],
            @"request-headers": @[
                @{ @"header": @"cache-control", @"operation": @"set", @"value": @"something" },
                @{ @"header": @"Cookie", @"operation": @"remove" },
            ],
        },
        @"trigger": @{
            @"url-filter": @".*",
            @"resource-type": @[ @"script" ],
        },
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithModifyHeadersWithoutAnyHeadersSpecified)
{
    NSDictionary *rule = @{
        @"id": @10,
        @"priority": @2,
        @"action": @{
            @"type": @"modifyHeaders",
        },
        @"condition": @{
            @"urlFilter": @"*",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NULL(convertedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RulesSortByPriorityFromDifferentRulesets)
{
    NSArray *rules = @[
        @[ @{
            @"id": @1,
            @"priority": @2,
            @"action": @{ @"type": @"allow" },
            @"condition": @{
                @"regexFilter": @"apple.com",
                @"resourceTypes": @[ @"script" ],
            },
        } ],
        @[ @{
            @"id": @1,
            @"priority": @1,
            @"action": @{ @"type": @"block" },
            @"condition": @{
                @"regexFilter": @"bananas.com",
                @"resourceTypes": @[ @"script" ],
            },
        } ],
    ];

    NSArray *sortedTranslatedRules = [_WKWebExtensionDeclarativeNetRequestTranslator translateRules:rules errorStrings:nil];
    EXPECT_NOT_NULL(sortedTranslatedRules);

    EXPECT_NS_EQUAL(sortedTranslatedRules[0][@"action"][@"type"], @"block");
    EXPECT_NS_EQUAL(sortedTranslatedRules[1][@"action"][@"type"], @"ignore-previous-rules");
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RulesSortWithoutExplicitPriority)
{
    NSArray *rules = @[
        @[
            @{
                @"id": @1,
                @"action": @{ @"type": @"upgradeScheme" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
            @{
                @"id": @1,
                @"priority": @3,
                @"action": @{ @"type": @"block" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
        ]
    ];

    NSArray *sortedTranslatedRules = [_WKWebExtensionDeclarativeNetRequestTranslator translateRules:rules errorStrings:nil];
    EXPECT_NOT_NULL(sortedTranslatedRules);

    EXPECT_NS_EQUAL(sortedTranslatedRules[0][@"action"][@"type"], @"ignore-previous-rules");
    EXPECT_NS_EQUAL(sortedTranslatedRules[1][@"action"][@"type"], @"make-https");
    EXPECT_NS_EQUAL(sortedTranslatedRules[2][@"action"][@"type"], @"block");
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RulesSortByActionType)
{
    NSArray *rules = @[
        @[
            @{
                @"id": @1,
                @"action": @{ @"type": @"upgradeScheme" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
            @{
                @"id": @1,
                @"action": @{ @"type": @"allowAllRequests" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"main_frame" ],
                },
            },
            @{
                @"id": @1,
                @"action": @{ @"type": @"allow" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
            @{
                @"id": @1,
                @"action": @{ @"type": @"block" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
            @{
                @"id": @1,
                @"action": @{
                    @"type": @"modifyHeaders",
                    @"requestHeaders": @[
                        @{ @"header": @"DNT", @"operation": @"set", @"value": @"1" },
                    ],
                },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
        ]
    ];

    NSArray *sortedTranslatedRules = [_WKWebExtensionDeclarativeNetRequestTranslator translateRules:rules errorStrings:nil];
    EXPECT_NOT_NULL(sortedTranslatedRules);

    EXPECT_NS_EQUAL(sortedTranslatedRules[0][@"action"][@"type"], @"modify-headers");
    EXPECT_NS_EQUAL(sortedTranslatedRules[1][@"action"][@"type"], @"ignore-previous-rules");
    EXPECT_NS_EQUAL(sortedTranslatedRules[2][@"action"][@"type"], @"make-https");
    EXPECT_NS_EQUAL(sortedTranslatedRules[3][@"action"][@"type"], @"block");
    EXPECT_NS_EQUAL(sortedTranslatedRules[4][@"action"][@"type"], @"ignore-previous-rules");
    EXPECT_NS_EQUAL(sortedTranslatedRules[5][@"action"][@"type"], @"ignore-previous-rules");
}


} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
