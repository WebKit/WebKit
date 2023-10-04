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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "WebExtensionAction.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionUtilities.h"

namespace WebKit {

static RefPtr<WebExtensionAction> getActionWithIdentifiers(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, WebExtensionContext& context, NSString *apiName, std::optional<String>& error)
{
    RefPtr<WebExtensionAction> action;

    if (windowIdentifier) {
        auto window = context.getWindow(windowIdentifier.value());
        if (!window) {
            error = toErrorString(apiName, nil, @"window not found");
            return nullptr;
        }

        return context.getAction(window.get()).ptr();
    }

    if (tabIdentifier) {
        auto tab = context.getTab(tabIdentifier.value());
        if (!tab) {
            error = toErrorString(apiName, nil, @"tab not found");
            return nullptr;
        }

        return context.getAction(tab.get()).ptr();
    }

    return &context.defaultAction();
}

static RefPtr<WebExtensionAction> getOrCreateActionWithIdentifiers(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, WebExtensionContext& context, NSString *apiName, std::optional<String>& error)
{
    RefPtr<WebExtensionAction> action;

    if (windowIdentifier) {
        auto window = context.getWindow(windowIdentifier.value());
        if (!window) {
            error = toErrorString(apiName, nil, @"window not found");
            return nullptr;
        }

        return context.getOrCreateAction(window.get()).ptr();
    }

    if (tabIdentifier) {
        auto tab = context.getTab(tabIdentifier.value());
        if (!tab) {
            error = toErrorString(apiName, nil, @"tab not found");
            return nullptr;
        }

        return context.getOrCreateAction(tab.get()).ptr();
    }

    return &context.defaultAction();
}

void WebExtensionContext::actionGetTitle(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(std::optional<String>, std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = @"action.getTitle()";

    std::optional<String> error;
    auto action = getActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName, error);
    if (error) {
        completionHandler(std::nullopt, error);
        return;
    }

    ASSERT(action);

    completionHandler(action->displayLabel(WebExtensionAction::FallbackWhenEmpty::No), std::nullopt);
}

void WebExtensionContext::actionSetTitle(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const String& title, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = @"action.setTitle()";

    std::optional<String> error;
    auto action = getOrCreateActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName, error);
    if (error) {
        completionHandler(error);
        return;
    }

    ASSERT(action);

    action->setDisplayLabel(title);

    completionHandler(std::nullopt);
}

void WebExtensionContext::actionSetIcon(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const String& iconDictionaryJSON, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = @"action.setIcon()";

    std::optional<String> error;
    auto action = getOrCreateActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName, error);
    if (error) {
        completionHandler(error);
        return;
    }

    ASSERT(action);

    action->setIconsDictionary(parseJSON(iconDictionaryJSON));

    completionHandler(std::nullopt);
}

void WebExtensionContext::actionGetPopup(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(std::optional<String>, std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = @"action.getPopup()";

    std::optional<String> error;
    auto action = getActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName, error);
    if (error) {
        completionHandler(std::nullopt, error);
        return;
    }

    ASSERT(action);

    completionHandler(action->popupPath(), std::nullopt);
}

void WebExtensionContext::actionSetPopup(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const String& popupPath, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = @"action.setPopup()";

    std::optional<String> error;
    auto action = getOrCreateActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName, error);
    if (error) {
        completionHandler(error);
        return;
    }

    ASSERT(action);

    action->setPopupPath(popupPath);

    completionHandler(std::nullopt);
}

void WebExtensionContext::actionOpenPopup(WebPageProxyIdentifier identifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = @"action.openPopup()";

    if (!defaultAction().canProgrammaticallyPresentPopup()) {
        completionHandler(toErrorString(apiName, nil, @"it is not implemented"));
        return;
    }

    if (windowIdentifier) {
        auto window = getWindow(windowIdentifier.value());
        if (!window) {
            completionHandler(toErrorString(apiName, nil, @"window not found"));
            return;
        }

        if (auto activeTab = window->activeTab()) {
            if (getAction(activeTab.get())->hasPopup())
                performAction(activeTab.get(), UserTriggered::No);

            completionHandler(std::nullopt);
            return;
        }
    }

    if (tabIdentifier) {
        auto tab = getTab(tabIdentifier.value());
        if (!tab) {
            completionHandler(toErrorString(apiName, nil, @"tab not found"));
            return;
        }

        if (getAction(tab.get())->hasPopup())
            performAction(tab.get(), UserTriggered::No);

        completionHandler(std::nullopt);
        return;
    }

    if (defaultAction().hasPopup())
        performAction(nullptr, UserTriggered::No);

    completionHandler(std::nullopt);
}

void WebExtensionContext::actionGetBadgeText(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(std::optional<String>, std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = @"action.getBadgeText()";

    std::optional<String> error;
    auto action = getActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName, error);
    if (error) {
        completionHandler(std::nullopt, error);
        return;
    }

    ASSERT(action);

    completionHandler(action->badgeText(), std::nullopt);
}

void WebExtensionContext::actionSetBadgeText(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const String& text, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = @"action.setBadgeText()";

    std::optional<String> error;
    auto action = getOrCreateActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName, error);
    if (error) {
        completionHandler(error);
        return;
    }

    ASSERT(action);

    action->setBadgeText(text);

    completionHandler(std::nullopt);
}

void WebExtensionContext::actionGetEnabled(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(std::optional<bool>, std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = @"action.isEnabled()";

    std::optional<String> error;
    auto action = getActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName, error);
    if (error) {
        completionHandler(std::nullopt, error);
        return;
    }

    ASSERT(action);

    completionHandler(action->isEnabled(), std::nullopt);
}

void WebExtensionContext::actionSetEnabled(std::optional<WebExtensionTabIdentifier> tabIdentifier, bool enabled, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = enabled ? @"action.enable()" : @"action.disable()";

    std::optional<String> error;
    auto action = getOrCreateActionWithIdentifiers(std::nullopt, tabIdentifier, *this, apiName, error);
    if (error) {
        completionHandler(error);
        return;
    }

    ASSERT(action);

    action->setEnabled(enabled);

    completionHandler(std::nullopt);
}

void WebExtensionContext::fireActionClickedEventIfNeeded(WebExtensionTab* tab)
{
    constexpr auto type = WebExtensionEventListenerType::ActionOnClicked;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchActionClickedEvent(tab ? std::optional(tab->parameters()) : std::nullopt));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
