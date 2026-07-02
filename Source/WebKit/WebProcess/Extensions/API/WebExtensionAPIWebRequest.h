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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPIWebRequest.h"
#include "WebExtensionAPIObject.h"
#include "WebExtensionAPIWebRequestEvent.h"

namespace WebKit {

class WebPage;

class WebExtensionAPIWebRequest : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPIWebRequest, webRequest, webRequest);

public:
#if PLATFORM(COCOA)
    WebExtensionAPIWebRequestEvent& onBeforeRequest();
    WebExtensionAPIWebRequestEvent& onBeforeSendHeaders();
    WebExtensionAPIWebRequestEvent& onSendHeaders();
    WebExtensionAPIWebRequestEvent& onHeadersReceived();
    WebExtensionAPIWebRequestEvent& onAuthRequired();
    WebExtensionAPIWebRequestEvent& onBeforeRedirect();
    WebExtensionAPIWebRequestEvent& onResponseStarted();
    WebExtensionAPIWebRequestEvent& onCompleted();
    WebExtensionAPIWebRequestEvent& onErrorOccurred();

private:
    RefPtr<WebExtensionAPIWebRequestEvent> m_onBeforeRequestEvent;
    RefPtr<WebExtensionAPIWebRequestEvent> m_onBeforeSendHeadersEvent;
    RefPtr<WebExtensionAPIWebRequestEvent> m_onSendHeadersEvent;
    RefPtr<WebExtensionAPIWebRequestEvent> m_onHeadersReceivedEvent;
    RefPtr<WebExtensionAPIWebRequestEvent> m_onAuthRequiredEvent;
    RefPtr<WebExtensionAPIWebRequestEvent> m_onBeforeRedirectEvent;
    RefPtr<WebExtensionAPIWebRequestEvent> m_onResponseStartedEvent;
    RefPtr<WebExtensionAPIWebRequestEvent> m_onCompletedEvent;
    RefPtr<WebExtensionAPIWebRequestEvent> m_onErrorOccurredEvent;

#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_WEB_EXTENSION(WebExtensionAPIWebRequest, webRequest);

#endif // ENABLE(WK_WEB_EXTENSIONS)
