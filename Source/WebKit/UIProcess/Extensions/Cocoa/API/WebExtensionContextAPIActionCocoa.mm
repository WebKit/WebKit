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

static Expected<Ref<WebExtensionAction>, WebExtensionError> getActionWithIdentifiers(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, WebExtensionContext& context, NSString *apiName)
{
    if (windowIdentifier) {
        RefPtr window = context.getWindow(windowIdentifier.value());
        if (!window)
            return toWebExtensionError(apiName, nil, @"window not found");

        return context.getAction(window.get());
    }

    if (tabIdentifier) {
        RefPtr tab = context.getTab(tabIdentifier.value());
        if (!tab)
            return toWebExtensionError(apiName, nil, @"tab not found");

        return context.getAction(tab.get());
    }

    return Ref { context.defaultAction() };
}

static Expected<Ref<WebExtensionAction>, WebExtensionError> getOrCreateActionWithIdentifiers(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, WebExtensionContext& context, NSString *apiName)
{
    if (windowIdentifier) {
        RefPtr window = context.getWindow(windowIdentifier.value());
        if (!window)
            return toWebExtensionError(apiName, nil, @"window not found");

        return context.getOrCreateAction(window.get());
    }

    if (tabIdentifier) {
        RefPtr tab = context.getTab(tabIdentifier.value());
        if (!tab)
            return toWebExtensionError(apiName, nil, @"tab not found");

        return context.getOrCreateAction(tab.get());
    }

    return Ref { context.defaultAction() };
}

bool WebExtensionContext::isActionMessageAllowed()
{
    Ref extension = *m_extension;
    return isLoaded() && (extension->hasAction() || extension->hasBrowserAction() || extension->hasPageAction());
}

void WebExtensionContext::actionGetTitle(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"action.getTitle()";

    auto action = getActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName);
    if (!action) {
        completionHandler(makeUnexpected(action.error()));
        return;
    }

    completionHandler(Ref { action.value() }->label(WebExtensionAction::FallbackWhenEmpty::No));
}

void WebExtensionContext::actionSetTitle(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const String& title, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"action.setTitle()";

    auto action = getOrCreateActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName);
    if (!action) {
        completionHandler(makeUnexpected(action.error()));
        return;
    }

    Ref { action.value() }->setLabel(title);

    completionHandler({ });
}

void WebExtensionContext::actionSetIcon(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const String& iconsJSON, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"action.setIcon()";

    auto action = getOrCreateActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName);
    if (!action) {
        completionHandler(makeUnexpected(action.error()));
        return;
    }

    id parsedIcons = parseJSON(iconsJSON, JSONOptions::FragmentsAllowed);
    Ref webExtensionAction = action.value();

    if (auto *dictionary = dynamic_objc_cast<NSDictionary>(parsedIcons))
        webExtensionAction->setIcons(dictionary);
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    else if (auto *array = dynamic_objc_cast<NSArray>(parsedIcons))
        webExtensionAction->setIconVariants(array);
#endif
    else {
        webExtensionAction->setIcons(nil);
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        webExtensionAction->setIconVariants(nil);
#endif
    }

    completionHandler({ });
}

void WebExtensionContext::actionGetPopup(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"action.getPopup()";

    auto action = getActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName);
    if (!action) {
        completionHandler(makeUnexpected(action.error()));
        return;
    }

    completionHandler(Ref { action.value() }->popupPath());
}

void WebExtensionContext::actionSetPopup(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const String& popupPath, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"action.setPopup()";

    auto action = getOrCreateActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName);
    if (!action) {
        completionHandler(makeUnexpected(action.error()));
        return;
    }

    Ref { action.value() }->setPopupPath(popupPath);

    completionHandler({ });
}

void WebExtensionContext::actionOpenPopup(WebPageProxyIdentifier identifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"action.openPopup()";

    if (!protectedDefaultAction()->canProgrammaticallyPresentPopup()) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented"));
        return;
    }

    if (extensionController()->isShowingActionPopup()) {
        completionHandler(toWebExtensionError(apiName, nil, @"another popup is already open"));
        return;
    }

    RefPtr<WebExtensionWindow> window;
    RefPtr<WebExtensionTab> tab;

    if (windowIdentifier) {
        window = getWindow(windowIdentifier.value());
        if (!window) {
            completionHandler(toWebExtensionError(apiName, nil, @"window not found"));
            return;
        }

        tab = window->activeTab();
        if (!tab) {
            completionHandler(toWebExtensionError(apiName, nil, @"active tab not found in window"));
            return;
        }
    }

    if (tabIdentifier) {
        tab = getTab(tabIdentifier.value());
        if (!tab) {
            completionHandler(toWebExtensionError(apiName, nil, @"tab not found"));
            return;
        }
    }

    if (!tab) {
        window = frontmostWindow();
        if (!window) {
            completionHandler(toWebExtensionError(apiName, nil, @"no windows open"));
            return;
        }

        tab = window->activeTab();
        if (!tab) {
            completionHandler(toWebExtensionError(apiName, nil, @"active tab not found in window"));
            return;
        }
    }

    if (getOrCreateAction(tab.get())->presentsPopup())
        performAction(tab.get(), UserTriggered::No);

    completionHandler({ });
}

void WebExtensionContext::actionGetBadgeText(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"action.getBadgeText()";

    auto action = getActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName);
    if (!action) {
        completionHandler(makeUnexpected(action.error()));
        return;
    }

    completionHandler(Ref { action.value() }->badgeText());
}

void WebExtensionContext::actionSetBadgeText(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const String& text, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"action.setBadgeText()";

    auto action = getOrCreateActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName);
    if (!action) {
        completionHandler(makeUnexpected(action.error()));
        return;
    }

    Ref { action.value() }->setBadgeText(text);

    completionHandler({ });
}

void WebExtensionContext::actionGetEnabled(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<bool, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"action.isEnabled()";

    auto action = getActionWithIdentifiers(windowIdentifier, tabIdentifier, *this, apiName);
    if (!action) {
        completionHandler(makeUnexpected(action.error()));
        return;
    }

    completionHandler(Ref { action.value() }->isEnabled());
}

void WebExtensionContext::actionSetEnabled(std::optional<WebExtensionTabIdentifier> tabIdentifier, bool enabled, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    auto action = getOrCreateActionWithIdentifiers(std::nullopt, tabIdentifier, *this, enabled ? @"action.enable()" : @"action.disable()");
    if (!action) {
        completionHandler(makeUnexpected(action.error()));
        return;
    }

    Ref { action.value() }->setEnabled(enabled);

    completionHandler({ });
}

void WebExtensionContext::fireActionClickedEventIfNeeded(WebExtensionTab* tab)
{
    constexpr auto type = WebExtensionEventListenerType::ActionOnClicked;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }, tab = RefPtr { tab }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchActionClickedEvent(tab ? std::optional(tab->parameters()) : std::nullopt));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
