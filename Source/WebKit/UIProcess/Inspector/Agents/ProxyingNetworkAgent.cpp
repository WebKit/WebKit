/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2026 Apple Inc. All rights reserved.
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
#include "ProxyingNetworkAgent.h"

#include "HandleMessage.h"
#include "ProxyingNetworkAgentMessages.h"
#include "WebInspectorBackendMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <WebCore/InspectorIdentifierRegistry.h>
#include <WebCore/ProcessQualified.h>

namespace Inspector {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProxyingNetworkAgent);

static Protocol::Page::ResourceType toProtocolResourceType(ResourceType type)
{
    switch (type) {
    case ResourceType::Document:
        return Protocol::Page::ResourceType::Document;
    case ResourceType::StyleSheet:
        return Protocol::Page::ResourceType::StyleSheet;
    case ResourceType::Image:
        return Protocol::Page::ResourceType::Image;
    case ResourceType::Font:
        return Protocol::Page::ResourceType::Font;
    case ResourceType::Script:
        return Protocol::Page::ResourceType::Script;
    case ResourceType::XHR:
        return Protocol::Page::ResourceType::XHR;
    case ResourceType::Fetch:
        return Protocol::Page::ResourceType::Fetch;
    case ResourceType::Ping:
        return Protocol::Page::ResourceType::Ping;
    case ResourceType::Beacon:
        return Protocol::Page::ResourceType::Beacon;
    case ResourceType::WebSocket:
        return Protocol::Page::ResourceType::WebSocket;
#if ENABLE(APPLICATION_MANIFEST)
    case ResourceType::ApplicationManifest:
        return Protocol::Page::ResourceType::Other;
#endif
    case ResourceType::EventSource:
        return Protocol::Page::ResourceType::EventSource;
    case ResourceType::Other:
        return Protocol::Page::ResourceType::Other;
    }
    ASSERT_NOT_REACHED();
    return Protocol::Page::ResourceType::Other;
}

static Ref<Protocol::Network::Headers> buildObjectForHeaders(const HTTPHeaderMap& headers)
{
    auto headersValue = Protocol::Network::Headers::create().release();
    auto headersObject = headersValue->asObject();
    for (const auto& header : headers)
        headersObject->setString(header.key, header.value);
    return headersValue;
}

static Ref<Protocol::Network::Request> buildObjectForResourceRequest(const ResourceRequest& request)
{
    auto requestObject = Protocol::Network::Request::create()
        .setUrl(request.url().string())
        .setMethod(request.httpMethod())
        .setHeaders(buildObjectForHeaders(request.httpHeaderFields()))
        .release();

    if (RefPtr body = request.httpBody()) {
        if (!body->isEmpty()) {
            auto bytes = body->flatten();
            requestObject->setPostData(String::fromUTF8WithLatin1Fallback(bytes.span()));
        }
    }

    return requestObject;
}

static Protocol::Network::Response::Source toProtocolResponseSource(ResourceResponse::Source source)
{
    switch (source) {
    case ResourceResponse::Source::DOMCache:
    case ResourceResponse::Source::LegacyApplicationCachePlaceholder:
    case ResourceResponse::Source::Unknown:
        return Protocol::Network::Response::Source::Unknown;
    case ResourceResponse::Source::Network:
        return Protocol::Network::Response::Source::Network;
    case ResourceResponse::Source::MemoryCache:
    case ResourceResponse::Source::MemoryCacheAfterValidation:
        return Protocol::Network::Response::Source::MemoryCache;
    case ResourceResponse::Source::DiskCache:
    case ResourceResponse::Source::DiskCacheAfterValidation:
        return Protocol::Network::Response::Source::DiskCache;
    case ResourceResponse::Source::ServiceWorker:
        return Protocol::Network::Response::Source::ServiceWorker;
    case ResourceResponse::Source::InspectorOverride:
        return Protocol::Network::Response::Source::InspectorOverride;
    }
    ASSERT_NOT_REACHED();
    return Protocol::Network::Response::Source::Unknown;
}

static RefPtr<Protocol::Network::Response> buildObjectForResourceResponse(const ResourceResponse& response)
{
    if (response.isNull())
        return nullptr;

    return Protocol::Network::Response::create()
        .setUrl(response.url().string())
        .setStatus(response.httpStatusCode())
        .setStatusText(response.httpStatusText())
        .setHeaders(buildObjectForHeaders(response.httpHeaderFields()))
        .setMimeType(response.mimeType())
        .setSource(toProtocolResponseSource(response.source()))
        .release();
}

