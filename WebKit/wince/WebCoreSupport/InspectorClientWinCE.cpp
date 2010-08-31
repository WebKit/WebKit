/*
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorClientWinCE.h"

#include "NotImplemented.h"

using namespace WebCore;

namespace WebKit {

InspectorClient::InspectorClient(WebView* webView)
    : m_inspectedWebView(webView)
{
}

InspectorClient::~InspectorClient()
{
}

void InspectorClient::inspectorDestroyed()
{
    delete this;
}

void InspectorClient::openInspectorFrontend(InspectorController* controller)
{
    notImplemented();
}

void InspectorClient::releaseFrontendPage()
{
    notImplemented();
}

void InspectorClient::highlight(Node* node)
{
    notImplemented();
}

void InspectorClient::hideHighlight()
{
    notImplemented();
}

void InspectorClient::populateSetting(const String& key, String* value)
{
    notImplemented();
}

void InspectorClient::storeSetting(const String& key, const String& value)
{
    notImplemented();
}

bool InspectorClient::sendMessageToFrontend(const String& message)
{
    notImplemented();
    return false;
}

} // namespace WebKit
