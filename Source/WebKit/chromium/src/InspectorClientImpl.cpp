/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorClientImpl.h"

#include "DOMWindow.h"
#include "FloatRect.h"
#include "NotImplemented.h"
#include "Page.h"
#include "WebDevToolsAgentImpl.h"
#include "WebRect.h"
#include "WebURL.h"
#include "WebURLRequest.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

InspectorClientImpl::InspectorClientImpl(WebViewImpl* webView)
    : m_inspectedWebView(webView)
{
    ASSERT(m_inspectedWebView);
}

InspectorClientImpl::~InspectorClientImpl()
{
}

void InspectorClientImpl::inspectorDestroyed()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->inspectorDestroyed();
}

void InspectorClientImpl::openInspectorFrontend(InspectorController* controller)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->openInspectorFrontend(controller);
}

void InspectorClientImpl::highlight(Node* node)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->highlight(node);
}

void InspectorClientImpl::hideHighlight()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->hideHighlight();
}

void InspectorClientImpl::populateSetting(const String& key, String* value)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->populateSetting(key, value);
}

void InspectorClientImpl::storeSetting(const String& key, const String& value)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->storeSetting(key, value);
}

bool InspectorClientImpl::sendMessageToFrontend(const WTF::String& message)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        return agent->sendMessageToFrontend(message);
    return false;
}

void InspectorClientImpl::updateInspectorStateCookie(const WTF::String& inspectorState)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->updateInspectorStateCookie(inspectorState);
}

WebDevToolsAgentImpl* InspectorClientImpl::devToolsAgent()
{
    return static_cast<WebDevToolsAgentImpl*>(m_inspectedWebView->devToolsAgent());
}

} // namespace WebKit
