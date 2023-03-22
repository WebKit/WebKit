/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LegacyWebSocketInspectorInstrumentation.h"

#include "InspectorInstrumentation.h"

namespace WebCore {

bool LegacyWebSocketInspectorInstrumentation::hasFrontends()
{
    return InspectorInstrumentation::hasFrontends();
}

void LegacyWebSocketInspectorInstrumentation::didCreateWebSocket(Document* document, WebSocketChannelIdentifier identifier, const URL& requestURL)
{
    InspectorInstrumentation::didCreateWebSocket(document, identifier, requestURL);
}

void LegacyWebSocketInspectorInstrumentation::willSendWebSocketHandshakeRequest(Document* document, WebSocketChannelIdentifier identifier, const ResourceRequest& request)
{
    InspectorInstrumentation::willSendWebSocketHandshakeRequest(document, identifier, request);
}

void LegacyWebSocketInspectorInstrumentation::didReceiveWebSocketHandshakeResponse(Document* document, WebSocketChannelIdentifier identifier, const ResourceResponse& response)
{
    InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(document, identifier, response);
}

void LegacyWebSocketInspectorInstrumentation::didCloseWebSocket(Document* document, WebSocketChannelIdentifier identifier)
{
    InspectorInstrumentation::didCloseWebSocket(document, identifier);
}

void LegacyWebSocketInspectorInstrumentation::didReceiveWebSocketFrame(Document* document, WebSocketChannelIdentifier identifier, const WebSocketFrame& frame)
{
    InspectorInstrumentation::didReceiveWebSocketFrame(document, identifier, frame);
}

void LegacyWebSocketInspectorInstrumentation::didSendWebSocketFrame(Document* document, WebSocketChannelIdentifier identifier, const WebSocketFrame& frame)
{
    InspectorInstrumentation::didSendWebSocketFrame(document, identifier, frame);
}

void LegacyWebSocketInspectorInstrumentation::didReceiveWebSocketFrameError(Document* document, WebSocketChannelIdentifier identifier, const String& errorMessage)
{
    InspectorInstrumentation::didReceiveWebSocketFrameError(document, identifier, errorMessage);
}

}
