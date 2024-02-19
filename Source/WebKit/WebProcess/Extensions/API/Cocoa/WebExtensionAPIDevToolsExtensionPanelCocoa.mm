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
#import "WebExtensionAPIDevToolsExtensionPanel.h"

#if ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)

#import "CocoaHelpers.h"
#import "JSWebExtensionWrapper.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPINamespace.h"
#import "WebProcess.h"

namespace WebKit {

WebExtensionAPIEvent& WebExtensionAPIDevToolsExtensionPanel::onShown()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools/panels/ExtensionPanel

    if (!m_onShown)
        m_onShown = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::DevToolsExtensionPanelOnShown);

    return *m_onShown;
}

WebExtensionAPIEvent& WebExtensionAPIDevToolsExtensionPanel::onHidden()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools/panels/ExtensionPanel

    if (!m_onHidden)
        m_onHidden = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::DevToolsExtensionPanelOnHidden);

    return *m_onHidden;
}

void WebExtensionContextProxy::dispatchDevToolsExtensionPanelShownEvent(Inspector::ExtensionTabID identifier, WebCore::FrameIdentifier frameIdentifier)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools/panels/ExtensionPanel

    RefPtr frame = WebProcess::singleton().webFrame(frameIdentifier);
    if (!frame)
        return;

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        RefPtr extensionPanel = namespaceObject.devtools().panels().extensionPanel(identifier);
        if (!extensionPanel)
            return;

        for (auto& listener : extensionPanel->onShown().listeners()) {
            auto globalContext = listener->globalContext();
            auto *windowObject = toWindowObject(globalContext, *frame) ?: NSNull.null;
            listener->call(windowObject);
        }
    });
}

void WebExtensionContextProxy::dispatchDevToolsExtensionPanelHiddenEvent(Inspector::ExtensionTabID identifier)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools/panels/ExtensionPanel

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        RefPtr extensionPanel = namespaceObject.devtools().panels().extensionPanel(identifier);
        if (!extensionPanel)
            return;

        extensionPanel->onHidden().invokeListeners();
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)
