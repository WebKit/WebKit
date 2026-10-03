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
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include "WebSocketFrame.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"

namespace WebCore {

WebSocketChannelInspector::WebSocketChannelInspector(ScriptExecutionContext& context)
    : m_progressIdentifier(WebSocketChannelIdentifier::generate())
{
    if (is<WorkerGlobalScope>(context)) {
        if (CheckedPtr loaderProxy = dynamicDowncast<WorkerGlobalScope>(context)->thread()->workerLoaderProxy())
            m_target = loaderProxy->loaderContextIdentifier();
    } else {
        ASSERT(is<Document>(context));
        m_target = DocumentWeakPtr { downcast<Document>(context) };
    }
}

WebSocketChannelInspector::~WebSocketChannelInspector() = default;

void WebSocketChannelInspector::didCreateWebSocket(Document& document, WebSocketChannelIdentifier progressIdentifier, const URL& url)
{
    InspectorInstrumentation::didCreateWebSocket(&document, progressIdentifier, url);
}

void WebSocketChannelInspector::willSendWebSocketHandshakeRequest(Document& document, WebSocketChannelIdentifier progressIdentifier, ResourceRequest& request)
{
    InspectorInstrumentation::willSendWebSocketHandshakeRequest(&document, progressIdentifier, request);
}

void WebSocketChannelInspector::didSendWebSocketHandshakeRequest(const ResourceRequest& request) const
{
    FAST_RETURN_IF_NO_FRONTENDS(void());

    WTF::switchOn(m_target, [&](const DocumentWeakPtr& weakDocument) {
        if (RefPtr document = weakDocument.get())
            InspectorInstrumentation::didSendWebSocketHandshakeRequest(document.get(), m_progressIdentifier, request);
    }, [&](ScriptExecutionContextIdentifier identifier) {
        ScriptExecutionContext::postTaskTo(identifier, [progressIdentifier = m_progressIdentifier, request = request.isolatedCopy()](ScriptExecutionContext& context) {
            if (RefPtr document = dynamicDowncast<Document>(context))
                InspectorInstrumentation::didSendWebSocketHandshakeRequest(document.get(), progressIdentifier, request);
        });
    });
}

void WebSocketChannelInspector::didReceiveWebSocketHandshakeResponse(const ResourceResponse& response) const
{
    FAST_RETURN_IF_NO_FRONTENDS(void());

    WTF::switchOn(m_target, [&](const DocumentWeakPtr& weakDocument) {
        if (RefPtr document = weakDocument.get())
            InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(document.get(), m_progressIdentifier, response);
    }, [&](ScriptExecutionContextIdentifier identifier) {
        ScriptExecutionContext::postTaskTo(identifier, [progressIdentifier = m_progressIdentifier, responseData = response.crossThreadData()](ScriptExecutionContext& context) mutable {
            if (RefPtr document = dynamicDowncast<Document>(context)) {
                auto response = ResourceResponse::fromCrossThreadData(WTF::move(responseData));
                InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(document.get(), progressIdentifier, response);
            }
        });
    });
}

void WebSocketChannelInspector::didCloseWebSocket() const
{
    FAST_RETURN_IF_NO_FRONTENDS(void());

    WTF::switchOn(m_target, [&](const DocumentWeakPtr& weakDocument) {
        if (RefPtr document = weakDocument.get())
            InspectorInstrumentation::didCloseWebSocket(document.get(), m_progressIdentifier);
    }, [&](ScriptExecutionContextIdentifier identifier) {
        ScriptExecutionContext::postTaskTo(identifier, [progressIdentifier = m_progressIdentifier](ScriptExecutionContext& context) {
            if (RefPtr document = dynamicDowncast<Document>(context))
                InspectorInstrumentation::didCloseWebSocket(document.get(), progressIdentifier);
        });
    });
}

void WebSocketChannelInspector::didReceiveWebSocketFrame(const WebSocketFrame& frame) const
{
    FAST_RETURN_IF_NO_FRONTENDS(void());

    WTF::switchOn(m_target, [&](const DocumentWeakPtr& weakDocument) {
        if (RefPtr document = weakDocument.get())
            InspectorInstrumentation::didReceiveWebSocketFrame(document.get(), m_progressIdentifier, frame);
    }, [&](ScriptExecutionContextIdentifier identifier) {
        ScriptExecutionContext::postTaskTo(identifier, [progressIdentifier = m_progressIdentifier, frameCopy = frame, payload = Vector<uint8_t> { frame.payload }](ScriptExecutionContext& context) mutable {
            if (RefPtr document = dynamicDowncast<Document>(context)) {
                frameCopy.payload = payload.span();
                InspectorInstrumentation::didReceiveWebSocketFrame(document.get(), progressIdentifier, frameCopy);
            }
        });
    });
}

void WebSocketChannelInspector::didSendWebSocketFrame(const WebSocketFrame& frame) const
{
    FAST_RETURN_IF_NO_FRONTENDS(void());

    WTF::switchOn(m_target, [&](const DocumentWeakPtr& weakDocument) {
        if (RefPtr document = weakDocument.get())
            InspectorInstrumentation::didSendWebSocketFrame(document.get(), m_progressIdentifier, frame);
    }, [&](ScriptExecutionContextIdentifier identifier) {
        ScriptExecutionContext::postTaskTo(identifier, [progressIdentifier = m_progressIdentifier, frameCopy = frame, payload = Vector<uint8_t> { frame.payload }](ScriptExecutionContext& context) mutable {
            if (RefPtr document = dynamicDowncast<Document>(context)) {
                frameCopy.payload = payload.span();
                InspectorInstrumentation::didSendWebSocketFrame(document.get(), progressIdentifier, frameCopy);
            }
        });
    });
}

void WebSocketChannelInspector::didReceiveWebSocketFrameError(const String& errorMessage) const
{
    FAST_RETURN_IF_NO_FRONTENDS(void());

    WTF::switchOn(m_target, [&](const DocumentWeakPtr& weakDocument) {
        if (RefPtr document = weakDocument.get())
            InspectorInstrumentation::didReceiveWebSocketFrameError(document.get(), m_progressIdentifier, errorMessage);
    }, [&](ScriptExecutionContextIdentifier identifier) {
        ScriptExecutionContext::postTaskTo(identifier, [progressIdentifier = m_progressIdentifier, errorMessage = errorMessage.isolatedCopy()](ScriptExecutionContext& context) {
            if (RefPtr document = dynamicDowncast<Document>(context))
                InspectorInstrumentation::didReceiveWebSocketFrameError(document.get(), progressIdentifier, errorMessage);
        });
    });
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
