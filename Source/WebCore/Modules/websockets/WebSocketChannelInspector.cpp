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
#include "WebSocketFrame.h"

namespace WebCore {

WebSocketChannelInspector::WebSocketChannelInspector(Document& document)
    : m_document(document)
    , m_progressIdentifier(WebSocketChannelIdentifier::generate())
{
}

WebSocketChannelInspector::~WebSocketChannelInspector() = default;

void WebSocketChannelInspector::didCreateWebSocket(const URL& url) const
{
    if (!m_progressIdentifier || !m_document)
        return;

    InspectorInstrumentation::didCreateWebSocket(m_document.get(), m_progressIdentifier, url);
}

void WebSocketChannelInspector::willSendWebSocketHandshakeRequest(const ResourceRequest& request) const
{
    if (!m_progressIdentifier || !m_document)
        return;

    InspectorInstrumentation::willSendWebSocketHandshakeRequest(m_document.get(), m_progressIdentifier, request);
}

void WebSocketChannelInspector::didReceiveWebSocketHandshakeResponse(const ResourceResponse& response) const
{
    if (!m_progressIdentifier || !m_document)
        return;

    InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(m_document.get(), m_progressIdentifier, response);
}

void WebSocketChannelInspector::didCloseWebSocket() const
{
    if (!m_progressIdentifier || !m_document)
        return;

    InspectorInstrumentation::didCloseWebSocket(m_document.get(), m_progressIdentifier);
}

void WebSocketChannelInspector::didReceiveWebSocketFrame(const WebSocketFrame& frame) const
{
    if (!m_progressIdentifier || !m_document)
        return;

    InspectorInstrumentation::didReceiveWebSocketFrame(m_document.get(), m_progressIdentifier, frame);
}

void WebSocketChannelInspector::didSendWebSocketFrame(const WebSocketFrame& frame) const
{
    if (!m_progressIdentifier || !m_document)
        return;

    InspectorInstrumentation::didSendWebSocketFrame(m_document.get(), m_progressIdentifier, frame);
}

void WebSocketChannelInspector::didReceiveWebSocketFrameError(const String& errorMessage) const
{
    if (!m_progressIdentifier || !m_document)
        return;

    InspectorInstrumentation::didReceiveWebSocketFrameError(m_document.get(), m_progressIdentifier, errorMessage);
}

WebSocketChannelIdentifier WebSocketChannelInspector::progressIdentifier() const
{
    return m_progressIdentifier;
}

WebSocketFrame WebSocketChannelInspector::createFrame(std::span<const uint8_t> data, WebSocketFrame::OpCode opCode)
{
    // This is an approximation since frames can be merged on a single message.
    WebSocketFrame frame;
    frame.opCode = opCode;
    frame.masked = false;
    frame.payload = data;

    // WebInspector does not use them.
    frame.final = false;
    frame.compress = false;
    frame.reserved2 = false;
    frame.reserved3 = false;

    return frame;
}

} // namespace WebCore
