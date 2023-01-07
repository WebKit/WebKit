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

#include "JSWebExtensionAPINamespace.h"
#include "WebExtensionAPIExtension.h"
#include "WebExtensionAPIObject.h"
#include "WebExtensionAPIPermissions.h"
#include "WebExtensionAPIRuntime.h"
#include "WebExtensionAPITest.h"
#include "WebExtensionAPIWebNavigation.h"

namespace WebKit {

class WebExtensionAPIExtension;
class WebExtensionAPIRuntime;

class WebExtensionAPINamespace : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPINamespace, namespace);

public:
#if PLATFORM(COCOA)
    bool isPropertyAllowed(String propertyName, WebPage*);

    WebExtensionAPIExtension& extension();
    WebExtensionAPIPermissions& permissions();
    WebExtensionAPIRuntime& runtime() final;
    WebExtensionAPITest& test();
    WebExtensionAPIWebNavigation& webNavigation();
#endif

private:
    RefPtr<WebExtensionAPIExtension> m_extension;
    RefPtr<WebExtensionAPIPermissions> m_permissions;
    RefPtr<WebExtensionAPIRuntime> m_runtime;
    RefPtr<WebExtensionAPITest> m_test;
    RefPtr<WebExtensionAPIWebNavigation> m_webNavigation;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
