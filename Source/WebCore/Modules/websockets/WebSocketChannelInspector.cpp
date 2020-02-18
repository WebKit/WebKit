/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WebSocketChannelInspector.h"

#include "Document.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "ProgressTracker.h"

namespace WebCore {

WebSocketChannelInspector::WebSocketChannelInspector(Document& document)
{
    if (auto* page = document.page())
        m_progressIdentifier = page->progress().createUniqueIdentifier();
}

void WebSocketChannelInspector::didCreateWebSocket(Document* document, const URL& url)
{
    if (!m_progressIdentifier || !document)
        return;

    InspectorInstrumentation::didCreateWebSocket(document, m_progressIdentifier, url);
}

void WebSocketChannelInspector::willSendWebSocketHandshakeRequest(Document* document, const ResourceRequest& request)
{
    if (!m_progressIdentifier || !document)
        return;

    InspectorInstrumentation::willSendWebSocketHandshakeRequest(document, m_progressIdentifier, request);
}

void WebSocketChannelInspector::didReceiveWebSocketHandshakeResponse(Document* document, const ResourceResponse& response)
{
    if (!m_progressIdentifier || !document)
        return;

    InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(document, m_progressIdentifier, response);
}

void WebSocketChannelInspector::didCloseWebSocket(Document* document)
{
    if (!m_progressIdentifier || !document)
        return;

    InspectorInstrumentation::didCloseWebSocket(document, m_progressIdentifier);
}

void WebSocketChannelInspector::didReceiveWebSocketFrame(Document* document, const WebSocketFrame& frame)
{
    if (!m_progressIdentifier || !document)
        return;

    InspectorInstrumentation::didReceiveWebSocketFrame(document, m_progressIdentifier, frame);
}

void WebSocketChannelInspector::didSendWebSocketFrame(Document* document, const WebSocketFrame& frame)
{
    if (!m_progressIdentifier || !document)
        return;

    InspectorInstrumentation::didSendWebSocketFrame(document, m_progressIdentifier, frame);
}

void WebSocketChannelInspector::didReceiveWebSocketFrameError(Document* document, const String& errorMessage)
{
    if (!m_progressIdentifier || !document)
        return;

    InspectorInstrumentation::didReceiveWebSocketFrameError(document, m_progressIdentifier, errorMessage);
}

} // namespace WebCore
