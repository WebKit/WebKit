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
#include "WebExtensionAPIObject.h"

OBJC_CLASS NSDictionary;
OBJC_CLASS NSString;
OBJC_CLASS NSURL;

namespace WebKit {

class WebExtensionAPIRuntime;

class WebExtensionAPIRuntimeBase : public JSWebExtensionWrappable {
public:
    void reportErrorForCallbackHandler(WebExtensionCallbackHandler&, NSString *error, JSGlobalContextRef);

private:
    friend class WebExtensionAPIRuntime;

    bool m_lastErrorAccessed = false;

#if PLATFORM(COCOA)
    RetainPtr<JSValue> m_lastError;
#endif
};

class WebExtensionAPIRuntime : public WebExtensionAPIObject, public WebExtensionAPIRuntimeBase {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPIRuntime, runtime);

public:
    WebExtensionAPIRuntime& runtime() final { return *this; }

#if PLATFORM(COCOA)
    NSURL *getURL(NSString *resourcePath, NSString **errorString);

    NSDictionary *getManifest();

    NSString *runtimeIdentifier();

    void getPlatformInfo(Ref<WebExtensionCallbackHandler>&&);

    JSValueRef lastError();
#endif
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