ProxyingNetworkAgent::ProxyingNetworkAgent(WebKit::WebPageAgentContext& context)
    : InspectorAgentBase("Network"_s, context)
    , m_frontendDispatcher(makeUniqueRef<NetworkFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(NetworkBackendDispatcher::create(context.backendDispatcher, this))
    , m_inspectedPage(context.inspectedPage)
{
}

ProxyingNetworkAgent::~ProxyingNetworkAgent()
{
    // Backstop in case Inspector teardown bypasses willDestroyFrontendAndBackend().
    removeAllRegisteredReceivers();
}

void ProxyingNetworkAgent::removeAllRegisteredReceivers()
{
    // Use the pinned WebProcessProxy refs rather than WebProcessProxy::processForIdentifier(),
    // which can return null when the process has been destructed but its receiver map entry
    // has not yet been torn down. The pin guarantees the process (and its receiver map) is
    // still alive here so removeMessageReceiver() can decrement m_messageReceiverMapCount.
    for (auto& [key, _] : std::exchange(m_instrumentedProcessPageCounts, { })) {
        auto [processID, pageID] = key;
        auto it = m_pinnedInstrumentedProcesses.find(processID);
        ASSERT(it != m_pinnedInstrumentedProcesses.end());
        if (it == m_pinnedInstrumentedProcesses.end())
            continue;
        Ref webProcess = it->value;
        webProcess->removeMessageReceiver(Messages::ProxyingNetworkAgent::messageReceiverName(), pageID);
    }
    m_pinnedInstrumentedProcesses.clear();
}

void ProxyingNetworkAgent::didCreateFrontendAndBackend()
{
    enable();
}

void ProxyingNetworkAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    disable();
}

void ProxyingNetworkAgent::enableInstrumentationForProcess(WebKit::WebProcessProxy& webProcess, WebCore::PageIdentifier pageID)
{
    auto key = std::make_pair(webProcess.coreProcessIdentifier(), pageID);
    auto result = m_instrumentedProcessPageCounts.add(key, 0);
    if (++result.iterator->value > 1)
        return;

    m_pinnedInstrumentedProcesses.ensure(webProcess.coreProcessIdentifier(), [&] {
        return Ref { webProcess };
    });
    webProcess.addMessageReceiver(Messages::ProxyingNetworkAgent::messageReceiverName(), pageID, *this);
    webProcess.send(Messages::WebInspectorBackend::EnableNetworkInstrumentation { }, pageID);
}

void ProxyingNetworkAgent::disableInstrumentationForProcess(WebKit::WebProcessProxy& webProcess, WebCore::PageIdentifier pageID)
{
    auto processID = webProcess.coreProcessIdentifier();
    auto key = std::make_pair(processID, pageID);
    auto it = m_instrumentedProcessPageCounts.find(key);
    if (it == m_instrumentedProcessPageCounts.end())
        return;

    if (--it->value > 0)
        return;

    m_instrumentedProcessPageCounts.remove(it);
    webProcess.send(Messages::WebInspectorBackend::DisableNetworkInstrumentation { }, pageID);
    webProcess.removeMessageReceiver(Messages::ProxyingNetworkAgent::messageReceiverName(), pageID);

    // Drop the pin once this process has no remaining page registrations.
    bool processStillHasRegistrations = false;
    for (auto& entry : m_instrumentedProcessPageCounts) {
        if (entry.key.first == processID) {
            processStillHasRegistrations = true;
            break;
        }
    }
    if (!processStillHasRegistrations)
        m_pinnedInstrumentedProcesses.remove(processID);
}

CommandResult<void> ProxyingNetworkAgent::enable()
{
    // FIXME: <https://webkit.org/b/308890> Only needed under Site Isolation; without it,
    // InspectorNetworkAgent in the single WebContent process handles network events.
    Ref inspectedPage = m_inspectedPage.get();
    Ref prefs = inspectedPage->preferences();
    if (!prefs->siteIsolationEnabled())
        return { };

    m_enabled = true;

    inspectedPage->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        Ref protectedWebProcess { webProcess };
        enableInstrumentationForProcess(protectedWebProcess, pageID);
    });

    return { };
}

