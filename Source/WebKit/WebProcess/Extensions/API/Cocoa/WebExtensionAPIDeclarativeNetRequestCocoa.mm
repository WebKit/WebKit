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
#import "WebExtensionAPIDeclarativeNetRequest.h"

#import "CocoaHelpers.h"
#import "JSWebExtensionWrapper.h"
#import "MessageSenderInlines.h"
#import "WKContentRuleListPrivate.h"
#import "WebExtensionContext.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionMatchedRuleParameters.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import "_WKWebExtensionDeclarativeNetRequestRule.h"
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
#import "WebExtensionAPINamespace.h"
#import <WebCore/ContentRuleListMatchedRule.h>
#endif

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

static NSString * const disableRulesetsKey = @"disableRulesetIds";
static NSString * const enableRulesetsKey = @"enableRulesetIds";

static NSString * const regexKey = @"regex";
static NSString * const regexIsCaseSensitiveKey = @"isCaseSensitive";
static NSString * const regexRequireCapturingKey = @"requireCapturing";

static NSString * const actionCountDisplayActionCountAsBadgeTextKey = @"displayActionCountAsBadgeText";
static NSString * const actionCountTabUpdateKey = @"tabUpdate";
static NSString * const actionCountTabIDKey = @"tabId";
static NSString * const actionCountIncrementKey = @"increment";

static NSString * const getMatchedRulesTabIDKey = @"tabId";
static NSString * const getMatchedRulesMinTimeStampKey = @"minTimeStamp";

static NSString * const getDynamicOrSessionRulesRuleIDsKey = @"ruleIds";

static NSString * const addRulesKey = @"addRules";
static NSString * const removeRulesKey = @"removeRuleIds";

#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
static NSString * const requestKey = @"request";
static NSString * const documentIdKey = @"documentId";
static NSString * const documentLifecycleKey = @"documentLifecycle";
static NSString * const frameIdKey = @"frameId";
static NSString * const frameTypeKey = @"frameType";
static NSString * const initiatorKey = @"initiator";
static NSString * const methodKey = @"method";
static NSString * const parentDocumentIdKey = @"parentDocumentId";
static NSString * const parentFrameIdKey = @"parentFrameId";
static NSString * const requestIdKey = @"requestId";
static NSString * const tabIdKey = @"tabId";
static NSString * const typeKey = @"type";
static NSString * const urlKey = @"url";

static NSString * const ruleKey = @"rule";
static NSString * const extensionIdKey = @"extensionId";
static NSString * const ruleIdKey = @"ruleId";
static NSString * const rulesetIdKey = @"rulesetId";

static inline NSDictionary *toWebAPI(const WebCore::ContentRuleListMatchedRule& matchedRuleInfo)
{
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    NSMutableDictionary *request = [NSMutableDictionary dictionary];
    NSMutableDictionary *rule = [NSMutableDictionary dictionary];

    request[frameIdKey] = @(matchedRuleInfo.request.frameId);
    request[parentFrameIdKey] = @(matchedRuleInfo.request.parentFrameId);
    request[methodKey] = matchedRuleInfo.request.method.createNSString().get();
    request[requestIdKey] = matchedRuleInfo.request.requestId.createNSString().get();
    request[typeKey] = matchedRuleInfo.request.type.createNSString().get();
    request[tabIdKey] = @(matchedRuleInfo.request.tabId);
    request[urlKey] = matchedRuleInfo.request.url.createNSString().get();
    request[initiatorKey] = matchedRuleInfo.request.initiator.has_value() ? matchedRuleInfo.request.initiator.value().createNSString().get() : nil;
    request[documentIdKey] = matchedRuleInfo.request.documentId.has_value() ? matchedRuleInfo.request.documentId.value().createNSString().get() : nil;
    request[documentLifecycleKey] = matchedRuleInfo.request.documentLifecycle.has_value() ? matchedRuleInfo.request.documentLifecycle.value().createNSString().get() : nil;
    request[frameTypeKey] = matchedRuleInfo.request.frameType.has_value() ? matchedRuleInfo.request.frameType.value().createNSString().get() : nil;
    request[parentDocumentIdKey] = matchedRuleInfo.request.parentDocumentId.has_value() ? matchedRuleInfo.request.parentDocumentId.value().createNSString().get() : nil;
    result[requestKey] = [request copy];

    rule[ruleIdKey] = @(matchedRuleInfo.rule.ruleId);
    rule[rulesetIdKey] = matchedRuleInfo.rule.rulesetId.createNSString().get();
    rule[extensionIdKey] = matchedRuleInfo.rule.extensionId.has_value() ? matchedRuleInfo.rule.extensionId.value().createNSString().get() : nil;
    result[ruleKey] = [rule copy];

    return [result copy];
}

