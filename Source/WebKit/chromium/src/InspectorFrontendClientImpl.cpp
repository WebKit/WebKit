/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "InspectorFrontendClientImpl.h"

#include "Document.h"
#include "Frame.h"
#include "InspectorFrontendHost.h"
#include "Page.h"
#include "PlatformString.h"
#include "V8InspectorFrontendHost.h"
#include "V8Proxy.h"
#include "WebDevToolsFrontendClient.h"
#include "WebDevToolsFrontendImpl.h"
#include "platform/WebFloatPoint.h"
#include "platform/WebString.h"

using namespace WebCore;

namespace WebKit {

InspectorFrontendClientImpl::InspectorFrontendClientImpl(Page* frontendPage, WebDevToolsFrontendClient* client, WebDevToolsFrontendImpl* frontend)
    : m_frontendPage(frontendPage)
    , m_client(client)
    , m_frontend(frontend)
{
}

InspectorFrontendClientImpl::~InspectorFrontendClientImpl()
{
    if (m_frontendHost)
        m_frontendHost->disconnectClient();
    m_client = 0;
}

void InspectorFrontendClientImpl::windowObjectCleared()
{
    v8::HandleScope handleScope;
    v8::Handle<v8::Context> frameContext = V8Proxy::context(m_frontendPage->mainFrame());
    v8::Context::Scope contextScope(frameContext);

    ASSERT(!m_frontendHost);
    m_frontendHost = InspectorFrontendHost::create(this, m_frontendPage);
    v8::Handle<v8::Value> frontendHostObj = toV8(m_frontendHost.get());
    v8::Handle<v8::Object> global = frameContext->Global();

    global->Set(v8::String::New("InspectorFrontendHost"), frontendHostObj);
}

void InspectorFrontendClientImpl::frontendLoaded()
{
}

void InspectorFrontendClientImpl::moveWindowBy(float x, float y)
{
    m_client->moveWindowBy(WebFloatPoint(x, y));
}

String InspectorFrontendClientImpl::localizedStringsURL()
{
    return "";
}

String InspectorFrontendClientImpl::hiddenPanels()
{
    return "";
}

void InspectorFrontendClientImpl::bringToFront()
{
    m_client->activateWindow();
}

void InspectorFrontendClientImpl::closeWindow()
{
    m_client->closeWindow();
}

void InspectorFrontendClientImpl::requestAttachWindow()
{
    m_client->requestDockWindow();
}

void InspectorFrontendClientImpl::requestDetachWindow()
{
    m_client->requestUndockWindow();
}

void InspectorFrontendClientImpl::changeAttachedWindowHeight(unsigned)
{
    // Do nothing;
}

void InspectorFrontendClientImpl::saveAs(const String& fileName, const String& content)
{
    m_client->saveAs(fileName, content);
}

bool InspectorFrontendClientImpl::canSaveAs()
{
    return true;
}

void InspectorFrontendClientImpl::inspectedURLChanged(const String& url)
{
    m_frontendPage->mainFrame()->document()->setTitle("Developer Tools - " + url);
}

void InspectorFrontendClientImpl::sendMessageToBackend(const String& message)
{
    m_client->sendMessageToBackend(message);
}

} // namespace WebKit
