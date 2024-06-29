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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPIRuntime.h"
#include "JSWebExtensionAPIWebPageRuntime.h"
#include "WebExtensionAPIEvent.h"
#include "WebExtensionAPIObject.h"

OBJC_CLASS NSDictionary;
OBJC_CLASS NSString;
OBJC_CLASS NSURL;

namespace WebKit {

class WebExtensionAPIPort;
class WebExtensionAPIRuntime;

class WebExtensionAPIRuntimeBase : public JSWebExtensionWrappable {
public:
    JSValue *reportError(NSString *errorMessage, JSGlobalContextRef, Function<void()>&& = nullptr);
    JSValue *reportError(NSString *errorMessage, WebExtensionCallbackHandler&);

private:
    friend class WebExtensionAPIRuntime;

    bool m_lastErrorAccessed = false;

#if PLATFORM(COCOA)
    RetainPtr<JSValue> m_lastError;
#endif
};

class WebExtensionAPIRuntime : public WebExtensionAPIObject, public WebExtensionAPIRuntimeBase {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPIRuntime, runtime, runtime);

public:
    WebExtensionAPIRuntime& runtime() const final { return const_cast<WebExtensionAPIRuntime&>(*this); }

#if PLATFORM(COCOA)
    bool isPropertyAllowed(const ASCIILiteral& propertyName, WebPage*);

    NSURL *getURL(NSString *resourcePath, NSString **outExceptionString);
    NSDictionary *getManifest();
    void getPlatformInfo(Ref<WebExtensionCallbackHandler>&&);
    void getBackgroundPage(Ref<WebExtensionCallbackHandler>&&);
    double getFrameId(JSValue *);

    void setUninstallURL(URL, Ref<WebExtensionCallbackHandler>&&);

    void openOptionsPage(Ref<WebExtensionCallbackHandler>&&);
    void reload();

    NSString *runtimeIdentifier();

    JSValue *lastError();

    void sendMessage(WebFrame&, NSString *extensionID, NSString *messageJSON, NSDictionary *options, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    RefPtr<WebExtensionAPIPort> connect(WebFrame&, JSContextRef, NSString *extensionID, NSDictionary *options, NSString **outExceptionString);

    void sendNativeMessage(WebFrame&, NSString *applicationID, NSString *messageJSON, Ref<WebExtensionCallbackHandler>&&);
    RefPtr<WebExtensionAPIPort> connectNative(WebFrame&, JSContextRef, NSString *applicationID);

    WebExtensionAPIEvent& onConnect();
    WebExtensionAPIEvent& onInstalled();
    WebExtensionAPIEvent& onMessage();
    WebExtensionAPIEvent& onStartup();
    WebExtensionAPIEvent& onConnectExternal();
    WebExtensionAPIEvent& onMessageExternal();

private:
    friend class WebExtensionAPIWebPageRuntime;

    static bool parseConnectOptions(NSDictionary *, std::optional<String>& name, NSString *sourceKey, NSString **outExceptionString);

    RefPtr<WebExtensionAPIEvent> m_onConnect;
    RefPtr<WebExtensionAPIEvent> m_onInstalled;
    RefPtr<WebExtensionAPIEvent> m_onMessage;
    RefPtr<WebExtensionAPIEvent> m_onStartup;
    RefPtr<WebExtensionAPIEvent> m_onConnectExternal;
    RefPtr<WebExtensionAPIEvent> m_onMessageExternal;
#endif
};

class WebExtensionAPIWebPageRuntime : public WebExtensionAPIObject, public WebExtensionAPIRuntimeBase {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPIWebPageRuntime, webPageRuntime, webPageRuntime);

public:
    WebExtensionAPIWebPageRuntime& runtime() const final { return const_cast<WebExtensionAPIWebPageRuntime&>(*this); }

#if PLATFORM(COCOA)
    void sendMessage(WebFrame&, NSString *extensionID, NSString *messageJSON, NSDictionary *options, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    RefPtr<WebExtensionAPIPort> connect(WebFrame&, JSContextRef, NSString *extensionID, NSDictionary *options, NSString **outExceptionString);
#endif
};

NSDictionary *toWebAPI(const WebExtensionMessageSenderParameters&);

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
