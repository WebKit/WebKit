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

#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionMenuItem.h"
#import "WebExtensionMenuItemContextParameters.h"
#import "WebExtensionMenuItemParameters.h"
#import "WebExtensionUtilities.h"

namespace WebKit {

static bool isAncestorOrSelf(WebExtensionContext& context, const String& potentialAncestorIdentifier, const String& identifier)
{
    if (potentialAncestorIdentifier == identifier)
        return true;

    RefPtr current = context.menuItem(identifier);
    while (current) {
        RefPtr parent = current->parentMenuItem();
        if (parent && parent->identifier() == potentialAncestorIdentifier)
            return true;
        current = parent;
    }

    return false;
}

void WebExtensionContext::menusCreate(const WebExtensionMenuItemParameters& parameters, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = @"menus.create()";

    if (m_menuItems.contains(parameters.identifier)) {
        completionHandler(toErrorString(apiName, nil, @"identifier is already used"));
        return;
    }

    if (parameters.parentIdentifier && !m_menuItems.contains(parameters.parentIdentifier.value())) {
        completionHandler(toErrorString(apiName, nil, @"parent menu item not found"));
        return;
    }

    if (parameters.parentIdentifier && isAncestorOrSelf(*this, parameters.parentIdentifier.value(), parameters.identifier)) {
        completionHandler(toErrorString(apiName, nil, @"parent menu item cannot be another ancestor"));
        return;
    }

    auto menuItem = WebExtensionMenuItem::create(*this, parameters);

    m_menuItems.set(parameters.identifier, menuItem);

    if (!parameters.parentIdentifier)
        m_mainMenuItems.append(menuItem);

    completionHandler(std::nullopt);
}

void WebExtensionContext::menusUpdate(const String& identifier, const WebExtensionMenuItemParameters& parameters, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    static NSString * const apiName = @"menus.update()";

    RefPtr menuItem = this->menuItem(identifier);
    if (!menuItem) {
        completionHandler(toErrorString(apiName, nil, @"menu item not found"));
        return;
    }

    if (!parameters.identifier.isEmpty() && identifier != parameters.identifier) {
        m_menuItems.remove(identifier);
        m_menuItems.set(parameters.identifier, *menuItem);
    }

    if (parameters.parentIdentifier && !m_menuItems.contains(parameters.parentIdentifier.value())) {
        completionHandler(toErrorString(apiName, nil, @"parent menu item not found"));
        return;
    }

    if (parameters.parentIdentifier && isAncestorOrSelf(*this, parameters.parentIdentifier.value(), !parameters.identifier.isEmpty() ? parameters.identifier : identifier)) {
        completionHandler(toErrorString(apiName, nil, @"parent menu item cannot be itself or another ancestor"));
        return;
    }

    menuItem->update(parameters);

    completionHandler(std::nullopt);
}

void WebExtensionContext::menusRemove(const String& identifier, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    RefPtr menuItem = this->menuItem(identifier);
    if (!menuItem) {
        completionHandler(toErrorString(@"menus.remove()", nil, @"menu item not found"));
        return;
    }

    Function<void(WebExtensionMenuItem&)> removeRecursive;
    removeRecursive = [&](WebExtensionMenuItem& menuItem) {
        for (auto& submenuItem : menuItem.submenuItems())
            removeRecursive(submenuItem);

        m_menuItems.remove(menuItem.identifier());

        if (!menuItem.parentMenuItem())
            m_mainMenuItems.removeAll(menuItem);
    };

    removeRecursive(*menuItem);

    completionHandler(std::nullopt);
}

void WebExtensionContext::menusRemoveAll(CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    m_menuItems.clear();
    m_mainMenuItems.clear();

    completionHandler(std::nullopt);
}

void WebExtensionContext::fireMenusClickedEventIfNeeded(const WebExtensionMenuItem& menuItem, bool wasChecked, const WebExtensionMenuItemContextParameters& contextParameters)
{
    auto tab = contextParameters.tabIdentifier ? getTab(contextParameters.tabIdentifier.value()) : nullptr;

    constexpr auto type = WebExtensionEventListenerType::MenusOnClicked;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchMenusClickedEvent(menuItem.minimalParameters(), wasChecked, contextParameters, tab ? std::optional { tab->parameters() } : std::nullopt));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
