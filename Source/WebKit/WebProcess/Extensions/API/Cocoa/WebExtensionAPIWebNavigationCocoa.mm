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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPIWebNavigation.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

WebExtensionAPIWebNavigationEvent& WebExtensionAPIWebNavigation::onBeforeNavigate()
{
    if (!m_onBeforeNavigateEvent)
        m_onBeforeNavigateEvent = WebExtensionAPIWebNavigationEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebNavigationOnBeforeNavigate);
    return *m_onBeforeNavigateEvent;
}

WebExtensionAPIWebNavigationEvent& WebExtensionAPIWebNavigation::onCommitted()
{
    if (!m_onCommittedEvent)
        m_onCommittedEvent = WebExtensionAPIWebNavigationEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebNavigationOnCommitted);
    return *m_onCommittedEvent;
}

WebExtensionAPIWebNavigationEvent& WebExtensionAPIWebNavigation::onDOMContentLoaded()
{
    if (!m_onDOMContentLoadedEvent)
        m_onDOMContentLoadedEvent = WebExtensionAPIWebNavigationEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebNavigationOnDOMContentLoaded);
    return *m_onDOMContentLoadedEvent;
}

WebExtensionAPIWebNavigationEvent& WebExtensionAPIWebNavigation::onCompleted()
{
    if (!m_onCompletedEvent)
        m_onCompletedEvent = WebExtensionAPIWebNavigationEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebNavigationOnCompleted);
    return *m_onCompletedEvent;
}

WebExtensionAPIWebNavigationEvent& WebExtensionAPIWebNavigation::onErrorOccurred()
{
    if (!m_onErrorOccurredEvent)
        m_onErrorOccurredEvent = WebExtensionAPIWebNavigationEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WebNavigationOnErrorOccurred);
    return *m_onErrorOccurredEvent;
}

void WebExtensionAPIWebNavigation::getAllFrames(WebPage* webPage, NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **errorString)
{
    // FIXME: Implement this.
}

void WebExtensionAPIWebNavigation::getFrame(WebPage* webPage, NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **errorString)
{
    // FIXME: Implement this.
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
