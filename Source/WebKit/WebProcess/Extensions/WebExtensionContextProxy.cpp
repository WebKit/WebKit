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
#include "WebExtensionContextProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPINamespace.h"
#include "JSWebExtensionWrapper.h"
#include "WebExtensionContextMessages.h"
#include "WebExtensionContextProxyMessages.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

using namespace WebCore;

static HashMap<WebExtensionContextIdentifier, WeakPtr<WebExtensionContextProxy>>& webExtensionContextProxies()
{
    static MainThreadNeverDestroyed<HashMap<WebExtensionContextIdentifier, WeakPtr<WebExtensionContextProxy>>> contexts;
    return contexts;
}

RefPtr<WebExtensionContextProxy> WebExtensionContextProxy::get(WebExtensionContextIdentifier identifier)
{
    return webExtensionContextProxies().get(identifier).get();
}

Ref<WebExtensionContextProxy> WebExtensionContextProxy::getOrCreate(WebExtensionContextParameters parameters)
{
    auto updateProperties = [&](WebExtensionContextProxy& context) {
        context.m_baseURL = parameters.baseURL;
        context.m_uniqueIdentifier = parameters.uniqueIdentifier;
        context.m_manifest = parameters.manifest;
        context.m_manifestVersion = parameters.manifestVersion;
        context.m_testingMode = parameters.testingMode;
    };

    if (auto context = webExtensionContextProxies().get(parameters.identifier)) {
        updateProperties(*context);
        return *context;
    }

    auto result = adoptRef(new WebExtensionContextProxy(parameters));
    updateProperties(*result);
    return result.releaseNonNull();
}

WebExtensionContextProxy::WebExtensionContextProxy(WebExtensionContextParameters parameters)
    : m_identifier(parameters.identifier)
{
    ASSERT(!webExtensionContextProxies().contains(m_identifier));
    webExtensionContextProxies().add(m_identifier, this);

    WebProcess::singleton().addMessageReceiver(Messages::WebExtensionContextProxy::messageReceiverName(), m_identifier, *this);
}

WebExtensionContextProxy::~WebExtensionContextProxy()
{
    WebProcess::singleton().removeMessageReceiver(Messages::WebExtensionContextProxy::messageReceiverName(), m_identifier);
}

void WebExtensionContextProxy::addFrameWithExtensionContent(WebFrame& frame)
{
    m_extensionContentFrames.add(frame);
}

void WebExtensionContextProxy::enumerateNamespaceObjects(const Function<void(WebExtensionAPINamespace&)>& function, DOMWrapperWorld& world)
{
    for (auto& frame : m_extensionContentFrames) {
        auto* page = frame.page() ? frame.page()->corePage() : nullptr;
        if (!page)
            continue;

        auto context = page->isServiceWorkerPage() ? frame.jsContextForServiceWorkerWorld(world) : frame.jsContextForWorld(world);
        auto globalObject = JSContextGetGlobalObject(context);
        auto namespaceObject = JSObjectGetProperty(context, globalObject, toJSString("browser").get(), nullptr);
        if (!namespaceObject || !JSValueIsObject(context, namespaceObject))
            continue;

        auto* namespaceObjectImpl = toWebExtensionAPINamespace(context, namespaceObject);
        if (!namespaceObjectImpl)
            continue;

        function(*namespaceObjectImpl);
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
