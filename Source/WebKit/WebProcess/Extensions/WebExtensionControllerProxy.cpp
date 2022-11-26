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

#include "config.h"
#include "WebExtensionControllerProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionContextProxy.h"
#include "WebExtensionControllerMessages.h"
#include "WebExtensionControllerProxyMessages.h"
#include "WebFrame.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

using namespace WebCore;

static HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionControllerProxy>>& webExtensionControllerProxies()
{
    static MainThreadNeverDestroyed<HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionControllerProxy>>> controllers;
    return controllers;
}

RefPtr<WebExtensionControllerProxy> WebExtensionControllerProxy::get(WebExtensionControllerIdentifier identifier)
{
    return webExtensionControllerProxies().get(identifier).get();
}

Ref<WebExtensionControllerProxy> WebExtensionControllerProxy::getOrCreate(WebExtensionControllerParameters parameters)
{
    auto updateProperties = [&](WebExtensionControllerProxy& controller) {
        WebExtensionContextProxySet contexts;
        WebExtensionContextProxyBaseURLMap baseURLMap;

        for (auto& contextParameters : parameters.contextParameters) {
            auto context = WebExtensionContextProxy::getOrCreate(contextParameters);
            baseURLMap.add(contextParameters.baseURL, context);
            contexts.add(context);
        }

        controller.m_extensionContexts.swap(contexts);
        controller.m_extensionContextBaseURLMap.swap(baseURLMap);
    };

    if (auto controller = webExtensionControllerProxies().get(parameters.identifier)) {
        updateProperties(*controller);
        return *controller;
    }

    auto result = adoptRef(new WebExtensionControllerProxy(parameters));
    updateProperties(*result);
    return result.releaseNonNull();
}

WebExtensionControllerProxy::WebExtensionControllerProxy(WebExtensionControllerParameters parameters)
    : m_identifier(parameters.identifier)
{
    ASSERT(!webExtensionControllerProxies().contains(m_identifier));
    webExtensionControllerProxies().add(m_identifier, this);

    WebProcess::singleton().addMessageReceiver(Messages::WebExtensionControllerProxy::messageReceiverName(), m_identifier, *this);
}

WebExtensionControllerProxy::~WebExtensionControllerProxy()
{
    WebProcess::singleton().removeMessageReceiver(Messages::WebExtensionControllerProxy::messageReceiverName(), m_identifier);
}

void WebExtensionControllerProxy::load(const WebExtensionContextParameters& contextParameters)
{
    auto context = WebExtensionContextProxy::getOrCreate(contextParameters);
    m_extensionContextBaseURLMap.add(contextParameters.baseURL, context);
    m_extensionContexts.add(context);
}

void WebExtensionControllerProxy::unload(WebExtensionContextIdentifier contextIdentifier)
{
    m_extensionContextBaseURLMap.removeIf([&](auto& entry) {
        return entry.value->identifier() == contextIdentifier;
    });

    m_extensionContexts.removeIf([&](auto& entry) {
        return entry->identifier() == contextIdentifier;
    });
}

RefPtr<WebExtensionContextProxy> WebExtensionControllerProxy::extensionContext(const String& uniqueIdentifier) const
{
    for (auto& extensionContext : m_extensionContexts) {
        if (extensionContext->uniqueIdentifier() == uniqueIdentifier)
            return extensionContext.ptr();
    }

    return nullptr;
}

RefPtr<WebExtensionContextProxy> WebExtensionControllerProxy::extensionContext(const URL& url) const
{
    return m_extensionContextBaseURLMap.get(url.truncatedForUseAsBase());
}

RefPtr<WebExtensionContextProxy> WebExtensionControllerProxy::extensionContext(WebFrame& frame, DOMWrapperWorld& world) const
{
    if (!world.isNormal()) {
        auto prefix = "WebExtension-"_s;
        if (!world.name().startsWith(prefix))
            return nullptr;

        auto prefixLength = prefix.length();
        auto uniqueIdentifier = world.name().substring(prefixLength);
        return extensionContext(uniqueIdentifier);
    }

    return extensionContext(frame.url());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
