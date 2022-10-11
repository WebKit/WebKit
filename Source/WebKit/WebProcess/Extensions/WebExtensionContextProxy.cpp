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

#include "WebExtensionContextMessages.h"
#include "WebExtensionContextProxyMessages.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

using namespace WebCore;

static HashMap<WebExtensionContextIdentifier, WebExtensionContextProxy*>& webExtensionContextProxies()
{
    static MainThreadNeverDestroyed<HashMap<WebExtensionContextIdentifier, WebExtensionContextProxy*>> contexts;
    return contexts;
}

Ref<WebExtensionContextProxy> WebExtensionContextProxy::getOrCreate(WebExtensionContextIdentifier identifier)
{
    auto& webExtensionContextProxyPtr = webExtensionContextProxies().add(identifier, nullptr).iterator->value;
    if (webExtensionContextProxyPtr)
        return *webExtensionContextProxyPtr;

    RefPtr<WebExtensionContextProxy> webExtensionContextProxy = adoptRef(new WebExtensionContextProxy(identifier));
    webExtensionContextProxyPtr = webExtensionContextProxy.get();

    return webExtensionContextProxy.releaseNonNull();
}

WebExtensionContextProxy::WebExtensionContextProxy(WebExtensionContextIdentifier identifier)
    : m_identifier(identifier)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebExtensionContextProxy::messageReceiverName(), m_identifier, *this);
}

WebExtensionContextProxy::~WebExtensionContextProxy()
{
    WebProcess::singleton().removeMessageReceiver(Messages::WebExtensionContextProxy::messageReceiverName(), m_identifier);

    ASSERT(webExtensionContextProxies().contains(m_identifier));
    webExtensionContextProxies().remove(m_identifier);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
