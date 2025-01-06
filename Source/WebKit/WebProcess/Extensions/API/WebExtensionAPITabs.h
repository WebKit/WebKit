/*
  *Copyright (C) 2023 Apple Inc. All rights reserved.
 *
  *Redistribution and use in source and binary forms, with or without
  *modification, are permitted provided that the following conditions
  *are met:
  *1. Redistributions of source code must retain the above copyright
  *   notice, this list of conditions and the following disclaimer.
  *2. Redistributions in binary form must reproduce the above copyright
  *   notice, this list of conditions and the following disclaimer in the
  *   documentation and/or other materials provided with the distribution.
 *
  *THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
  *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  *THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  *PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
  *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  *THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPITabs.h"
#include "WebExtensionAPIEvent.h"
#include "WebExtensionAPIObject.h"
#include "WebExtensionFrameIdentifier.h"
#include "WebExtensionTab.h"
#include "WebPageProxyIdentifier.h"

OBJC_CLASS NSDictionary;
OBJC_CLASS NSString;

namespace WebKit {

class WebExtensionAPIPort;
struct WebExtensionMessageTargetParameters;
struct WebExtensionScriptInjectionParameters;
struct WebExtensionTabParameters;
struct WebExtensionTabQueryParameters;

class WebExtensionAPITabs : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPITabs, tabs, tabs);

public:
#if PLATFORM(COCOA)
    bool isPropertyAllowed(const ASCIILiteral& propertyName, WebPage*);

    void createTab(WebPageProxyIdentifier, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void query(WebPageProxyIdentifier, NSDictionary *info, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void get(double tabID, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getCurrent(WebPageProxyIdentifier, Ref<WebExtensionCallbackHandler>&&);
    void getSelected(WebPageProxyIdentifier, double windowID, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void duplicate(double tabID, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void update(WebPageProxyIdentifier, double tabID, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void remove(NSObject *tabIDs, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void reload(WebPageProxyIdentifier, double tabID, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void goBack(WebPageProxyIdentifier, double tabID, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void goForward(WebPageProxyIdentifier, double tabID, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void getZoom(WebPageProxyIdentifier, double tabID, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void setZoom(WebPageProxyIdentifier, double tabID, double zoomFactor, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void detectLanguage(WebPageProxyIdentifier, double tabID, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void toggleReaderMode(WebPageProxyIdentifier, double tabID, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void captureVisibleTab(WebPageProxyIdentifier, double windowID, NSDictionary *options, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void sendMessage(WebFrame&, double tabID, NSString *message, NSDictionary *options, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    RefPtr<WebExtensionAPIPort> connect(WebFrame&, JSContextRef, double tabID, NSDictionary *options, NSString **outExceptionString);

    void executeScript(WebPageProxyIdentifier, double tabID, NSDictionary *options, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void insertCSS(WebPageProxyIdentifier, double tabID, NSDictionary *options, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void removeCSS(WebPageProxyIdentifier, double tabID, NSDictionary *options, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    double tabIdentifierNone() const { return -1; }

    WebExtensionAPIEvent& onActivated();
    WebExtensionAPIEvent& onAttached();
    WebExtensionAPIEvent& onCreated();
    WebExtensionAPIEvent& onDetached();
    WebExtensionAPIEvent& onHighlighted();
    WebExtensionAPIEvent& onMoved();
    WebExtensionAPIEvent& onRemoved();
    WebExtensionAPIEvent& onReplaced();
    WebExtensionAPIEvent& onUpdated();

private:
    bool parseTabCreateOptions(NSDictionary *, WebExtensionTabParameters&, NSString *sourceKey, NSString **outExceptionString);
    bool parseTabUpdateOptions(NSDictionary *, WebExtensionTabParameters&, NSString *sourceKey, NSString **outExceptionString);
    static bool parseTabDuplicateOptions(NSDictionary *, WebExtensionTabParameters&, NSString *sourceKey, NSString **outExceptionString);
    static bool parseTabQueryOptions(NSDictionary *, WebExtensionTabQueryParameters&, NSString *sourceKey, NSString **outExceptionString);
    static bool parseCaptureVisibleTabOptions(NSDictionary *, WebExtensionTab::ImageFormat&, uint8_t& imageQuality, NSString *sourceKey, NSString **outExceptionString);
    static bool parseSendMessageOptions(NSDictionary *, WebExtensionMessageTargetParameters&, NSString *sourceKey, NSString **outExceptionString);
    static bool parseConnectOptions(NSDictionary *, std::optional<String>& name, WebExtensionMessageTargetParameters&, NSString *sourceKey, NSString **outExceptionString);
    static bool parseScriptOptions(NSDictionary *, WebExtensionScriptInjectionParameters&, NSString **outExceptionString);

    RefPtr<WebExtensionAPIEvent> m_onActivated;
    RefPtr<WebExtensionAPIEvent> m_onAttached;
    RefPtr<WebExtensionAPIEvent> m_onCreated;
    RefPtr<WebExtensionAPIEvent> m_onDetached;
    RefPtr<WebExtensionAPIEvent> m_onHighlighted;
    RefPtr<WebExtensionAPIEvent> m_onMoved;
    RefPtr<WebExtensionAPIEvent> m_onRemoved;
    RefPtr<WebExtensionAPIEvent> m_onReplaced;
    RefPtr<WebExtensionAPIEvent> m_onUpdated;
#endif
};

bool isValid(std::optional<WebExtensionTabIdentifier>, NSString **outExceptionString);
NSDictionary *toWebAPI(const WebExtensionTabParameters&);

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
