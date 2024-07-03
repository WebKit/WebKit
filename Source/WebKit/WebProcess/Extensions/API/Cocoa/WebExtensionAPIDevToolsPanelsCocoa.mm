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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPIDevToolsPanels.h"

#if ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)

#import "CocoaHelpers.h"
#import "InspectorExtensionTypes.h"
#import "JSWebExtensionWrapper.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPIEvent.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionContextMessages.h"
#import "WebProcess.h"

namespace WebKit {

RefPtr<WebExtensionAPIDevToolsExtensionPanel> WebExtensionAPIDevToolsPanels::extensionPanel(Inspector::ExtensionTabID identifier) const
{
    return m_extensionPanels.get(identifier);
}

void WebExtensionAPIDevToolsPanels::createPanel(WebPage& page, NSString *title, NSString *iconPath, NSString *pagePath, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools/panels/create

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DevToolsPanelsCreate(page.webPageProxyIdentifier(), title, iconPath, pagePath), [this, protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Inspector::ExtensionTabID, WebExtensionError>&& result) mutable {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        Ref extensionPanel = WebExtensionAPIDevToolsExtensionPanel::create(*this);
        m_extensionPanels.set(result.value(), extensionPanel);

        auto globalContext = callback->globalContext();
        auto *panelValue = toJSValue(globalContext, toJS(globalContext, extensionPanel.ptr()));

        callback->call(panelValue);
    }, extensionContext().identifier());
}

NSString *WebExtensionAPIDevToolsPanels::themeName()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools/panels/themeName

    switch (extensionContext().inspectorAppearance()) {
    case Inspector::ExtensionAppearance::Light:
        return @"light";

    case Inspector::ExtensionAppearance::Dark:
        return @"dark";
    }
}

WebExtensionAPIEvent& WebExtensionAPIDevToolsPanels::onThemeChanged()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools/panels/onThemeChanged

    if (!m_onThemeChanged)
        m_onThemeChanged = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::DevToolsPanelsOnThemeChanged);

    return *m_onThemeChanged;
}

void WebExtensionContextProxy::dispatchDevToolsPanelsThemeChangedEvent(Inspector::ExtensionAppearance appearance)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools/panels/onThemeChanged

    setInspectorAppearance(appearance);

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& panels = namespaceObject.devtools().panels();
        panels.onThemeChanged().invokeListenersWithArgument(panels.themeName());
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)
