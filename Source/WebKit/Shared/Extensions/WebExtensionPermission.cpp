/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "WebExtensionPermission.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

RefPtr<StringImpl> WebExtensionPermission::activeTab()
{
    return "activeTab"_si;
}

RefPtr<StringImpl> WebExtensionPermission::alarms()
{
    return "alarms"_si;
}

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
RefPtr<StringImpl> WebExtensionPermission::bookmarks()
{
    return "bookmarks"_si;
}
#endif

RefPtr<StringImpl> WebExtensionPermission::clipboardWrite()
{
    return "clipboardWrite"_si;
}

RefPtr<StringImpl> WebExtensionPermission::contextMenus()
{
    return "contextMenus"_si;
}

RefPtr<StringImpl> WebExtensionPermission::cookies()
{
    return "cookies"_si;
}

RefPtr<StringImpl> WebExtensionPermission::declarativeNetRequest()
{
    return "declarativeNetRequest"_si;
}

RefPtr<StringImpl> WebExtensionPermission::declarativeNetRequestFeedback()
{
    return "declarativeNetRequestFeedback"_si;
}

RefPtr<StringImpl> WebExtensionPermission::declarativeNetRequestWithHostAccess()
{
    return "declarativeNetRequestWithHostAccess"_si;
}

RefPtr<StringImpl> WebExtensionPermission::menus()
{
    return "menus"_si;
}

RefPtr<StringImpl> WebExtensionPermission::nativeMessaging()
{
    return "nativeMessaging"_si;
}

RefPtr<StringImpl> WebExtensionPermission::notifications()
{
    return "notifications"_si;
}

#if ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
RefPtr<StringImpl> WebExtensionPermission::offscreen()
{
    return "offscreen"_si;
}
#endif

RefPtr<StringImpl> WebExtensionPermission::scripting()
{
    return "scripting"_si;
}

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
RefPtr<StringImpl> WebExtensionPermission::sidePanel()
{
    return "sidePanel"_si;
}
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

RefPtr<StringImpl> WebExtensionPermission::storage()
{
    return "storage"_si;
}

RefPtr<StringImpl> WebExtensionPermission::tabs()
{
    return "tabs"_si;
}

RefPtr<StringImpl> WebExtensionPermission::unlimitedStorage()
{
    return "unlimitedStorage"_si;
}

RefPtr<StringImpl> WebExtensionPermission::webNavigation()
{
    return "webNavigation"_si;
}

RefPtr<StringImpl> WebExtensionPermission::webRequest()
{
    return "webRequest"_si;
}

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WebExtensionPermissionAdditionsImpl.cpp>)
#include <WebKitAdditions/WebExtensionPermissionAdditionsImpl.cpp>
#endif

} // namespace WebKit

#endif