CommandResult<void> ProxyingNetworkAgent::disable()
{
    if (!m_enabled)
        return { };

    m_enabled = false;

    // Force-teardown: disable all processes unconditionally, bypassing the
    // refcount discipline in disableInstrumentationForProcess(). This is
    // correct because disable() is called when the Network domain is torn
    // down entirely -- no per-frame refcount preservation is needed.
    //
    // Iterate the registration map, not forEachWebContentProcess(): under
    // Site Isolation a process may have swapped out while still holding our
    // message receiver, in which case forEachWebContentProcess() would no
    // longer enumerate it. The pinned process refs (see m_pinnedInstrumentedProcesses)
    // keep the WebProcessProxy alive long enough for the send + remove below.
    for (auto& [key, _] : m_instrumentedProcessPageCounts) {
        auto [processID, pageID] = key;
        auto it = m_pinnedInstrumentedProcesses.find(processID);
        ASSERT(it != m_pinnedInstrumentedProcesses.end());
        if (it == m_pinnedInstrumentedProcesses.end())
            continue;
        Ref webProcess = it->value;
        webProcess->send(Messages::WebInspectorBackend::DisableNetworkInstrumentation { }, pageID);
    }
    removeAllRegisteredReceivers();

    return { };
}

CommandResult<void> ProxyingNetworkAgent::setExtraHTTPHeaders(Ref<JSON::Object>&&)
{
    // FIXME: Forward to all WebContent processes.
    return { };
}

void ProxyingNetworkAgent::getResponseBody(const Protocol::Network::RequestId& requestId, Ref<GetResponseBodyCallback>&& callback)
{
    auto parsed = IdentifierRegistry::parseProtocolRequestId(requestId);
    if (!parsed) {
        callback->sendFailure("Invalid requestId format"_s);
        return;
    }

    auto [processIdentifier, resourceID] = *parsed;

    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage) {
        callback->sendFailure("Inspected page is gone"_s);
        return;
    }

    RefPtr<WebKit::WebProcessProxy> targetProcess;
    std::optional<PageIdentifier> targetPageID;

    inspectedPage->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (webProcess.coreProcessIdentifier() == processIdentifier) {
            targetProcess = &webProcess;
            targetPageID = pageID;
        }
    });

    if (!targetProcess || !targetPageID) {
        callback->sendFailure("WebProcess not found for requestId"_s);
        return;
    }

    targetProcess->sendWithAsyncReply(
        Messages::WebInspectorBackend::GetResponseBody { resourceID },
        [callback = WTF::move(callback)](String content, bool base64Encoded, String errorString) mutable {
            if (!errorString.isEmpty())
                callback->sendFailure(errorString);
            else
                callback->sendSuccess(content, base64Encoded);
        },
        *targetPageID);
}

CommandResult<void> ProxyingNetworkAgent::setResourceCachingDisabled(bool)
{
    // FIXME: Forward to all WebContent processes.
    return { };
}

CommandResult<void> ProxyingNetworkAgent::setClearResourceDataOnNavigate(bool)
{
    // FIXME: Forward to all WebContent processes.
    return { };
}

void ProxyingNetworkAgent::loadResource(const Protocol::Network::FrameId&, const String&, Ref<LoadResourceCallback>&& callback)
{
    // FIXME: Route to correct WebContent process for the frame.
    callback->sendFailure("Not yet implemented"_s);
}

CommandResult<String> ProxyingNetworkAgent::getSerializedCertificate(const Protocol::Network::RequestId&)
{
    // FIXME: Implement certificate retrieval (P2 -- BackendResourceDataStore).
    return makeUnexpected("Not yet implemented"_s);
}

CommandResult<Ref<Protocol::Runtime::RemoteObject>> ProxyingNetworkAgent::resolveWebSocket(const Protocol::Network::RequestId&, const String&)
{
    return makeUnexpected("Not yet implemented"_s);
}

CommandResult<void> ProxyingNetworkAgent::setInterceptionEnabled(bool)
{
    return { };
}

CommandResult<void> ProxyingNetworkAgent::addInterception(const String&, Protocol::Network::NetworkStage, std::optional<bool>&&, std::optional<bool>&&)
{
    return { };
}

CommandResult<void> ProxyingNetworkAgent::removeInterception(const String&, Protocol::Network::NetworkStage, std::optional<bool>&&, std::optional<bool>&&)
{
    return { };
}

CommandResult<void> ProxyingNetworkAgent::interceptContinue(const Protocol::Network::RequestId&, Protocol::Network::NetworkStage)
{
    return { };
}

CommandResult<void> ProxyingNetworkAgent::interceptWithRequest(const Protocol::Network::RequestId&, const String&, const String&, RefPtr<JSON::Object>&&, const String&)
{
    return { };
}

CommandResult<void> ProxyingNetworkAgent::interceptWithResponse(const Protocol::Network::RequestId&, const String&, bool, const String&, std::optional<int>&&, const String&, RefPtr<JSON::Object>&&)
{
    return { };
}

