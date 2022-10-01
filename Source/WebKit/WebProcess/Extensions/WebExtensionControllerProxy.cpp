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

#include "WebExtensionControllerMessages.h"
#include "WebExtensionControllerProxyMessages.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

using namespace WebCore;

static HashMap<WebExtensionControllerIdentifier, WebExtensionControllerProxy*>& webExtensionControllerProxies()
{
    static NeverDestroyed<HashMap<WebExtensionControllerIdentifier, WebExtensionControllerProxy*>> controllers;
    return controllers;
}

Ref<WebExtensionControllerProxy> WebExtensionControllerProxy::getOrCreate(WebExtensionControllerIdentifier identifier)
{
    auto& webExtensionControllerProxyPtr = webExtensionControllerProxies().add(identifier, nullptr).iterator->value;
    if (webExtensionControllerProxyPtr)
        return *webExtensionControllerProxyPtr;

    RefPtr<WebExtensionControllerProxy> webExtensionControllerProxy = adoptRef(new WebExtensionControllerProxy(identifier));
    webExtensionControllerProxyPtr = webExtensionControllerProxy.get();

    return webExtensionControllerProxy.releaseNonNull();
}

WebExtensionControllerProxy::WebExtensionControllerProxy(WebExtensionControllerIdentifier identifier)
    : m_identifier(identifier)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebExtensionControllerProxy::messageReceiverName(), m_identifier, *this);
}

WebExtensionControllerProxy::~WebExtensionControllerProxy()
{
    WebProcess::singleton().removeMessageReceiver(Messages::WebExtensionControllerProxy::messageReceiverName(), m_identifier);

    ASSERT(webExtensionControllerProxies().contains(m_identifier));
    webExtensionControllerProxies().remove(m_identifier);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
