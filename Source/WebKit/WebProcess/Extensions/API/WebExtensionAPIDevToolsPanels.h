/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)

#include "JSWebExtensionAPIDevToolsPanels.h"
#include "WebExtensionAPIDevToolsExtensionPanel.h"
#include "WebExtensionAPIEvent.h"
#include "WebExtensionAPIObject.h"

namespace WebKit {

class WebPage;

class WebExtensionAPIDevToolsPanels : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPIDevToolsPanels, devToolsPanels, devtools.panels);

public:
#if PLATFORM(COCOA)
    RefPtr<WebExtensionAPIDevToolsExtensionPanel> extensionPanel(Inspector::ExtensionTabID) const;

    void createPanel(WebPage&, NSString *title, NSString *iconPath, NSString *pagePath, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    NSString *themeName();

    Inspector::ExtensionAppearance theme() const { return m_theme; }
    void setTheme(Inspector::ExtensionAppearance appearance) { m_theme = appearance; }

    WebExtensionAPIEvent& onThemeChanged();
#endif

private:
    RefPtr<WebExtensionAPIEvent> m_onThemeChanged;
    HashMap<Inspector::ExtensionTabID, Ref<WebExtensionAPIDevToolsExtensionPanel>> m_extensionPanels;
    Inspector::ExtensionAppearance m_theme { Inspector::ExtensionAppearance::Light };
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)
