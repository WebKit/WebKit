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
#import "WebExtensionAPIWebNavigationEvent.h"

#import "WebExtensionContextMessages.h"
#import "WebPageProxy.h"
#import "WebProcess.h"
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/ScriptCallStack.h>
#import <JavaScriptCore/ScriptCallStackFactory.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

class JSWebExtensionWrappable;

void WebExtensionAPIWebNavigationEvent::invokeListenersWithArgument(id argument1, NSURL *targetURL)
{
    if (m_listeners.isEmpty())
        return;

    // FIXME: Honor the targetURL making sure it matches the filter for each listener.

    for (auto& listener : m_listeners)
        listener->call(argument1);
}

void WebExtensionAPIWebNavigationEvent::addListener(WebPage* page, RefPtr<WebExtensionCallbackHandler> listener, NSDictionary *filter, NSString **exceptionString)
{
    // FIXME: Save the filter associated with each of these listeners.

    m_listeners.append(listener);

    if (!page)
        return;

    WebProcess::singleton().send(Messages::WebExtensionContext::AddListener(page->webPageProxyIdentifier(), m_type));
}

void WebExtensionAPIWebNavigationEvent::removeListener(WebPage* page, RefPtr<WebExtensionCallbackHandler> listener)
{
    m_listeners.removeAllMatching([&](auto& entry) {
        return entry->callbackFunction() == listener->callbackFunction();
    });

    if (!page)
        return;

    WebProcess::singleton().send(Messages::WebExtensionContext::RemoveListener(page->webPageProxyIdentifier(), m_type));
}

bool WebExtensionAPIWebNavigationEvent::hasListener(RefPtr<WebExtensionCallbackHandler> listener)
{
    return m_listeners.containsIf([&](auto& entry) {
        return entry->callbackFunction() == listener->callbackFunction();
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
