/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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
#import "WebExtensionAPIKeys.h"

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

#import "CocoaHelpers.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPISidebarAction.h"
#import "WebExtensionActionClickBehavior.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionPermission.h"
#import "WebProcess.h"

namespace WebKit {


static Expected<std::optional<WebExtensionTabIdentifier>, WebExtensionError> parseTabIdentifier(NSDictionary *options)
{
    id maybeTabId = [options objectForKey:tabIdKey];

    if (!maybeTabId || [maybeTabId isKindOfClass:NSNull.class])
        return { std::nullopt };

    if ([maybeTabId isKindOfClass:NSNumber.class]) {
        auto tabId = toWebExtensionTabIdentifier(((NSNumber *) maybeTabId).doubleValue);
        if (!isValid(tabId))
            return makeUnexpected(toErrorString(nullString(), @"options", @"'tabId' is invalid"));
        return std::optional(tabId.value());
    }

    return makeUnexpected(toErrorString(nullString(), @"options", @"'tabId' must be a number"));
}

static Expected<std::optional<WebExtensionWindowIdentifier>, WebExtensionError> parseWindowIdentifier(NSDictionary *options)
{
    id maybeWindowId = [options objectForKey:windowIdKey];

    if (!maybeWindowId || [maybeWindowId isKindOfClass:NSNull.class])
        return { std::nullopt };

    if ([maybeWindowId isKindOfClass:NSNumber.class]) {
        auto windowId = toWebExtensionWindowIdentifier(((NSNumber *) maybeWindowId).doubleValue);
        if (!isValid(windowId))
            return makeUnexpected(toErrorString(nullString(), @"options", @"'windowId' is invalid"));
        return std::optional(windowId.value());
    }

    return makeUnexpected(toErrorString(nullString(), @"options", @"'windowId' must be a number"));
}

static Expected<std::optional<WebExtensionActionClickBehavior>, WebExtensionError> parseActionClickBehavior(NSDictionary *behavior)
{
    static NSDictionary<NSString *, id> *types = @{
        actionClickBehaviorKey: @YES.class,
    };

    NSString *exceptionString;
    if (!validateDictionary(behavior, @"behavior", nil, types, &exceptionString))
        return makeUnexpected(exceptionString);

    NSNumber *actionClickBehavior = behavior[actionClickBehaviorKey];
    if (!actionClickBehavior)
        return { std::nullopt };
    return { actionClickBehavior.boolValue ? WebExtensionActionClickBehavior::OpenSidebar : WebExtensionActionClickBehavior::OpenPopup };
}

static NSDictionary<NSString *, id> *serializeSidebarParameters(WebExtensionSidebarParameters const& parameters)
{
    NSMutableDictionary *serializedParameters = [NSMutableDictionary new];

    serializedParameters[@"enabled"] = @(parameters.enabled);
    serializedParameters[@"path"] = parameters.panelPath.createNSString().get();

    if (parameters.tabIdentifier)
        serializedParameters[@"tabId"] = @(toWebAPI(parameters.tabIdentifier.value()));

    return serializedParameters;
}

void WebExtensionAPISidePanel::getOptions(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    auto result = parseTabIdentifier(options);
    if ((*outExceptionString = indicatesError(result).get()))
        return;

    const auto tabId = WTF::move(result.value());

    WebProcess::singleton()
        .sendWithAsyncReply(Messages::WebExtensionContext::SidebarGetOptions(std::nullopt, tabId), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<WebExtensionSidebarParameters, WebExtensionError>&& result) {
            if (!result) {
                callback->reportError(result.error().createNSString().get());
                return;
            }

            callback->call(toJSValueRef(callback->globalContext(), serializeSidebarParameters(result.value())));
        }, extensionContext().identifier());
}

void WebExtensionAPISidePanel::setOptions(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    auto tabIdentifierResult = parseTabIdentifier(options);
    if ((*outExceptionString = indicatesError(tabIdentifierResult).get()))
        return;

    RetainPtr path = objectForKey<NSString>(options, @"path", false);
    RetainPtr enabled = objectForKey<NSNumber>(options, @"enabled");

    if (!path && !enabled) {
        *outExceptionString = toErrorString(nullString(), @"options", @"it must specify at least one of 'path' or 'enabled'").createNSString().get();
        return;
    }

    // An empty path (`path: ""`) clears the panel, so it is sent as nullopt.
    std::optional<String> panelPath;
    if (path.get().length)
        panelPath = String { path.get() };

    std::optional<bool> enabledValue;
    if (enabled)
        enabledValue = enabled.get().boolValue;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarSetOptions(std::nullopt, tabIdentifierResult.value(), panelPath, enabledValue), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPISidePanel::getPanelBehavior(Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarGetActionClickBehavior(), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<WebExtensionActionClickBehavior, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        bool openPanelOnActionClick = result.value() == WebExtensionActionClickBehavior::OpenSidebar;
        callback->call(toJSValueRef(callback->globalContext(), @{
            actionClickBehaviorKey: @(openPanelOnActionClick),
        }));
    }, extensionContext().identifier());
}

void WebExtensionAPISidePanel::setPanelBehavior(NSDictionary *behavior, Ref<WebExtensionCallbackHandler>&& callback, NSString** outExceptionString)
{
    auto result = parseActionClickBehavior(behavior);
    if ((*outExceptionString = indicatesError(result).get()))
        return;

    std::optional maybeClickBehavior = WTF::move(result.value());
    if (!maybeClickBehavior) {
        // setPanelBehavior is an upsert; if `openPanelOnActionClick` is omitted then the current behavior is unchanged
        callback->call({ });
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarSetActionClickBehavior(*maybeClickBehavior), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call({ });
    }, extensionContext().identifier());
}

void WebExtensionAPISidePanel::open(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    if (!WebCore::UserGestureIndicator::processingUserGesture()) {
        // In Chrome, this error manifests as a rejected promise, so match this behavior.
        callback->reportError(toErrorString(@"sidePanel.open()", nullString(), @"it must be called during a user gesture").createNSString().get());
        return;
    }

    auto tabResult = parseTabIdentifier(options);
    if ((*outExceptionString = indicatesError(tabResult).get()))
        return;

    auto tabId = WTF::move(tabResult.value());

    auto windowResult = parseWindowIdentifier(options);
    if ((*outExceptionString = indicatesError(windowResult).get()))
        return;

    auto windowId = WTF::move(windowResult.value());

    if (!windowId && !tabId) {
        *outExceptionString = toErrorString(nullString(), @"details", @"it must specify at least one of 'tabId' or 'windowId'").createNSString().get();
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::SidebarOpen(windowId, tabId), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

