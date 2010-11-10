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

bool WebPolicyClient::decidePolicyForNavigationAction(WebPageProxy* page, NavigationType type, WebEvent::Modifiers modifiers, WebMouseEvent::Button mouseButton, const String& url, WebFrameProxy* frame, WebFramePolicyListenerProxy* listener)
{
    if (!m_client.decidePolicyForNavigationAction)
        return false;

    m_client.decidePolicyForNavigationAction(toAPI(page), toAPI(type), toAPI(modifiers), toAPI(mouseButton), toURLRef(url.impl()), toAPI(frame), toAPI(listener), m_client.clientInfo);
    return true;
}

bool WebPolicyClient::decidePolicyForNewWindowAction(WebPageProxy* page, NavigationType type, WebEvent::Modifiers modifiers, WebMouseEvent::Button mouseButton, const String& url, WebFrameProxy* frame, WebFramePolicyListenerProxy* listener)
{
    if (!m_client.decidePolicyForNewWindowAction)
        return false;

    m_client.decidePolicyForNewWindowAction(toAPI(page), toAPI(type), toAPI(modifiers), toAPI(mouseButton), toURLRef(url.impl()), toAPI(frame), toAPI(listener), m_client.clientInfo);
    return true;
}

bool WebPolicyClient::decidePolicyForMIMEType(WebPageProxy* page, const String& MIMEType, const String& url, WebFrameProxy* frame, WebFramePolicyListenerProxy* listener)
{
    if (!m_client.decidePolicyForMIMEType)
        return false;

    m_client.decidePolicyForMIMEType(toAPI(page), toAPI(MIMEType.impl()), toURLRef(url.impl()), toAPI(frame), toAPI(listener), m_client.clientInfo);
    return true;
}

} // namespace WebKit
