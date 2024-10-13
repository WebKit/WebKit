/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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

String WebExtensionPermission::activeTab()
{
    return "activeTab"_s;
}

String WebExtensionPermission::alarms()
{
    return "alarms"_s;
}

String WebExtensionPermission::clipboardWrite()
{
    return "clipboardWrite"_s;
}

String WebExtensionPermission::contextMenus()
{
    return "contextMenus"_s;
}

String WebExtensionPermission::cookies()
{
    return "cookies"_s;
}

String WebExtensionPermission::declarativeNetRequest()
{
    return "declarativeNetRequest"_s;
}

String WebExtensionPermission::declarativeNetRequestFeedback()
{
    return "declarativeNetRequestFeedback"_s;
}

String WebExtensionPermission::declarativeNetRequestWithHostAccess()
{
    return "declarativeNetRequestWithHostAccess"_s;
}

String WebExtensionPermission::menus()
{
    return "menus"_s;
}

String WebExtensionPermission::nativeMessaging()
{
    return "nativeMessaging"_s;
}

String WebExtensionPermission::notifications()
{
    return "notifications"_s;
}

String WebExtensionPermission::scripting()
{
    return "scripting"_s;
}

#if ENABLE(WK_WEB_EXTENSION_SIDEBAR)
String WebExtensionPermission::sidePanel()
{
    return "sidePanel"_s;
}
#endif // ENABLE(WK_WEB_EXTENSION_SIDEBAR)

String WebExtensionPermission::storage()
{
    return "storage"_s;
}

String WebExtensionPermission::tabs()
{
    return "tabs"_s;
}

String WebExtensionPermission::unlimitedStorage()
{
    return "unlimitedStorage"_s;
}

String WebExtensionPermission::webNavigation()
{
    return "webNavigation"_s;
}

String WebExtensionPermission::webRequest()
{
    return "webRequest"_s;
}

} // namespace WebKit

#endif