CommandResult<void> ProxyingNetworkAgent::interceptRequestWithResponse(const Protocol::Network::RequestId&, const String&, bool, const String&, int, const String&, Ref<JSON::Object>&&)
{
    return { };
}

CommandResult<void> ProxyingNetworkAgent::interceptRequestWithError(const Protocol::Network::RequestId&, Protocol::Network::ResourceErrorType)
{
    return { };
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

CommandResult<void> ProxyingNetworkAgent::setEmulatedConditions(std::optional<int>&&)
{
    return makeUnexpected("Not supported"_s);
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

// IPC message handlers from WebProcess FrameNetworkAgentProxy.

void ProxyingNetworkAgent::requestWillBeSent(ResourceID resourceID, FrameID frameID, ContextID contextID, const String& targetID, const String& documentURL, const ResourceRequest& request, std::optional<ResourceResponse>&& redirectResponse, ResourceType resourceType, double timestamp, double walltime)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    auto frameIdString = IdentifierRegistry::protocolFrameId(frameID, resourceID.processIdentifier());
    auto loaderId = IdentifierRegistry::protocolLoaderId(contextID);
    auto requestObject = buildObjectForResourceRequest(request);

    // FIXME: Build Initiator object once we have stack trace IPC.
    auto initiatorObject = Protocol::Network::Initiator::create()
        .setType(Protocol::Network::Initiator::Type::Other)
        .release();

    RefPtr<Protocol::Network::Response> redirectResponseObject;
    if (redirectResponse)
        redirectResponseObject = buildObjectForResourceResponse(*redirectResponse);

    m_frontendDispatcher->requestWillBeSent(requestId, frameIdString, loaderId, documentURL, WTF::move(requestObject), timestamp, walltime, WTF::move(initiatorObject), WTF::move(redirectResponseObject), toProtocolResourceType(resourceType), targetID);
}

void ProxyingNetworkAgent::responseReceived(ResourceID resourceID, FrameID frameID, ContextID contextID, const ResourceResponse& response, ResourceType resourceType, double timestamp)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    auto frameIdString = IdentifierRegistry::protocolFrameId(frameID, resourceID.processIdentifier());
    auto loaderId = IdentifierRegistry::protocolLoaderId(contextID);
    auto responseObject = buildObjectForResourceResponse(response);

    if (responseObject)
        m_frontendDispatcher->responseReceived(requestId, frameIdString, loaderId, timestamp, toProtocolResourceType(resourceType), responseObject.releaseNonNull());
}

void ProxyingNetworkAgent::dataReceived(ResourceID resourceID, int dataLength, int encodedDataLength, double timestamp)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    m_frontendDispatcher->dataReceived(requestId, timestamp, dataLength, encodedDataLength);
}

void ProxyingNetworkAgent::loadingFinished(ResourceID resourceID, double timestamp, const String& sourceMapURL)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    // FIXME: Add metrics parameter once we have NetworkLoadMetrics IPC.
    m_frontendDispatcher->loadingFinished(requestId, timestamp, sourceMapURL, nullptr);
}

void ProxyingNetworkAgent::loadingFailed(ResourceID resourceID, double timestamp, const String& errorText, bool canceled)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    m_frontendDispatcher->loadingFailed(requestId, timestamp, errorText, canceled);
}

void ProxyingNetworkAgent::requestServedFromMemoryCache(ResourceID resourceID, FrameID frameID, ContextID contextID, const String& documentURL, const ResourceRequest& request, const ResourceResponse& response, ResourceType resourceType, double timestamp)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    auto frameIdString = IdentifierRegistry::protocolFrameId(frameID, resourceID.processIdentifier());
    auto loaderId = IdentifierRegistry::protocolLoaderId(contextID);
    auto requestObject = buildObjectForResourceRequest(request);
    auto responseObject = buildObjectForResourceResponse(response);

    auto initiatorObject = Protocol::Network::Initiator::create()
        .setType(Protocol::Network::Initiator::Type::Other)
        .release();

    m_frontendDispatcher->requestWillBeSent(requestId, frameIdString, loaderId, documentURL, WTF::move(requestObject), timestamp, timestamp, WTF::move(initiatorObject), nullptr, toProtocolResourceType(resourceType), String());

    if (responseObject)
        m_frontendDispatcher->responseReceived(requestId, frameIdString, loaderId, timestamp, toProtocolResourceType(resourceType), responseObject.releaseNonNull());
}

} // namespace Inspector
