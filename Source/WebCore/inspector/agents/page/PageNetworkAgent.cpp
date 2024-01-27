/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "PageNetworkAgent.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "FrameDestructionObserverInlines.h"
#include "InspectorClient.h"
#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "ThreadableWebSocketChannel.h"
#include "WebSocket.h"

namespace WebCore {

using namespace Inspector;

PageNetworkAgent::PageNetworkAgent(PageAgentContext& context, InspectorClient* client)
    : InspectorNetworkAgent(context)
    , m_inspectedPage(context.inspectedPage)
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    , m_client(client)
#endif
{
#if !ENABLE(INSPECTOR_NETWORK_THROTTLING)
    UNUSED_PARAM(client);
#endif
}

PageNetworkAgent::~PageNetworkAgent() = default;

Protocol::Network::LoaderId PageNetworkAgent::loaderIdentifier(DocumentLoader* loader)
{
    if (loader) {
        if (auto* pageAgent = m_instrumentingAgents.enabledPageAgent())
            return pageAgent->loaderId(loader);
    }
    return { };
}

Protocol::Network::FrameId PageNetworkAgent::frameIdentifier(DocumentLoader* loader)
{
    if (loader) {
        if (auto* pageAgent = m_instrumentingAgents.enabledPageAgent())
            return pageAgent->frameId(loader->frame());
    }
    return { };
}

Vector<WebSocket*> PageNetworkAgent::activeWebSockets()
{
    Vector<WebSocket*> webSockets;

    for (auto* webSocket : WebSocket::allActiveWebSockets()) {
        auto channel = webSocket->channel();
        if (!channel)
            continue;

        if (!channel->hasCreatedHandshake())
            continue;

        RefPtr document = dynamicDowncast<Document>(webSocket->scriptExecutionContext());
        if (!document)
            continue;

        // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display iframe's WebSockets
        if (document->page() != &m_inspectedPage)
            continue;

        webSockets.append(webSocket);
    }

    return webSockets;
}

void PageNetworkAgent::setResourceCachingDisabledInternal(bool disabled)
{
    m_inspectedPage.setResourceCachingDisabledByWebInspector(disabled);
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

bool PageNetworkAgent::setEmulatedConditionsInternal(std::optional<int>&& bytesPerSecondLimit)
{
    return m_client && m_client->setEmulatedConditions(WTFMove(bytesPerSecondLimit));
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

ScriptExecutionContext* PageNetworkAgent::scriptExecutionContext(Protocol::ErrorString& errorString, const Protocol::Network::FrameId& frameId)
{
    auto* pageAgent = m_instrumentingAgents.enabledPageAgent();
    if (!pageAgent) {
        errorString = "Page domain must be enabled"_s;
        return nullptr;
    }

    auto* frame = pageAgent->assertFrame(errorString, frameId);
    if (!frame)
        return nullptr;

    auto* document = frame->document();
    if (!document) {
        errorString = "Missing frame of docuemnt for given frameId"_s;
        return nullptr;
    }

    return document;
}

void PageNetworkAgent::addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&& message)
{
    m_inspectedPage.console().addMessage(WTFMove(message));
}

} // namespace WebCore