bool WebExtensionAPIDeclarativeNetRequest::isPropertyAllowed(const ASCIILiteral& name, WebPage*)
{
    if (extensionContext().isUnsupportedAPI(propertyPath(), name)) [[unlikely]]
        return false;

    if (name == "onRuleMatchedDebug"_s)
        return extensionContext().hasPermission("declarativeNetRequestFeedback"_s);

    ASSERT_NOT_REACHED();
    return false;
}
#endif

void WebExtensionAPIDeclarativeNetRequest::updateEnabledRulesets(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        disableRulesetsKey: @[ NSString.class ],
        enableRulesetsKey: @[ NSString.class ],
    };

    if (!validateDictionary(options, @"options", nil, types, outExceptionString))
        return;

    Vector<String> rulesetsToEnable = makeVector<String>(objectForKey<NSArray>(options, enableRulesetsKey, true, NSString.class));
    Vector<String> rulesetsToDisable = makeVector<String>(objectForKey<NSArray>(options, disableRulesetsKey, true, NSString.class));

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DeclarativeNetRequestUpdateEnabledRulesets(rulesetsToEnable, rulesetsToDisable), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIDeclarativeNetRequest::getEnabledRulesets(Ref<WebExtensionCallbackHandler>&& callback)
{
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DeclarativeNetRequestGetEnabledRulesets(), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Vector<String> enabledRulesets) {
        callback->call(createNSArray(enabledRulesets).get());
    }, extensionContext().identifier());
}

