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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include <wtf/text/WTFString.h>

namespace WebKit {

/* Constants for specifying permission in a WebExtensionContext. */
class WebExtensionPermission {
public:
    static RefPtr<StringImpl> activeTab();
    static RefPtr<StringImpl> alarms();
#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
    static RefPtr<StringImpl> bookmarks();
#endif
    static RefPtr<StringImpl> clipboardWrite();
    static RefPtr<StringImpl> contextMenus();
    static RefPtr<StringImpl> cookies();
    static RefPtr<StringImpl> declarativeNetRequest();
    static RefPtr<StringImpl> declarativeNetRequestFeedback();
    static RefPtr<StringImpl> declarativeNetRequestWithHostAccess();
    static RefPtr<StringImpl> menus();
    static RefPtr<StringImpl> nativeMessaging();
    static RefPtr<StringImpl> notifications();
#if ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
    static RefPtr<StringImpl> offscreen();
#endif
    static RefPtr<StringImpl> scripting();
#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    static RefPtr<StringImpl> sidePanel();
#endif
    static RefPtr<StringImpl> storage();
    static RefPtr<StringImpl> tabs();
    static RefPtr<StringImpl> unlimitedStorage();
    static RefPtr<StringImpl> webNavigation();
    static RefPtr<StringImpl> webRequest();

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WebExtensionPermissionAdditions.h>)
#include <WebKitAdditions/WebExtensionPermissionAdditions.h>
#endif
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
