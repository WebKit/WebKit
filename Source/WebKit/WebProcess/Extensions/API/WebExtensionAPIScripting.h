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

#include "JSWebExtensionAPIScripting.h"
#include "WebExtensionAPIObject.h"

OBJC_CLASS NSDictionary;
OBJC_CLASS NSObject;
OBJC_CLASS NSString;

namespace WebKit {

using FirstTimeRegistration = WebExtensionDynamicScripts::WebExtensionRegisteredScript::FirstTimeRegistration;

class WebExtension;

class WebExtensionAPIScripting : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPIScripting, scripting);

public:
#if PLATFORM(COCOA)
    void executeScript(NSDictionary *, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void insertCSS(NSDictionary *, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void removeCSS(NSDictionary *, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void registerContentScripts(NSObject *, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getRegisteredContentScripts(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void updateContentScripts(NSObject *, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void unregisterContentScripts(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

private:
    friend class WebExtensionContext;

    bool hasValidExecutionWorld(NSDictionary *script, NSString **outExceptionString);

    bool validateScript(NSDictionary *, NSString **outExceptionString);
    bool validateTarget(NSDictionary *, NSString **outExceptionString);
    bool validateCSS(NSDictionary *, NSString **outExceptionString);

    bool validateRegisteredScripts(NSArray *, FirstTimeRegistration, NSString **outExceptionString);
    bool validateFilter(NSDictionary *filter, NSString **outExceptionString);

    void parseCSSInjectionOptions(NSDictionary *, WebExtensionScriptInjectionParameters&);
    void parseTargetInjectionOptions(NSDictionary *, WebExtensionScriptInjectionParameters&, NSString **outExceptionString);
    void parseScriptInjectionOptions(NSDictionary *, WebExtensionScriptInjectionParameters&, NSString **outExceptionString);
    static void parseRegisteredContentScripts(NSArray *, FirstTimeRegistration, Vector<WebExtensionRegisteredScriptParameters>&);

#endif
};

NSArray *toWebAPI(const Vector<WebExtensionScriptInjectionResultParameters>&, bool returnExecutionResultOnly);
NSArray *toWebAPI(const Vector<WebExtensionRegisteredScriptParameters>&);
NSString *toWebAPI(WebExtension::InjectionTime);

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
