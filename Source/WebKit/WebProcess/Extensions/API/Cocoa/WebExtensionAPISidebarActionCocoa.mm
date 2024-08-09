/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"
#import "WebExtensionAPISidebarAction.h"

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

#import "MessageSenderInlines.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionSidebarParameters.h"
#import "WebExtensionTabIdentifier.h"
#import "WebExtensionWindowIdentifier.h"
#import "WebProcess.h"

namespace WebKit {

static NSString * const tabIdKey = @"tabId";
static NSString * const windowIdKey = @"windowId";
static NSString * const panelKey = @"panel";
static NSString * const titleKey = @"title";

static ParseResult parseSidebarActionDetails(NSDictionary *details)
{
    id maybeTabId = [details objectForKey:tabIdKey];
    id maybeWindowId = [details objectForKey:windowIdKey];

    if (maybeTabId && maybeWindowId)
        return toErrorString(nil, @"details", @"it cannot specify both 'tabId' and 'windowId'");

    if (maybeTabId && ![maybeTabId isKindOfClass:NSNumber.class])
        return toErrorString(nil, @"details", @"'tabId' must be a number");

    if (maybeWindowId && ![maybeWindowId isKindOfClass:NSNumber.class])
        return toErrorString(nil, @"details", @"'windowId' must be a number");

    if (maybeTabId) {
        auto tabId = toWebExtensionTabIdentifier(((NSNumber *) maybeTabId).doubleValue);
        return isValid(tabId) ? ParseResult(tabId.value()) : ParseResult(toErrorString(nil, @"details", @"'tabId' is invalid"));
    }

    if (maybeWindowId) {
        auto windowId = toWebExtensionWindowIdentifier(((NSNumber *) maybeWindowId).doubleValue);
        return isValid(windowId) ? ParseResult(windowId.value()) : ParseResult(toErrorString(nil, @"details", @"'windowId' is invalid"));
    }

    return std::monostate();
}

static std::variant<std::monostate, String, SidebarError> parseDetailsStringFromKey(NSDictionary *dict, NSString *key, bool required = false)
{
    id maybeValue = [dict objectForKey:key];
    if (!maybeValue && required)
        return SidebarError { toErrorString(nil, @"details", [NSString stringWithFormat:@"'%@' is required", key]) };

    if ([maybeValue isKindOfClass:NSNull.class]) {
        if (required)
            return SidebarError { toErrorString(nil, @"details", [NSString stringWithFormat:@"'%@' is required", key]) };
        return std::monostate();
    }

    if (![maybeValue isKindOfClass:NSString.class]) {
        if (required)
            return SidebarError { toErrorString(nil, @"details", [NSString stringWithFormat:@"'%@' must be of type 'string'", key]) };
        return SidebarError { toErrorString(nil, @"details", [NSString stringWithFormat:@"'%@' must be of type 'string' or 'null'", key]) };
    }

    return SidebarError { (NSString *)maybeValue };
}

template<typename VariantType>
static std::tuple<std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>> getIdentifiers(VariantType& variant)
{
    static_assert(isVariantMember<WebExtensionWindowIdentifier, VariantType>::value);
    static_assert(isVariantMember<WebExtensionTabIdentifier, VariantType>::value);

    return std::make_tuple(WTFMove(toOptional<WebExtensionWindowIdentifier>(variant)), WTFMove(toOptional<WebExtensionTabIdentifier>(variant)));
}

void WebExtensionAPISidebarAction::open(Ref<WebExtensionCallbackHandler>&& callback , NSString **outExceptionString)
{
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarOpen(std::nullopt, std::nullopt), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPISidebarAction::close(Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarClose(), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPISidebarAction::toggle(Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarToggle(), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPISidebarAction::isOpen(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // we don't use parseSidebarActionDetails here because we only need windowId for isOpen
    id maybeWindowId = details[windowIdKey];
    if (maybeWindowId && ![maybeWindowId isKindOfClass:NSNumber.class]) {
        *outExceptionString = toErrorString(nil, @"details", @"'windowId' must be a number");
        return;
    }

    std::optional<WebExtensionWindowIdentifier> windowId = maybeWindowId ? toWebExtensionWindowIdentifier(((NSNumber *) maybeWindowId).doubleValue) : std::nullopt;
    if (!isValid(windowId)) {
        *outExceptionString = toErrorString(nil, @"details", @"'windowId' is invalid");
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarIsOpen(windowId), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<bool, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(@(result.value()));
    }, extensionContext().identifier());
}


void WebExtensionAPISidebarAction::getPanel(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    auto result = parseSidebarActionDetails(details);
    if ((*outExceptionString = indicatesError(result).get()))
        return;

    const auto [windowId, tabId] = getIdentifiers(result);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarGetOptions(windowId, tabId), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<WebExtensionSidebarParameters, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(result.value().panelPath);
    }, extensionContext().identifier());
}

void WebExtensionAPISidebarAction::setPanel(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    auto panelResult = parseDetailsStringFromKey(details, panelKey);
    if ((*outExceptionString = indicatesError(panelResult).get()))
        return;

    const auto panelPath = toOptional<String>(panelResult);

    auto result = parseSidebarActionDetails(details);
    if ((*outExceptionString = indicatesError(result).get()))
        return;

    const auto [windowId, tabId] = getIdentifiers(result);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarSetOptions(windowId, tabId, std::nullopt, std::nullopt), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPISidebarAction::getTitle(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    auto result = parseSidebarActionDetails(details);
    if ((*outExceptionString = indicatesError(result).get()))
        return;

    const auto [windowId, tabId] = getIdentifiers(result);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarGetTitle(windowId, tabId), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(result.value());
    }, extensionContext().identifier());
}

void WebExtensionAPISidebarAction::setTitle(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    auto titleResult = parseDetailsStringFromKey(details, titleKey);
    if ((*outExceptionString = indicatesError(titleResult).get()))
        return;

    const auto title = toOptional<String>(titleResult);

    auto result = parseSidebarActionDetails(details);
    if ((*outExceptionString = indicatesError(result).get()))
        return;

    const auto [windowId, tabId] = getIdentifiers(result);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarSetTitle(windowId, tabId, title), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPISidebarAction::setIcon(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // FIXME: <https://webkit.org/b/276833> Implement icon-related functionality
    static NSString * const apiName = @"sidebarAction.setIcon()";
    callback->reportError([NSString stringWithFormat:@"'%@' is unimplemented", apiName]);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
