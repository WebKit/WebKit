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
#import "WebExtensionAPIEvent.h"

#import "MessageSenderInlines.h"
#import "WebExtensionContextMessages.h"
#import "WebProcess.h"
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/ScriptCallStack.h>
#import <JavaScriptCore/ScriptCallStackFactory.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

class JSWebExtensionWrappable;

void WebExtensionAPIEvent::invokeListeners()
{
    if (m_listeners.isEmpty())
        return;

    for (auto& listener : m_listeners)
        listener->call();
}

void WebExtensionAPIEvent::invokeListenersWithArgument(id argument1)
{
    if (m_listeners.isEmpty())
        return;

    for (auto& listener : m_listeners)
        listener->call(argument1);
}

void WebExtensionAPIEvent::invokeListenersWithArgument(id argument1, id argument2)
{
    if (m_listeners.isEmpty())
        return;

    for (auto& listener : m_listeners)
        listener->call(argument1, argument2);
}

void WebExtensionAPIEvent::invokeListenersWithArgument(id argument1, id argument2, id argument3)
{
    if (m_listeners.isEmpty())
        return;

    for (auto& listener : m_listeners)
        listener->call(argument1, argument2, argument3);
}

void WebExtensionAPIEvent::addListener(WebPage* page, RefPtr<WebExtensionCallbackHandler> listener)
{
    ASSERT(page);

    m_pageProxyIdentifier = page->webPageProxyIdentifier();
    m_listeners.append(listener);

    WebProcess::singleton().send(Messages::WebExtensionContext::AddListener(page->webPageProxyIdentifier(), m_type, contentWorldType()), extensionContext().identifier());
}

void WebExtensionAPIEvent::removeListener(WebPage* page, RefPtr<WebExtensionCallbackHandler> listener)
{
    ASSERT(page);

    auto removedCount = m_listeners.removeAllMatching([&](auto& entry) {
        return entry->callbackFunction() == listener->callbackFunction();
    });

    if (!removedCount)
        return;

    ASSERT(page->webPageProxyIdentifier() == m_pageProxyIdentifier);

    WebProcess::singleton().send(Messages::WebExtensionContext::RemoveListener(m_pageProxyIdentifier, m_type, contentWorldType(), removedCount), extensionContext().identifier());
}

bool WebExtensionAPIEvent::hasListener(RefPtr<WebExtensionCallbackHandler> listener)
{
    return m_listeners.containsIf([&](auto& entry) {
        return entry->callbackFunction() == listener->callbackFunction();
    });
}

void WebExtensionAPIEvent::removeAllListeners()
{
    if (m_listeners.isEmpty())
        return;

    WebProcess::singleton().send(Messages::WebExtensionContext::RemoveListener(m_pageProxyIdentifier, m_type, contentWorldType(), m_listeners.size()), extensionContext().identifier());

    m_listeners.clear();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
