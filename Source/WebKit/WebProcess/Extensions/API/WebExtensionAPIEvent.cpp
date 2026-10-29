/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "WebExtensionAPIEvent.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "MessageSenderInlines.h"
#include "WebExtensionContextMessages.h"
#include "WebExtensionControllerProxy.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"

namespace WebKit {

class JSWebExtensionWrappable;

void WebExtensionAPIEvent::invokeListeners()
{
    if (m_listeners.isEmpty())
        return;

    // Copy the listeners since call() can trigger a mutation of the listeners.
    auto listenersCopy = m_listeners;

    for (RefPtr listener : listenersCopy)
        listener->call();
}

void WebExtensionAPIEvent::invokeListenersWithJSONArgument(const String& argument)
{
    if (m_listeners.isEmpty())
        return;

    // Copy the listeners since call() can trigger a mutation of the listeners.
    auto listenersCopy = m_listeners;

    for (RefPtr listener : listenersCopy)
        listener->call(!argument.isEmpty() ? JSValueMakeFromJSONString(listener->globalContext(), toJSString(argument).get()) : JSValueMakeUndefined(listener->globalContext()));
}

void WebExtensionAPIEvent::invokeListenersWithJSONArgument(const String& argument1, const String& argument2)
{
    if (m_listeners.isEmpty())
        return;

    // Copy the listeners since call() can trigger a mutation of the listeners.
    auto listenersCopy = m_listeners;

    for (RefPtr listener : listenersCopy) {
        listener->call(
            toJSValueRef(listener->globalContext(), argument1),
            !argument2.isEmpty() ? JSValueMakeFromJSONString(listener->globalContext(), toJSString(argument2).get()) : JSValueMakeUndefined(listener->globalContext())
        );
    }
}

void WebExtensionAPIEvent::invokeListenersWithJSArgument(JSListenerArgument argument1, JSListenerArgument argument2)
{
    if (m_listeners.isEmpty())
        return;

    // Copy the listeners since call() can trigger a mutation of the listeners.
    auto listenersCopy = m_listeners;

    auto argument = [](JSContextRef context, JSListenerArgument argumentValue) {
        if (argumentValue.convertToJSON)
            return !argumentValue.argument.isEmpty() ? JSValueMakeFromJSONString(context, toJSString(argumentValue.argument).get()) : JSValueMakeUndefined(context);
        return toJSValueRef(context, argumentValue.argument);
    };

    for (RefPtr listener : listenersCopy)
        listener->call(argument(listener->globalContext(), argument1), argument(listener->globalContext(), argument2));
}

void WebExtensionAPIEvent::addListener(WebCore::FrameIdentifier frameIdentifier, RefPtr<WebExtensionCallbackHandler> listener)
{
    m_frameIdentifier = frameIdentifier;
    m_listeners.append(listener);

    if (!hasExtensionContext()) {
        RefPtr webFrame = WebProcess::singleton().webFrame(m_frameIdentifier);
        RefPtr webPage = webFrame ? webFrame->page() : nullptr;
        RefPtr webExtensionControllerProxy = webPage ? webPage->webExtensionControllerProxy() : nullptr;

        if (webExtensionControllerProxy && webExtensionControllerProxy->inTestingMode()) {
            for (Ref extensionContext : webExtensionControllerProxy->extensionContexts()) {
                extensionContext->addFrameWithExtensionContent(*webFrame);
                WebProcess::singleton().send(Messages::WebExtensionContext::AddListener(*m_frameIdentifier, m_type, contentWorldType()), extensionContext->identifier());
            }
        }

        return;
    }

    WebProcess::singleton().send(Messages::WebExtensionContext::AddListener(*m_frameIdentifier, m_type, contentWorldType()), extensionContext().identifier());
}

void WebExtensionAPIEvent::removeListener(WebCore::FrameIdentifier frameIdentifier, RefPtr<WebExtensionCallbackHandler> listener)
{
    auto removedCount = m_listeners.removeAllMatching([&](auto& entry) {
        return entry->callbackFunction() == listener->callbackFunction();
    });

    if (!removedCount)
        return;

    ASSERT(frameIdentifier == m_frameIdentifier);

    if (!hasExtensionContext()) {
        RefPtr webFrame = WebProcess::singleton().webFrame(m_frameIdentifier);
        RefPtr webPage = webFrame ? webFrame->page() : nullptr;
        RefPtr webExtensionControllerProxy = webPage ? webPage->webExtensionControllerProxy() : nullptr;

        if (webExtensionControllerProxy && webExtensionControllerProxy->inTestingMode()) {
            for (Ref extensionContext : webExtensionControllerProxy->extensionContexts())
                WebProcess::singleton().send(Messages::WebExtensionContext::RemoveListener(*m_frameIdentifier, m_type, contentWorldType(), removedCount), extensionContext->identifier());
        }

        return;
    }

    WebProcess::singleton().send(Messages::WebExtensionContext::RemoveListener(*m_frameIdentifier, m_type, contentWorldType(), removedCount), extensionContext().identifier());
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

    auto removedCount = m_listeners.size();
    m_listeners.clear();

    if (!hasExtensionContext()) {
        RefPtr webFrame = WebProcess::singleton().webFrame(m_frameIdentifier);
        RefPtr webPage = webFrame ? webFrame->page() : nullptr;
        RefPtr webExtensionControllerProxy = webPage ? webPage->webExtensionControllerProxy() : nullptr;

        if (webExtensionControllerProxy && webExtensionControllerProxy->inTestingMode()) {
            for (Ref extensionContext : webExtensionControllerProxy->extensionContexts())
                WebProcess::singleton().send(Messages::WebExtensionContext::RemoveListener(*m_frameIdentifier, m_type, contentWorldType(), removedCount), extensionContext->identifier());
        }

        return;
    }

    WebProcess::singleton().send(Messages::WebExtensionContext::RemoveListener(*m_frameIdentifier, m_type, contentWorldType(), removedCount), extensionContext().identifier());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
