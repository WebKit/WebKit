/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebPolicyClient.h"

#include "WKAPICast.h"
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

WebPolicyClient::WebPolicyClient()
{
    initialize(0);
}

void WebPolicyClient::initialize(const WKPagePolicyClient* client)
{
    if (client && !client->version)
        m_pagePolicyClient = *client;
    else 
        memset(&m_pagePolicyClient, 0, sizeof(m_pagePolicyClient));
}

bool WebPolicyClient::decidePolicyForNavigationAction(WebPageProxy* page, NavigationType type, WebEvent::Modifiers modifiers, WebMouseEvent::Button mouseButton, const String& url, WebFrameProxy* frame, WebFramePolicyListenerProxy* listener)
{
    if (!m_pagePolicyClient.decidePolicyForNavigationAction)
        return false;

    m_pagePolicyClient.decidePolicyForNavigationAction(toRef(page), toRef(type), toRef(modifiers), toRef(mouseButton), toURLRef(url.impl()), toRef(frame), toRef(listener), m_pagePolicyClient.clientInfo);
    return true;
}

bool WebPolicyClient::decidePolicyForNewWindowAction(WebPageProxy* page, NavigationType type, WebEvent::Modifiers modifiers, WebMouseEvent::Button mouseButton, const String& url, WebFrameProxy* frame, WebFramePolicyListenerProxy* listener)
{
    if (!m_pagePolicyClient.decidePolicyForNewWindowAction)
        return false;

    m_pagePolicyClient.decidePolicyForNewWindowAction(toRef(page), toRef(type), toRef(modifiers), toRef(mouseButton), toURLRef(url.impl()), toRef(frame), toRef(listener), m_pagePolicyClient.clientInfo);
    return true;
}

bool WebPolicyClient::decidePolicyForMIMEType(WebPageProxy* page, const String& MIMEType, const String& url, WebFrameProxy* frame, WebFramePolicyListenerProxy* listener)
{
    if (!m_pagePolicyClient.decidePolicyForMIMEType)
        return false;

    m_pagePolicyClient.decidePolicyForMIMEType(toRef(page), toRef(MIMEType.impl()), toURLRef(url.impl()), toRef(frame), toRef(listener), m_pagePolicyClient.clientInfo);
    return true;
}

} // namespace WebKit
