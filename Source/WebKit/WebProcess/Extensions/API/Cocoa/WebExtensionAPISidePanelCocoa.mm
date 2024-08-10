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

#import "config.h"
#import "WebExtensionAPISidePanel.h"

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

#import "CocoaHelpers.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPISidebarAction.h"
#import "WebExtensionContextMessages.h"
#import "WebProcess.h"

namespace WebKit {

static NSString * const tabIdKey = @"tabId";
static NSString * const windowIdKey = @"windowId";

static ParseResult parseTabIdentifier(NSDictionary *options)
{
    id maybeTabId = [options objectForKey:tabIdKey];

    if (!maybeTabId || [maybeTabId isKindOfClass:NSNull.class])
        return std::monostate();

    if ([maybeTabId isKindOfClass:NSNumber.class]) {
        auto tabId = toWebExtensionTabIdentifier(((NSNumber *) maybeTabId).doubleValue);
        return isValid(tabId) ? ParseResult(tabId.value()) : ParseResult(toErrorString(nil, @"options", @"'tabId' is invalid"));
    }

    return toErrorString(nil, @"options", @"'tabId' must be a number");
}

static ParseResult parseWindowIdentifier(NSDictionary *options)
{
    id maybeWindowId = [options objectForKey:tabIdKey];

    if (!maybeWindowId || [maybeWindowId isKindOfClass:NSNull.class])
        return std::monostate();

    if ([maybeWindowId isKindOfClass:NSNumber.class]) {
        auto windowId = toWebExtensionWindowIdentifier(((NSNumber *) maybeWindowId).doubleValue);
        return isValid(windowId) ? ParseResult(windowId.value()) : ParseResult(toErrorString(nil, @"options", @"'windowId' is invalid"));
    }

    return toErrorString(nil, @"options", @"'windowId' must be a number");
}

static NSDictionary<NSString *, id> *serializeSidebarParameters(WebExtensionSidebarParameters const& parameters)
{
    NSDictionary *serializedParameters = @{
        @"enabled": @(parameters.enabled),
        @"path": parameters.panelPath,
    };

    if (parameters.tabIdentifier) {
        NSNumber *tabIdNum = [NSNumber numberWithUnsignedLongLong:parameters.tabIdentifier->toUInt64()];
        [serializedParameters setValue:tabIdNum forKey:@"tabId"];
    }

    return serializedParameters;
}

static Expected<WebExtensionSidebarParameters, WebExtensionError> deserializeSidebarParameters(NSDictionary<NSString *, id> *serializedParameters)
{
    WebExtensionSidebarParameters parameters;

    if (auto enabled = objectForKey<NSNumber>(serializedParameters, @"enabled"))
        parameters.enabled = [enabled boolValue];

    if (auto path = objectForKey<NSString>(serializedParameters, @"path"))
        parameters.panelPath = path;

    auto tabIdentifierResult = parseTabIdentifier(serializedParameters);
    if (NSString *error = indicatesError(tabIdentifierResult).get())
        return toWebExtensionError(nil, @"details", error);
    parameters.tabIdentifier = toOptional<WebExtensionTabIdentifier>(tabIdentifierResult);

    return parameters;
}

void WebExtensionAPISidePanel::getOptions(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    auto result = parseTabIdentifier(options);
    if ((*outExceptionString = indicatesError(result).get()))
        return;

    const auto tabId = toOptional<WebExtensionTabIdentifier>(result);

    WebProcess::singleton()
        .sendWithAsyncReply(Messages::WebExtensionContext::SidebarGetOptions(std::nullopt, tabId), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<WebExtensionSidebarParameters, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error());
                return;
            }

            callback->call(serializeSidebarParameters(result.value()));
        }, extensionContext().identifier());
}

void WebExtensionAPISidePanel::setOptions(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    auto result = deserializeSidebarParameters(options);
    if (!result) {
        *outExceptionString = result.error();
        return;
    }

    std::optional<String> panelPath = result->panelPath != ""_s ? std::optional(result->panelPath) : std::nullopt;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarSetOptions(std::nullopt, result->tabIdentifier, panelPath, result->enabled), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPISidePanel::getPanelBehavior(Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // FIXME: <https://webkit.org/b/276833> Implement panel behavior methods
    callback->reportError(@"unimplemented");
}

void WebExtensionAPISidePanel::setPanelBehavior(NSDictionary *behavior, Ref<WebExtensionCallbackHandler>&& callback, NSString** outExceptionString)
{
    // FIXME: <https://webkit.org/b/276833> Implement panel behavior methods
    callback->reportError(@"unimplemented");
}

void WebExtensionAPISidePanel::open(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    auto tabResult = parseTabIdentifier(options);
    if ((*outExceptionString = indicatesError(tabResult).get()))
        return;

    auto tabId = toOptional<WebExtensionTabIdentifier>(tabResult);

    auto windowResult = parseWindowIdentifier(options);
    if ((*outExceptionString = indicatesError(windowResult).get()))
        return;

    auto windowId = toOptional<WebExtensionWindowIdentifier>(windowResult);

    if (!windowId && !tabId) {
        *outExceptionString = toErrorString(nil, @"details", @"it must specify at least one of 'tabId' or 'windowId'");
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarOpen(windowId, tabId), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

