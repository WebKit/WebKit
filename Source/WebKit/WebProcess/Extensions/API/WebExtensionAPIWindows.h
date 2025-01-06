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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPIWindows.h"
#include "WebExtensionAPIObject.h"
#include "WebExtensionAPIWindowsEvent.h"
#include "WebExtensionWindow.h"
#include "WebExtensionWindowIdentifier.h"
#include "WebPageProxyIdentifier.h"

OBJC_CLASS NSDictionary;
OBJC_CLASS NSString;

namespace WebKit {

class WebPage;

class WebExtensionAPIWindows : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPIWindows, windows, windows);

public:
#if PLATFORM(COCOA)
    using PopulateTabs = WebExtensionWindow::PopulateTabs;
    using WindowTypeFilter = WebExtensionWindow::TypeFilter;

    bool isPropertyAllowed(const ASCIILiteral& propertyName, WebPage*);

    void createWindow(NSDictionary *data, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void get(WebPageProxyIdentifier, double windowID, NSDictionary *info, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getCurrent(WebPageProxyIdentifier, NSDictionary *info, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getLastFocused(NSDictionary *info, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getAll(NSDictionary *info, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void update(double windowID, NSDictionary *info, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void remove(double windowID, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    double windowIdentifierNone() const { return WebExtensionWindowConstants::None; }
    double windowIdentifierCurrent() const { return WebExtensionWindowConstants::Current; }

    WebExtensionAPIWindowsEvent& onCreated();
    WebExtensionAPIWindowsEvent& onRemoved();
    WebExtensionAPIWindowsEvent& onFocusChanged();

private:
    friend class WebExtensionAPITabs;
    friend class WebExtensionAPIWindowsEvent;

    static bool parsePopulateTabs(NSDictionary *, PopulateTabs&, NSString *sourceKey, NSString **outExceptionString);
    static bool parseWindowTypesFilter(NSDictionary *, OptionSet<WindowTypeFilter>&, NSString *sourceKey, NSString **outExceptionString);
    static bool parseWindowTypeFilter(NSString *, OptionSet<WindowTypeFilter>&, NSString *sourceKey, NSString **outExceptionString);
    static bool parseWindowGetOptions(NSDictionary *, PopulateTabs&, OptionSet<WindowTypeFilter>&, NSString *sourceKey, NSString **outExceptionString);
    bool parseWindowCreateOptions(NSDictionary *, WebExtensionWindowParameters&, NSString *sourceKey, NSString **outExceptionString);
    static bool parseWindowUpdateOptions(NSDictionary *, WebExtensionWindowParameters&, NSString *sourceKey, NSString **outExceptionString);

    RefPtr<WebExtensionAPIWindowsEvent> m_onCreated;
    RefPtr<WebExtensionAPIWindowsEvent> m_onRemoved;
    RefPtr<WebExtensionAPIWindowsEvent> m_onFocusChanged;
#endif
};

bool isValid(std::optional<WebExtensionWindowIdentifier>, NSString **outExceptionString);

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
