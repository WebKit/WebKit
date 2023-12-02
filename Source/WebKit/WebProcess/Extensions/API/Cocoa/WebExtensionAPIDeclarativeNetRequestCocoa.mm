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
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

static NSString * const disableRulesetsKey = @"disableRulesetIds";
static NSString * const enableRulesetsKey = @"enableRulesetIds";

static NSString * const regexKey = @"regex";
static NSString * const regexIsCaseSensitiveKey = @"isCaseSensitive";
static NSString * const regexRequireCapturingKey = @"requireCapturing";

void WebExtensionAPIDeclarativeNetRequest::updateEnabledRulesets(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        disableRulesetsKey: @[ NSString.class ],
        enableRulesetsKey: @[ NSString.class ],
    };

    if (!validateDictionary(options, @"options", nil, types, outExceptionString))
        return;

    Vector<String> rulesetsToEnable =  makeVector<String>(objectForKey<NSArray>(options, enableRulesetsKey, true, NSString.class));
    Vector<String> rulesetsToDisable = makeVector<String>(objectForKey<NSArray>(options, disableRulesetsKey, true, NSString.class));

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DeclarativeNetRequestUpdateEnabledRulesets(rulesetsToEnable, rulesetsToDisable), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
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

void WebExtensionAPIDeclarativeNetRequest::updateDynamicRules(NSDictionary *options, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString)
{
    // FIXME: rdar://118476702 - Support dynamic rules
}

void WebExtensionAPIDeclarativeNetRequest::getDynamicRules(Ref<WebExtensionCallbackHandler>&&)
{
    // FIXME: rdar://118476702 - Support dynamic rules
}

void WebExtensionAPIDeclarativeNetRequest::updateSessionRules(NSDictionary *options, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString)
{
    // FIXME: rdar://118476774 - Support session rules
}

void WebExtensionAPIDeclarativeNetRequest::getSessionRules(Ref<WebExtensionCallbackHandler>&&)
{
    // FIXME: rdar://118476774 - Support session rules
}

void WebExtensionAPIDeclarativeNetRequest::getMatchedRules(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString)
{
    // FIXME: rdar://118940129 - Support getMatchedRules
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

void WebExtensionAPIDeclarativeNetRequest::setExtensionActionOptions(NSDictionary *options, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString)
{
    // FIXME: rdar://118476776 - Support badging the extension's action with the number of blocked resources
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
