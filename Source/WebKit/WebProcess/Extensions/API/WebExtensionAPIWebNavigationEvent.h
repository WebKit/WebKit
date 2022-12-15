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

#include "JSWebExtensionAPIWebNavigationEvent.h"
#include "JSWebExtensionWrappable.h"
#include "WebExtensionAPIObject.h"
#include "WebExtensionEventListenerType.h"
#include "WebPage.h"

OBJC_CLASS JSValue;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSString;
OBJC_CLASS NSURL;
OBJC_CLASS _WKWebExtensionWebNavigationURLFilter;

namespace WebKit {

class WebExtensionAPIWebNavigationEvent : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPIWebNavigationEvent, webNavigationEvent);

public:
    using FilterAndCallbackPair = std::pair<RefPtr<WebExtensionCallbackHandler>, RetainPtr<_WKWebExtensionWebNavigationURLFilter>>;
    using ListenerVector = Vector<FilterAndCallbackPair>;

    void invokeListenersWithArgument(id argument, NSURL *targetURL);

    const ListenerVector& listeners() const { return m_listeners; }

    void addListener(WebPage*, RefPtr<WebExtensionCallbackHandler>, NSDictionary *filter, NSString **exceptionString);
    void removeListener(WebPage*, RefPtr<WebExtensionCallbackHandler>);
    bool hasListener(RefPtr<WebExtensionCallbackHandler>);

private:
    explicit WebExtensionAPIWebNavigationEvent(ForMainWorld forMainWorld, WebExtensionAPIRuntimeBase& runtime, WebExtensionContextProxy& context, WebExtensionEventListenerType type)
        : WebExtensionAPIObject(forMainWorld, runtime, context)
    {
        m_type = type;
    }

    WebExtensionEventListenerType m_type;
    ListenerVector m_listeners;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
