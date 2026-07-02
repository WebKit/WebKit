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

#import "config.h"
#import "WebExtensionMenuItem.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionContext.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionMatchPattern.h"
#import "WebExtensionMenuItemContextParameters.h"
#import "WebExtensionMenuItemParameters.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionMenuItem);

bool WebExtensionMenuItem::operator==(const WebExtensionMenuItem& other) const
{
    return this == &other || (m_extensionContext == other.m_extensionContext && m_identifier == other.m_identifier);
}

WebExtensionContext* WebExtensionMenuItem::extensionContext() const
{
    return m_extensionContext.get();
}

String WebExtensionMenuItem::removeAmpersands(const String& title)
{
    // The title may contain ampersands used to indicate shortcut keys for the item. We don't support keyboard
    // shortcuts this way, but we still remove the ampersands from the visible title of the item.

    StringBuilder stringBuilder;
    size_t startIndex = 0;

    while (startIndex < title.length()) {
        size_t ampersandPosition = title.find('&', startIndex);

        // If no ampersand is found, append the rest of the string and break.
        if (ampersandPosition == notFound) {
            stringBuilder.append(title.substring(startIndex));
            break;
        }

        // Append the part of the string before the ampersand.
        if (ampersandPosition > startIndex)
            stringBuilder.append(title.substring(startIndex, ampersandPosition - startIndex));

        // If a double ampersand is found, replace it with a single ampersand.
        if (ampersandPosition < title.length() - 1 && title[ampersandPosition + 1] == '&') {
            stringBuilder.append('&');
            startIndex = ampersandPosition + 2;
            continue;
        }

        // Skip the single ampersand.
        startIndex = ampersandPosition + 1;
    }

    return stringBuilder.toString();
}

bool WebExtensionMenuItem::matches(const WebExtensionMenuItemContextParameters& contextParameters) const
{
    if (!contexts().containsAny(contextParameters.types))
        return false;

    using ContextType = WebExtensionMenuItemContextType;

    auto matchesType = [&](const OptionSet<ContextType>& types) {
        for (auto type : types) {
            if (contextParameters.types.contains(type) && contexts().contains(type))
                return true;
        }

        return false;
    };

    auto matchesPattern = [&](const auto& patterns, const URL& url) {
        if (url.isNull() || patterns.isEmpty())
            return true;

        for (const auto& pattern : patterns) {
            if (pattern->matchesURL(url))
                return true;
        }

        return false;
    };

    // Document patterns match for any context type.
    if (!matchesPattern(documentPatterns(), contextParameters.frameURL))
        return false;

    if (matchesType({ ContextType::Action, ContextType::Tab })) {
        // Additional context checks are not required for Action or Tab.
        return true;
    }

    if (matchesType(ContextType::Link)) {
        ASSERT(!contextParameters.linkURL.isNull());
        if (!matchesPattern(targetPatterns(), contextParameters.linkURL))
            return false;
    }

    if (matchesType({ ContextType::Image, ContextType::Video, ContextType::Audio })) {
        ASSERT(!contextParameters.sourceURL.isNull());
        if (!matchesPattern(targetPatterns(), contextParameters.sourceURL))
            return false;
    }

    if (matchesType(ContextType::Selection) && contextParameters.selectionString.isEmpty())
        return false;

    if (matchesType(ContextType::Editable) && !contextParameters.editable)
        return false;

    return true;
}

bool WebExtensionMenuItem::toggleCheckedIfNeeded(const WebExtensionMenuItemContextParameters& contextParameters)
{
    ASSERT(extensionContext());

    bool wasChecked = isChecked();

    switch (type()) {
    case WebExtensionMenuItemType::Normal:
    case WebExtensionMenuItemType::Separator:
        ASSERT(!wasChecked);
        break;

    case WebExtensionMenuItemType::Checkbox:
        setChecked(!wasChecked);
        break;

    case WebExtensionMenuItemType::Radio:
        if (wasChecked)
            break;

        setChecked(true);

        auto& items = parentMenuItem() ? parentMenuItem()->submenuItems() : extensionContext()->mainMenuItems();

        auto index = items.find(*this);
        if (index == notFound)
            break;

        // Uncheck all radio items in the same group before the current item.
        for (ssize_t i = index - 1; i >= 0; --i) {
            auto& item = items[i];
            if (!item->matches(contextParameters))
                continue;

            if (item->type() != WebExtensionMenuItemType::Radio)
                break;

            item->setChecked(false);
        }

        // Uncheck all radio items in the same group after the current item.
        for (size_t i = index + 1; i < items.size(); ++i) {
            auto& item = items[i];
            if (!item->matches(contextParameters))
                continue;

            if (item->type() != WebExtensionMenuItemType::Radio)
                break;

            item->setChecked(false);
        }

        break;
    }

    return wasChecked;
}

void WebExtensionMenuItem::addSubmenuItem(WebExtensionMenuItem& menuItem)
{
    menuItem.m_parentMenuItem = this;
    m_submenuItems.append(menuItem);
}

void WebExtensionMenuItem::removeSubmenuItem(WebExtensionMenuItem& menuItem)
{
    menuItem.m_parentMenuItem = nullptr;
    m_submenuItems.removeAll(menuItem);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
