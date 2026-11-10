/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "WebExtensionPermission.h"
#import "WebExtensionUtilities.h"

namespace WebKit {

static bool isAncestorOrSelf(WebExtensionContext& context, StringImpl* potentialAncestorIdentifier, StringImpl* identifier)
{
    if (equal(potentialAncestorIdentifier, identifier))
        return true;

    RefPtr current = context.menuItem(identifier);
    while (current) {
        RefPtr parent = current->parentMenuItem();
        if (parent && equal(parent->identifier(), potentialAncestorIdentifier))
            return true;
        current = parent;
    }

    return false;
}

bool WebExtensionContext::isMenusMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && (hasPermission(WebExtensionPermission::contextMenus()) || hasPermission(WebExtensionPermission::menus()));
}

void WebExtensionContext::menusCreate(const WebExtensionMenuItemParameters& parameters, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"menus.create()";

    if (m_menuItems.contains(parameters.identifier)) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"identifier is already used").get()));
        return;
    }

    if (parameters.parentIdentifier && !m_menuItems.contains(parameters.parentIdentifier.value())) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"parent menu item not found").get()));
        return;
    }

    if (parameters.parentIdentifier && isAncestorOrSelf(*this, parameters.parentIdentifier.value(), parameters.identifier)) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"parent menu item cannot be another ancestor").get()));
        return;
    }

    auto menuItem = WebExtensionMenuItem::create(*this, parameters);

    m_menuItems.set(parameters.identifier, menuItem);

    if (!parameters.parentIdentifier)
        m_mainMenuItems.append(menuItem);

    completionHandler({ });
}

void WebExtensionContext::menusUpdate(StringImpl* identifier, const WebExtensionMenuItemParameters& parameters, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"menus.update()";

    RefPtr menuItem = this->menuItem(identifier);
    if (!menuItem) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"menu item not found").get()));
        return;
    }

    if (!isEmpty(parameters.identifier) && !equal(identifier, parameters.identifier)) {
        m_menuItems.remove(identifier);
        m_menuItems.set(parameters.identifier, *menuItem);
    }

    if (parameters.parentIdentifier && !m_menuItems.contains(parameters.parentIdentifier.value())) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"parent menu item not found").get()));
        return;
    }

    if (parameters.parentIdentifier && isAncestorOrSelf(*this, parameters.parentIdentifier.value(), !isEmpty(parameters.identifier) ? parameters.identifier.get() : identifier)) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"parent menu item cannot be itself or another ancestor").get()));
        return;
    }

    menuItem->update(parameters);

    completionHandler({ });
}

void WebExtensionContext::menusRemove(StringImpl* identifier, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr menuItem = this->menuItem(identifier);
    if (!menuItem) {
        completionHandler(toWebExtensionError(StringImpl::create(@"menus.remove()").get(), nullString(), StringImpl::create(@"menu item not found").get()));
        return;
    }

    Function<void(WebExtensionMenuItem&)> removeRecursive;
    removeRecursive = [this, protectedThis = Ref { *this }, &removeRecursive](WebExtensionMenuItem& menuItem) {
        for (auto& submenuItem : menuItem.submenuItems())
            removeRecursive(submenuItem);

        m_menuItems.remove(menuItem.identifier());

        if (!menuItem.parentMenuItem())
            m_mainMenuItems.removeAll(menuItem);
    };

    removeRecursive(*menuItem);

    completionHandler({ });
}

void WebExtensionContext::menusRemoveAll(CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    m_menuItems.clear();
    m_mainMenuItems.clear();

    completionHandler({ });
}

void WebExtensionContext::fireMenusClickedEventIfNeeded(const WebExtensionMenuItem& menuItem, bool wasChecked, const WebExtensionMenuItemContextParameters& contextParameters)
{
    RefPtr tab = contextParameters.tabIdentifier ? getTab(contextParameters.tabIdentifier.value()) : nullptr;

    constexpr auto type = WebExtensionEventListenerType::MenusOnClicked;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }, menuItem = Ref { menuItem }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchMenusClickedEvent(menuItem->minimalParameters(), wasChecked, contextParameters, tab ? std::optional { tab->parameters() } : std::nullopt));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
