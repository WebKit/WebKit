/*
 * Copyright (C) 2022-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "WebExtensionAPINamespace.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionControllerProxy.h"
#include "WebExtensionPermission.h"

#if ENABLE(INSPECTOR_EXTENSIONS) || ENABLE(WK_WEB_EXTENSIONS_SIDEBAR) ||  ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS) || ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
#include "WebPage.h"
#include <WebCore/Page.h>
#include <WebCore/Settings.h>
#endif

namespace WebKit {

static bool doesDictionaryExist(RefPtr<JSON::Object> value, const String& name, bool returningFalseIfEmpty = false)
{
    RefPtr object = value ? value->getObject(name) : nullptr;
    if (!object)
        return false;
    return !returningFalseIfEmpty || object->size();
}

#if ENABLE(INSPECTOR_EXTENSIONS)
static bool doesKeyExist(RefPtr<JSON::Object> value, const String& key)
{
    return value && !value->getString(key).isEmpty();
}
#endif

bool WebExtensionAPINamespace::isPropertyAllowed(const ASCIILiteral& name, WebPage* page)
{
    Ref extensionContext = this->extensionContext();
    if (extensionContext->isUnsupportedAPI(propertyPath(), name)) [[unlikely]]
        return false;

    if (name == "test"_s)
        return extensionContext->inTestingMode();

#if ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
    if (name == "offscreen"_s)
        return page && page->corePage() && page->corePage()->settings().webExtensionOffscreenEnabled() && extensionContext->hasPermission("offscreen"_s);

    // The offscreen document is not a full extension environment; only runtime and test are reachable from it.
    if (page && extensionContext->isOffscreenPage(*page))
        return name == "runtime"_s;
#endif

    if (name == "action"_s)
        return extensionContext->supportsManifestVersion(3) && doesDictionaryExist(extensionContext->manifest(), "action"_s);

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
    if (name == "bookmarks"_s)
        return page->corePage()->settings().webExtensionBookmarksEnabled() && extensionContext->hasPermission("bookmarks"_s);
#endif

    if (name == "commands"_s)
        return doesDictionaryExist(extensionContext->manifest(), "commands"_s);

    if (name == "declarativeNetRequest"_s)
        return extensionContext->hasPermission(name) || extensionContext->hasPermission("declarativeNetRequestWithHostAccess"_s);

    if (name == "browserAction"_s)
        return !extensionContext->supportsManifestVersion(3) && doesDictionaryExist(extensionContext->manifest(), "browser_action"_s);

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (name == "devtools"_s)
        return doesKeyExist(extensionContext->manifest(), "devtools_page"_s) && page && (page->isInspectorPage() || extensionContext->isInspectorBackgroundPage(*page));
#else
    if (name == "devtools"_s)
        return false;
#endif

    if (name == "notifications"_s) {
        // FIXME: <rdar://problem/57202210> Add support for browser.notifications.
        // Notifications are currently only available in test mode as an empty stub.
        if (!extensionContext->inTestingMode())
            return false;
        goto finish;
    }

    if (name == "pageAction"_s)
        return !extensionContext->supportsManifestVersion(3) && doesDictionaryExist(extensionContext->manifest(), "page_action"_s);

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    // If the extension requests both sidePanel and sidebarAction, we will give them sidebarAction --
    // we check in sidePanel that there is no sidebar_action key, but we do not check in sidebarAction
    // that there is no sidePanel permission
    if (name == "sidePanel"_s)
        return page && page->corePage() && page->corePage()->settings().webExtensionSidebarEnabled() && extensionContext->hasPermission("sidePanel"_s) && !doesDictionaryExist(extensionContext->manifest(), "sidebar_action"_s, true);
    if (name == "sidebarAction"_s)
        return page && page->corePage() && page->corePage()->settings().webExtensionSidebarEnabled() && doesDictionaryExist(extensionContext->manifest(), "sidebar_action"_s, true);
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

    if (name == "storage"_s)
        return extensionContext->hasPermission(name) || extensionContext->hasPermission("unlimitedStorage"_s);

    if (name == "dom"_s || name == "extension"_s || name == "i18n"_s || name == "permissions"_s || name == "runtime"_s || name == "tabs"_s || name == "windows"_s)
        return true;

finish:
    // The rest of the property names marked dynamic in WebExtensionAPINamespace.idl match permission names.
    // Check for the permission to determine if the property is allowed to be accessed.
    return extensionContext->hasPermission(name);
}

#if PLATFORM(COCOA)
WebExtensionAPIAction& WebExtensionAPINamespace::action()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action

    if (!m_action)
        m_action = WebExtensionAPIAction::create(*this);

    return *m_action;
}
#endif

WebExtensionAPIAlarms& WebExtensionAPINamespace::alarms()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms

    if (!m_alarms)
        m_alarms = WebExtensionAPIAlarms::create(*this);

    return *m_alarms;
}

#if PLATFORM(COCOA)
WebExtensionAPICommands& WebExtensionAPINamespace::commands()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/commands

    if (!m_commands)
        m_commands = WebExtensionAPICommands::create(*this);

    return *m_commands;
}

WebExtensionAPICookies& WebExtensionAPINamespace::cookies()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies

    if (!m_cookies)
        m_cookies = WebExtensionAPICookies::create(*this);

    return *m_cookies;
}

WebExtensionAPIDeclarativeNetRequest& WebExtensionAPINamespace::declarativeNetRequest()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/declarativeNetRequest

    if (!m_declarativeNetRequest)
        m_declarativeNetRequest = WebExtensionAPIDeclarativeNetRequest::create(*this);

    return *m_declarativeNetRequest;
}

#if ENABLE(INSPECTOR_EXTENSIONS)
WebExtensionAPIDevTools& WebExtensionAPINamespace::devtools()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools

    if (!m_devtools)
        m_devtools = WebExtensionAPIDevTools::create(*this);

    return *m_devtools;
}
#endif // ENABLE(INSPECTOR_EXTENSIONS)

WebExtensionAPIDOM& WebExtensionAPINamespace::dom()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/dom

    if (!m_dom)
        m_dom = WebExtensionAPIDOM::create(*this);

    return *m_dom;
}

WebExtensionAPIExtension& WebExtensionAPINamespace::extension()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extension

    if (!m_extension)
        m_extension = WebExtensionAPIExtension::create(*this);

    return *m_extension;
}

WebExtensionAPILocalization& WebExtensionAPINamespace::i18n()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/i18n

    if (!m_i18n)
        m_i18n = WebExtensionAPILocalization::create(*this);

    return *m_i18n;
}

WebExtensionAPIMenus& WebExtensionAPINamespace::menus()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/menus

    if (!m_menus)
        m_menus = WebExtensionAPIMenus::create(*this);

    return *m_menus;
}

#if ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
WebExtensionAPIOffscreen& WebExtensionAPINamespace::offscreen()
{
    // Documentation: https://developer.chrome.com/docs/extensions/reference/api/offscreen

    if (!m_offscreen)
        m_offscreen = WebExtensionAPIOffscreen::create(*this);

    return *m_offscreen;
}
#endif

WebExtensionAPINotifications& WebExtensionAPINamespace::notifications()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications

    if (!m_notifications)
        m_notifications = WebExtensionAPINotifications::create(*this);

    return *m_notifications;
}

WebExtensionAPIPermissions& WebExtensionAPINamespace::permissions()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/permissions

    if (!m_permissions)
        m_permissions = WebExtensionAPIPermissions::create(*this);

    return *m_permissions;
}

WebExtensionAPIRuntime& WebExtensionAPINamespace::runtime() const
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime

    if (!m_runtime) {
        m_runtime = WebExtensionAPIRuntime::create(contentWorldType(), protect(extensionContext()));
        m_runtime->setPropertyPath("runtime"_s, this);
    }

    return *m_runtime;
}

WebExtensionAPIScripting& WebExtensionAPINamespace::scripting()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting

    if (!m_scripting)
        m_scripting = WebExtensionAPIScripting::create(*this);

    return *m_scripting;
}

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
WebExtensionAPISidePanel& WebExtensionAPINamespace::sidePanel()
{
    // Documentation: https://developer.chrome.com/docs/extensions/reference/api/sidePanel

    if (!m_sidePanel)
        m_sidePanel = WebExtensionAPISidePanel::create(*this);

    return *m_sidePanel;
}

WebExtensionAPISidebarAction& WebExtensionAPINamespace::sidebarAction()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/sidebarAction

    if (!m_sidebarAction)
        m_sidebarAction = WebExtensionAPISidebarAction::create(*this);

    return *m_sidebarAction;
}
#endif

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
WebExtensionAPIBookmarks& WebExtensionAPINamespace::bookmarks()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks

    if (!m_bookmarks)
        m_bookmarks = WebExtensionAPIBookmarks::create(*this);

    return *m_bookmarks;
}
#endif

WebExtensionAPIStorage& WebExtensionAPINamespace::storage()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage

    if (!m_storage)
        m_storage = WebExtensionAPIStorage::create(*this);

    return *m_storage;
}

WebExtensionAPITabs& WebExtensionAPINamespace::tabs()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs

    if (!m_tabs)
        m_tabs = WebExtensionAPITabs::create(*this);

    return *m_tabs;
}
#endif

WebExtensionAPITest& WebExtensionAPINamespace::test()
{
    // Documentation: None (Testing Only)

    if (!m_test)
        m_test = WebExtensionAPITest::create(*this);

    return *m_test;
}

#if PLATFORM(COCOA)
WebExtensionAPIWindows& WebExtensionAPINamespace::windows()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows

    if (!m_windows)
        m_windows = WebExtensionAPIWindows::create(*this);

    return *m_windows;
}

WebExtensionAPIWebNavigation& WebExtensionAPINamespace::webNavigation()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation

    if (!m_webNavigation)
        m_webNavigation = WebExtensionAPIWebNavigation::create(*this);

    return *m_webNavigation;
}

WebExtensionAPIWebRequest& WebExtensionAPINamespace::webRequest()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest

    if (!m_webRequest)
        m_webRequest = WebExtensionAPIWebRequest::create(*this);

    return *m_webRequest;
}
#endif

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
