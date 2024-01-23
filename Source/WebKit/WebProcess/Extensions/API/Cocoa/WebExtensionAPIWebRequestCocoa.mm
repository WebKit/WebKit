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
#import "WebExtensionAPIWebRequest.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onBeforeRequest()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onBeforeRequest

    if (!m_onBeforeRequestEvent)
        m_onBeforeRequestEvent = WebExtensionAPIWebRequestEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebRequestOnBeforeRequest);

    return *m_onBeforeRequestEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onBeforeSendHeaders()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onBeforeSendHeaders

    if (!m_onBeforeSendHeadersEvent)
        m_onBeforeSendHeadersEvent = WebExtensionAPIWebRequestEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebRequestOnBeforeSendHeaders);

    return *m_onBeforeSendHeadersEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onSendHeaders()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onSendHeaders

    if (!m_onSendHeadersEvent)
        m_onSendHeadersEvent = WebExtensionAPIWebRequestEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebRequestOnSendHeaders);

    return *m_onSendHeadersEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onHeadersReceived()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onHeadersReceived

    if (!m_onHeadersReceivedEvent)
        m_onHeadersReceivedEvent = WebExtensionAPIWebRequestEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebRequestOnHeadersReceived);

    return *m_onHeadersReceivedEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onAuthRequired()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onAuthRequired

    if (!m_onAuthRequiredEvent)
        m_onAuthRequiredEvent = WebExtensionAPIWebRequestEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebRequestOnAuthRequired);

    return *m_onAuthRequiredEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onBeforeRedirect()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onBeforeRedirect

    if (!m_onBeforeRedirectEvent)
        m_onBeforeRedirectEvent = WebExtensionAPIWebRequestEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebRequestOnBeforeRedirect);

    return *m_onBeforeRedirectEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onResponseStarted()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onResponseStarted

    if (!m_onResponseStartedEvent)
        m_onResponseStartedEvent = WebExtensionAPIWebRequestEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebRequestOnResponseStarted);

    return *m_onResponseStartedEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onCompleted()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onCompleted

    if (!m_onCompletedEvent)
        m_onCompletedEvent = WebExtensionAPIWebRequestEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebRequestOnCompleted);

    return *m_onCompletedEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onErrorOccurred()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onErrorOccurred

    if (!m_onErrorOccurredEvent)
        m_onErrorOccurredEvent = WebExtensionAPIWebRequestEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebRequestOnErrorOccurred);

    return *m_onErrorOccurredEvent;
}

void WebExtensionContextProxy::resourceLoadDidSendRequest(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceRequest&, const ResourceLoadInfo&)
{
    // FIXME: rdar://114823223 - Fire the necessary events.
}

void WebExtensionContextProxy::resourceLoadDidPerformHTTPRedirection(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceResponse&, const ResourceLoadInfo&, const WebCore::ResourceRequest& newRequest)
{
    // FIXME: rdar://114823223 - Fire the necessary events.
}

void WebExtensionContextProxy::resourceLoadDidReceiveChallenge(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::AuthenticationChallenge&, const ResourceLoadInfo&)
{
    // FIXME: rdar://114823223 - Fire the necessary events.
}

void WebExtensionContextProxy::resourceLoadDidReceiveResponse(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceResponse&, const ResourceLoadInfo&)
{
    // FIXME: rdar://114823223 - Fire the necessary events.
}

void WebExtensionContextProxy::resourceLoadDidCompleteWithError(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceResponse&, const WebCore::ResourceError&, const ResourceLoadInfo&)
{
    // FIXME: rdar://114823223 - Fire the necessary events.
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
