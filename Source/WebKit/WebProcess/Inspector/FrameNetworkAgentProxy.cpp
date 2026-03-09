/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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
#include "FrameNetworkAgentProxy.h"

#include "ProxyingNetworkAgentMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/CachedResource.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/InspectorResourceType.h>
#include <WebCore/InspectorResourceUtilities.h>
#include <WebCore/InstrumentingAgents.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/Page.h>
#include <WebCore/ResourceRequest.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WallTime.h>

namespace WebKit {

using namespace Inspector;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameNetworkAgentProxy);

FrameNetworkAgentProxy::FrameNetworkAgentProxy(WebAgentContext& context, WebPage& page)
    : NetworkAgentInstrumentation(context)
    , m_page(page)
{
}

FrameNetworkAgentProxy::~FrameNetworkAgentProxy()
{
    disable();
}

void FrameNetworkAgentProxy::didCreateFrontendAndBackend()
{
}

void FrameNetworkAgentProxy::willDestroyFrontendAndBackend(DisconnectReason)
{
    disable();
}

CommandResult<void> FrameNetworkAgentProxy::enable()
{
    m_enabled = true;

    Ref agents = m_instrumentingAgents.get();
    if (agents->enabledNetworkProxy() == this)
        return { };

    agents->setEnabledNetworkProxy(this);
    return { };
}

CommandResult<void> FrameNetworkAgentProxy::disable()
{
    m_enabled = false;

    Ref agents = m_instrumentingAgents.get();
    agents->setEnabledNetworkProxy(nullptr);
    return { };
}

static std::optional<ScriptExecutionContextIdentifier> contextIdentifier(DocumentLoader* loader)
{
    if (!loader || !loader->frame())
        return std::nullopt;
    auto* document = loader->frame()->document();
    if (!document)
        return std::nullopt;
    return document->identifier();
}

static std::optional<FrameIdentifier> frameIdentifier(DocumentLoader* loader)
{
    if (!loader || !loader->frame())
        return std::nullopt;
    return loader->frame()->frameID();
}

static ResourceType resourceTypeForRequest(const ResourceRequest& request, DocumentLoader* loader, const CachedResource* cachedResource)
{
    if (request.requester() == ResourceRequestRequester::XHR)
        return ResourceType::XHR;
    if (request.requester() == ResourceRequestRequester::Fetch)
        return ResourceType::Fetch;

    if (loader && equalIgnoringFragmentIdentifier(request.url(), loader->url()) && !loader->isCommitted())
        return ResourceType::Document;

    if (loader) {
        for (auto& linkIcon : loader->linkIcons()) {
            if (equalIgnoringFragmentIdentifier(request.url(), linkIcon.url))
                return ResourceType::Image;
        }
    }

    if (cachedResource)
        return ResourceUtilities::inspectorResourceType(*cachedResource);

    if (loader && loader->frame()) {
        if (auto* resource = ResourceUtilities::cachedResource(loader->frame(), request.url()))
            return ResourceUtilities::inspectorResourceType(*resource);
    }

    return ResourceType::Other;
}

void FrameNetworkAgentProxy::willSendRequest(ResourceLoaderIdentifier resourceID, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse, const CachedResource* cachedResource, ResourceLoader*)
{
    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    auto contextID = loader->frame()->document()->identifier();
    auto frameID = frameIdentifier(loader);
    auto resourceType = resourceTypeForRequest(request, loader, cachedResource);
    if (!frameID)
        return;

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();
    auto walltime = WallTime::now().secondsSinceEpoch().value();
    auto documentURL = loader->url().string();
    std::optional<ResourceResponse> optionalRedirectResponse;
    if (!redirectResponse.isNull())
        optionalRedirectResponse = redirectResponse;

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::RequestWillBeSent(
            resourceID, *frameID, contextID, String(), documentURL, request,
            WTF::move(optionalRedirectResponse), resourceType, timestamp, walltime),
        m_page->identifier());
}

void FrameNetworkAgentProxy::willSendRequestOfType(ResourceLoaderIdentifier resourceID, DocumentLoader* loader, ResourceRequest& request, Inspector::UncachedLoadType)
{
    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    auto contextID = contextIdentifier(loader);
    auto frameID = frameIdentifier(loader);
    if (!contextID || !frameID)
        return;

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();
    auto walltime = WallTime::now().secondsSinceEpoch().value();
    auto documentURL = loader->url().string();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::RequestWillBeSent(
            resourceID, *frameID, *contextID, String(), documentURL, request,
            std::nullopt, ResourceType::Other, timestamp, walltime),
        m_page->identifier());
}

void FrameNetworkAgentProxy::didReceiveResponse(ResourceLoaderIdentifier resourceID, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader*)
{
    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    auto contextID = loader->frame()->document()->identifier();
    auto frameID = frameIdentifier(loader);
    if (!frameID)
        return;

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();

    // FIXME: ResourceType is hardcoded to Other here because the actual type computed in
    // willSendRequest is not available at response time. Cache the type from willSendRequest
    // in a HashMap<ResourceLoaderIdentifier, ResourceType> and look it up here.
    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::ResponseReceived(
            resourceID, *frameID, contextID, response, ResourceType::Other, timestamp),
        m_page->identifier());
}

void FrameNetworkAgentProxy::didReceiveData(ResourceLoaderIdentifier resourceID, const SharedBuffer*, int dataLength, int encodedDataLength)
{
    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::DataReceived(resourceID, dataLength, encodedDataLength, timestamp),
        m_page->identifier());
}

void FrameNetworkAgentProxy::didFinishLoading(ResourceLoaderIdentifier resourceID, DocumentLoader* loader, const NetworkLoadMetrics&, ResourceLoader*)
{
    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::LoadingFinished(resourceID, timestamp, String()),
        m_page->identifier());
}

void FrameNetworkAgentProxy::didFailLoading(ResourceLoaderIdentifier resourceID, DocumentLoader* loader, const ResourceError& error)
{
    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::LoadingFailed(resourceID, timestamp, error.localizedDescription(), error.isCancellation()),
        m_page->identifier());
}

void FrameNetworkAgentProxy::didLoadResourceFromMemoryCache(DocumentLoader* loader, CachedResource& cachedResource)
{
    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    auto resourceID = ResourceLoaderIdentifier::generate();
    auto contextID = loader->frame()->document()->identifier();
    auto frameID = frameIdentifier(loader);
    auto resourceType = ResourceUtilities::inspectorResourceType(cachedResource);
    if (!frameID)
        return;

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();
    auto documentURL = loader->url().string();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::RequestServedFromMemoryCache(
            resourceID, *frameID, contextID, documentURL, cachedResource.resourceRequest(),
            cachedResource.response(), resourceType, timestamp),
        m_page->identifier());
}

void FrameNetworkAgentProxy::didReceiveScriptResponse(ResourceLoaderIdentifier)
{
}

void FrameNetworkAgentProxy::didReceiveThreadableLoaderResponse(ResourceLoaderIdentifier, DocumentThreadableLoader&)
{
}

void FrameNetworkAgentProxy::willDestroyCachedResource(CachedResource&)
{
}

void FrameNetworkAgentProxy::setInitialScriptContent(ResourceLoaderIdentifier, const String&)
{
}

void FrameNetworkAgentProxy::mainFrameNavigated(DocumentLoader&)
{
}

} // namespace WebKit
