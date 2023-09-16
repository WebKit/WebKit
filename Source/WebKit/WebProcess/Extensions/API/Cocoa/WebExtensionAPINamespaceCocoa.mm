/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "CocoaHelpers.h"
#import "WebExtensionAPINamespace.h"
#import <WebKit/_WKWebExtensionPermission.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

bool WebExtensionAPINamespace::isPropertyAllowed(ASCIILiteral name, WebPage*)
{
    // This property is only allowed in testing contexts.
    if (name == "test"_s)
        return extensionContext().inTestingMode();

    // FIXME: https://webkit.org/b/259914 This should be a hasPermission: call to extensionContext() and updated with actually granted permissions from the UI process.
    auto *permissions = objectForKey<NSArray>(extensionContext().manifest(), @"permissions", true, NSString.class);
    return [permissions containsObject:name.createNSString().get()];
}

WebExtensionAPIAlarms& WebExtensionAPINamespace::alarms()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms

    if (!m_alarms)
        m_alarms = WebExtensionAPIAlarms::create(forMainWorld(), runtime(), extensionContext());

    return *m_alarms;
}

WebExtensionAPIExtension& WebExtensionAPINamespace::extension()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extension

    if (!m_extension)
        m_extension = WebExtensionAPIExtension::create(forMainWorld(), runtime(), extensionContext());

    return *m_extension;
}

WebExtensionAPILocalization& WebExtensionAPINamespace::i18n()
{
    if (!m_i18n)
        m_i18n = WebExtensionAPILocalization::create(forMainWorld(), runtime(), extensionContext());
    return *m_i18n;
}

WebExtensionAPIPermissions& WebExtensionAPINamespace::permissions()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/permissions

    if (!m_permissions)
        m_permissions = WebExtensionAPIPermissions::create(forMainWorld(), runtime(), extensionContext());

    return *m_permissions;
}

WebExtensionAPIRuntime& WebExtensionAPINamespace::runtime()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime

    if (!m_runtime)
        m_runtime = WebExtensionAPIRuntime::create(forMainWorld(), extensionContext());

    return *m_runtime;
}

WebExtensionAPITabs& WebExtensionAPINamespace::tabs()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs

    if (!m_tabs)
        m_tabs = WebExtensionAPITabs::create(forMainWorld(), runtime(), extensionContext());

    return *m_tabs;
}

WebExtensionAPITest& WebExtensionAPINamespace::test()
{
    // Documentation: None (Testing Only)

    if (!m_test)
        m_test = WebExtensionAPITest::create(forMainWorld(), runtime(), extensionContext());

    return *m_test;
}

WebExtensionAPIWindows& WebExtensionAPINamespace::windows()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows

    if (!m_windows)
        m_windows = WebExtensionAPIWindows::create(forMainWorld(), runtime(), extensionContext());

    return *m_windows;
}

WebExtensionAPIWebNavigation& WebExtensionAPINamespace::webNavigation()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation

    if (!m_webNavigation)
        m_webNavigation = WebExtensionAPIWebNavigation::create(forMainWorld(), runtime(), extensionContext());

    return *m_webNavigation;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