void WebExtensionAPIDeclarativeNetRequest::updateDynamicRules(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *keyTypes = @{
        addRulesKey: @[ NSDictionary.class ],
        removeRulesKey: @[ NSNumber.class ],
    };

    if (!validateDictionary(options, @"options", nil, keyTypes, outExceptionString))
        return;

    auto *rulesToAdd = objectForKey<NSArray>(options, addRulesKey, false, NSDictionary.class);

    NSString *ruleErrorString;
    size_t index = 0;
    for (NSDictionary *ruleDictionary in rulesToAdd) {
        if (![[_WKWebExtensionDeclarativeNetRequestRule  alloc] initWithDictionary:ruleDictionary rulesetID:dynamicRulesetID errorString:&ruleErrorString]) {
            ASSERT(ruleErrorString);
            *outExceptionString = toErrorString(nullString(), addRulesKey, @"an error with rule at index %lu: %@", index, ruleErrorString).createNSString().autorelease();
            return;
        }

        ++index;
    }

    String rulesToAddJSON;
    if (rulesToAdd)
        rulesToAddJSON = encodeJSONString(rulesToAdd, JSONOptions::FragmentsAllowed);

    Vector<double> ruleIDsToRemove;
    if (NSArray *rulesToRemove = objectForKey<NSArray>(options, removeRulesKey, false, NSNumber.class)) {
        for (NSNumber *ruleIDToRemove in rulesToRemove)
            ruleIDsToRemove.append(ruleIDToRemove.doubleValue);
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DeclarativeNetRequestUpdateDynamicRules(WTFMove(rulesToAddJSON), WTFMove(ruleIDsToRemove)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIDeclarativeNetRequest::getDynamicRules(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&& callback)
{
    NSString *outExceptionString;

    static NSDictionary<NSString *, id> *keyTypes = @{
        getDynamicOrSessionRulesRuleIDsKey: @[ NSNumber.class ]
    };

    if (!validateDictionary(filter, nil, nil, keyTypes, &outExceptionString)) {
        callback->reportError(outExceptionString);
        return;
    }

    Vector<double> ruleIDs;
    for (NSNumber *ruleID in filter[getDynamicOrSessionRulesRuleIDsKey])
        ruleIDs.append(ruleID.doubleValue);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DeclarativeNetRequestGetDynamicRules(WTFMove(ruleIDs)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(parseJSON(result.value().createNSString().get(), JSONOptions::FragmentsAllowed));
    }, extensionContext().identifier());
}

void WebExtensionAPIDeclarativeNetRequest::updateSessionRules(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *keyTypes = @{
        addRulesKey: @[ NSDictionary.class ],
        removeRulesKey: @[ NSNumber.class ],
    };

    if (!validateDictionary(options, @"options", nil, keyTypes, outExceptionString))
        return;

    auto *rulesToAdd = objectForKey<NSArray>(options, addRulesKey, false, NSDictionary.class);

    NSString *ruleErrorString;
    size_t index = 0;
    for (NSDictionary *ruleDictionary in rulesToAdd) {
        if (![[_WKWebExtensionDeclarativeNetRequestRule  alloc] initWithDictionary:ruleDictionary rulesetID:sessionRulesetID errorString:&ruleErrorString]) {
            ASSERT(ruleErrorString);
            *outExceptionString = toErrorString(nullString(), addRulesKey, @"an error with rule at index %lu: %@", index, ruleErrorString).createNSString().autorelease();
            return;
        }

        ++index;
    }

    String rulesToAddJSON;
    if (rulesToAdd)
        rulesToAddJSON = encodeJSONString(rulesToAdd, JSONOptions::FragmentsAllowed);

    Vector<double> ruleIDsToRemove;
    if (NSArray *rulesToRemove = objectForKey<NSArray>(options, removeRulesKey, false, NSNumber.class)) {
        for (NSNumber *ruleIDToRemove in rulesToRemove)
            ruleIDsToRemove.append(ruleIDToRemove.doubleValue);
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DeclarativeNetRequestUpdateSessionRules(WTFMove(rulesToAddJSON), WTFMove(ruleIDsToRemove)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIDeclarativeNetRequest::getSessionRules(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&& callback)
{
    NSString *outExceptionString;

    static NSDictionary<NSString *, id> *keyTypes = @{
        getDynamicOrSessionRulesRuleIDsKey: @[ NSNumber.class ]
    };

    if (!validateDictionary(filter, nil, nil, keyTypes, &outExceptionString)) {
        callback->reportError(outExceptionString);
        return;
    }

    Vector<double> ruleIDs;
    for (NSNumber *ruleID in filter[getDynamicOrSessionRulesRuleIDsKey])
        ruleIDs.append(ruleID.doubleValue);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DeclarativeNetRequestGetSessionRules(WTFMove(ruleIDs)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(parseJSON(result.value().createNSString().get(), JSONOptions::FragmentsAllowed));
    }, extensionContext().identifier());
}

static NSDictionary *toWebAPI(const Vector<WebExtensionMatchedRuleParameters>& matchedRules)
{
    NSMutableArray *matchedRuleArray = [NSMutableArray arrayWithCapacity:matchedRules.size()];

    for (auto& matchedRule : matchedRules) {
        [matchedRuleArray addObject:@{
            @"request": @{ @"url": matchedRule.url.string().createNSString().get() },
            @"timeStamp": @(floor(matchedRule.timeStamp.secondsSinceEpoch().milliseconds())),
            @"tabId": @(toWebAPI(matchedRule.tabIdentifier))
        }];
    }

    return @{ @"rulesMatchedInfo": matchedRuleArray };
}

void WebExtensionAPIDeclarativeNetRequest::getMatchedRules(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    bool hasFeedbackPermission = extensionContext().hasPermission("declarativeNetRequestFeedback"_s);
    bool hasActiveTabPermission = extensionContext().hasPermission("activeTab"_s);

    if (!hasFeedbackPermission && !hasActiveTabPermission) {
        *outExceptionString = toErrorString(nullString(), nullString(), @"either the 'declarativeNetRequestFeedback' or 'activeTab' permission is required").createNSString().autorelease();
        return;
    }

    static NSArray<NSString *> *requiredKeysForActiveTab = @[
        getMatchedRulesTabIDKey,
    ];

    static NSDictionary<NSString *, id> *keyTypes = @{
        getMatchedRulesTabIDKey: NSNumber.class,
        getMatchedRulesMinTimeStampKey: NSNumber.class,
    };

    NSArray<NSString *> *requiredKeys = !hasFeedbackPermission ? requiredKeysForActiveTab : @[ ];
    if (!validateDictionary(filter, nil, requiredKeys, keyTypes, outExceptionString))
        return;

    NSNumber *tabID = objectForKey<NSNumber>(filter, getMatchedRulesTabIDKey);
    auto optionalTabIdentifier = tabID ? toWebExtensionTabIdentifier(tabID.doubleValue) : std::nullopt;
    if (tabID && !isValid(optionalTabIdentifier)) {
        *outExceptionString = toErrorString(nullString(), getMatchedRulesTabIDKey, @"%@ is not a valid tab identifier", tabID).createNSString().autorelease();
        return;
    }

    NSNumber *minTimeStamp = objectForKey<NSNumber>(filter, getMatchedRulesMinTimeStampKey);
    std::optional<WallTime> optionalTimeStamp;
    if (minTimeStamp)
        optionalTimeStamp = WallTime::fromRawSeconds(Seconds::fromMilliseconds(minTimeStamp.doubleValue).value());

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DeclarativeNetRequestGetMatchedRules(optionalTabIdentifier, optionalTimeStamp), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Vector<WebExtensionMatchedRuleParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPIDeclarativeNetRequest::isRegexSupported(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    static NSDictionary<NSString *, Class> *types = @{
        regexKey: NSString.class,
        regexIsCaseSensitiveKey: @YES.class,
        regexRequireCapturingKey: @YES.class,
    };

    if (!validateDictionary(options, @"regexOptions", @[ regexKey ], types, outExceptionString))
        return;

    NSString *regexString = objectForKey<NSString>(options, regexKey);
    if (![WKContentRuleList _supportsRegularExpression:regexString])
        callback->call(@{ @"isSupported": @NO, @"reason": @"syntaxError" });
    else
        callback->call(@{ @"isSupported": @YES });
}

void WebExtensionAPIDeclarativeNetRequest::setExtensionActionOptions(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    static NSDictionary<NSString *, Class> *types = @{
        actionCountDisplayActionCountAsBadgeTextKey: @YES.class,
        actionCountTabUpdateKey: NSDictionary.class
    };

    if (!validateDictionary(options, @"extensionActionOptions", nil, types, outExceptionString))
        return;

    if (NSDictionary *tabUpdateDictionary = objectForKey<NSDictionary>(options, actionCountTabUpdateKey)) {
        static NSDictionary<NSString *, Class> *tabUpdateTypes = @{
            actionCountTabIDKey: NSNumber.class,
            actionCountIncrementKey: NSNumber.class
        };

        if (!validateDictionary(tabUpdateDictionary, @"tabUpdate", @[ actionCountTabIDKey, actionCountIncrementKey ], tabUpdateTypes, outExceptionString))
            return;

        NSNumber *tabID = objectForKey<NSNumber>(tabUpdateDictionary, actionCountTabIDKey);
        auto tabIdentifier = toWebExtensionTabIdentifier(tabID.doubleValue);
        if (!isValid(tabIdentifier)) {
            *outExceptionString = toErrorString(nullString(), actionCountTabIDKey, @"%@ is not a valid tab identifier", tabID).createNSString().autorelease();
            return;
        }

        WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DeclarativeNetRequestIncrementActionCount(tabIdentifier.value(), objectForKey<NSNumber>(tabUpdateDictionary, actionCountIncrementKey).doubleValue), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().createNSString().get());
                return;
            }

            callback->call();
        }, extensionContext().identifier());
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DeclarativeNetRequestDisplayActionCountAsBadgeText(objectForKey<NSNumber>(options, actionCountDisplayActionCountAsBadgeTextKey).boolValue), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
WebExtensionAPIEvent& WebExtensionAPIDeclarativeNetRequest::onRuleMatchedDebug()
{
    if (!m_onRuleMatchedDebug)
        m_onRuleMatchedDebug = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::DeclarativeNetRequestOnRuleMatchedDebug);

    return *m_onRuleMatchedDebug;
}

void WebExtensionContextProxy::dispatchOnRuleMatchedDebugEvent(const WebCore::ContentRuleListMatchedRule& matchedRule)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/declarativeNetRequest/onRuleMatchedDebug

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.declarativeNetRequest().onRuleMatchedDebug().invokeListenersWithArgument(toWebAPI(matchedRule));
    });
}
#endif

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
