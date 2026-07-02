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
#include "FrameNetworkAgentProxy.h"

#include "ProxyingNetworkAgentMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/CachedResource.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/InspectorResourceType.h>
#include <WebCore/InspectorResourceUtilities.h>
#include <WebCore/InstrumentingAgents.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/Page.h>
#include <WebCore/ProcessQualified.h>
#include <WebCore/ResourceRequest.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WallTime.h>

namespace WebKit {

using namespace Inspector;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameNetworkAgentProxy);

static ScopedResourceLoaderIdentifier qualifyResourceID(ResourceLoaderIdentifier resourceID)
{
    return { resourceID, Process::identifier() };
}

FrameNetworkAgentProxy::FrameNetworkAgentProxy(WebAgentContext& context, WebPage& page, BackendResourceDataStore& store)
    : NetworkAgentInstrumentation(context)
    , m_page(page)
    , m_resourcesData(store)
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
    if (!m_enabled)
        return { };

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
    RefPtr protectedLoader = loader;

    if (request.requester() == ResourceRequestRequester::XHR)
        return ResourceType::XHR;
    if (request.requester() == ResourceRequestRequester::Fetch)
        return ResourceType::Fetch;

    if (protectedLoader && equalIgnoringFragmentIdentifier(request.url(), protectedLoader->url()) && !protectedLoader->isCommitted())
        return ResourceType::Document;

    if (protectedLoader) {
        for (auto& linkIcon : protectedLoader->linkIcons()) {
            if (equalIgnoringFragmentIdentifier(request.url(), linkIcon.url))
                return ResourceType::Image;
        }
    }

    if (cachedResource)
        return ResourceUtilities::inspectorResourceType(*cachedResource);

    if (RefPtr frame = protectedLoader ? protectedLoader->frame() : nullptr) {
        if (RefPtr resource = ResourceUtilities::cachedResource(frame.get(), request.url()))
            return ResourceUtilities::inspectorResourceType(*resource);
    }

    return ResourceType::Other;
}

void FrameNetworkAgentProxy::willSendRequest(ResourceLoaderIdentifier resourceID, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse, const CachedResource* cachedResource, ResourceLoader*)
{
    if (request.hiddenFromInspector())
        return;

    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    RefPtr protectedLoader = loader;
    RefPtr page = m_page.get();
    if (!page)
        return;

    auto contextID = protectedLoader->frame()->document()->identifier();
    auto frameID = frameIdentifier(protectedLoader.get());
    auto resourceType = resourceTypeForRequest(request, protectedLoader.get(), cachedResource);
    if (!frameID)
        return;

    m_resourcesData->resourceCreated(resourceID, resourceType);

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();
    auto walltime = WallTime::now().secondsSinceEpoch().value();
    auto documentURL = protectedLoader->url().string();
    std::optional<ResourceResponse> optionalRedirectResponse;
    if (!redirectResponse.isNull())
        optionalRedirectResponse = redirectResponse;

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::RequestWillBeSent(
            qualifyResourceID(resourceID), *frameID, contextID, String(), documentURL, request,
            WTF::move(optionalRedirectResponse), resourceType, timestamp, walltime),
        page->identifier());
}

void FrameNetworkAgentProxy::willSendRequestOfType(ResourceLoaderIdentifier resourceID, DocumentLoader* loader, ResourceRequest& request, Inspector::UncachedLoadType)
{
    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    RefPtr protectedLoader = loader;
    RefPtr page = m_page.get();
    if (!page)
        return;

    auto contextID = contextIdentifier(protectedLoader.get());
    auto frameID = frameIdentifier(protectedLoader.get());
    if (!contextID || !frameID)
        return;

    // FIXME: Map from UncachedLoadType to a more specific ResourceType.
    // https://webkit.org/b/312828
    m_resourcesData->resourceCreated(resourceID, ResourceType::Other);

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();
    auto walltime = WallTime::now().secondsSinceEpoch().value();
    auto documentURL = protectedLoader->url().string();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::RequestWillBeSent(
            qualifyResourceID(resourceID), *frameID, *contextID, String(), documentURL, request,
            std::nullopt, ResourceType::Other, timestamp, walltime),
        page->identifier());
}

void FrameNetworkAgentProxy::didReceiveResponse(ResourceLoaderIdentifier resourceID, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader*)
{
    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    RefPtr protectedLoader = loader;
    RefPtr page = m_page.get();
    if (!page)
        return;

    auto contextID = protectedLoader->frame()->document()->identifier();
    auto frameID = frameIdentifier(protectedLoader.get());
    if (!frameID)
        return;

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();

    // Look up the ResourceType cached from willSendRequest, falling back to Other.
    auto* resourceData = m_resourcesData->data(resourceID);
    auto resourceType = resourceData ? resourceData->type() : ResourceType::Other;
    m_resourcesData->responseReceived(resourceID, response, resourceType);

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::ResponseReceived(
            qualifyResourceID(resourceID), *frameID, contextID, response, resourceType, timestamp),
        page->identifier());
}

void FrameNetworkAgentProxy::didReceiveData(ResourceLoaderIdentifier resourceID, const SharedBuffer* data, int dataLength, int encodedDataLength)
{
    if (data)
        m_resourcesData->maybeAddResourceData(resourceID, *data);

    RefPtr page = m_page.get();
    if (!page)
        return;

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::DataReceived(qualifyResourceID(resourceID), dataLength, encodedDataLength, timestamp),
        page->identifier());
}

void FrameNetworkAgentProxy::didFinishLoading(ResourceLoaderIdentifier resourceID, DocumentLoader* loader, const NetworkLoadMetrics&, ResourceLoader*)
{
    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    RefPtr protectedLoader = loader;
    RefPtr frame = protectedLoader->frame();
    RefPtr document = frame->document();
    if (RefPtr frameLoader = protectedLoader->frameLoader()) {
        auto* resourceData = m_resourcesData->data(resourceID);
        if (resourceData && resourceData->type() == ResourceType::Document) {
            if (RefPtr documentLoader = frameLoader->documentLoader())
                m_resourcesData->addResourceSharedBuffer(resourceID, documentLoader->mainResourceData(), document->encoding());
        }
    }

    m_resourcesData->maybeDecodeDataToContent(resourceID);

    RefPtr page = m_page.get();
    if (!page)
        return;

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::LoadingFinished(qualifyResourceID(resourceID), timestamp, String()),
        page->identifier());
}

void FrameNetworkAgentProxy::didFailLoading(ResourceLoaderIdentifier resourceID, DocumentLoader* loader, const ResourceError& error)
{
    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    RefPtr page = m_page.get();
    if (!page)
        return;

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::LoadingFailed(qualifyResourceID(resourceID), timestamp, error.localizedDescription(), error.isCancellation()),
        page->identifier());
}

void FrameNetworkAgentProxy::didLoadResourceFromMemoryCache(DocumentLoader* loader, CachedResource& cachedResource)
{
    if (!loader || !loader->frame() || !loader->frame()->document())
        return;

    RefPtr protectedLoader = loader;
    RefPtr page = m_page.get();
    if (!page)
        return;

    auto resourceID = ResourceLoaderIdentifier::generate();
    auto contextID = protectedLoader->frame()->document()->identifier();
    auto frameID = frameIdentifier(protectedLoader.get());
    auto resourceType = ResourceUtilities::inspectorResourceType(cachedResource);
    if (!frameID)
        return;

    m_resourcesData->resourceCreated(resourceID, resourceType);

    // Copy content from the CachedResource now, since the store does not hold
    // CachedResource references. This is the only chance to capture the content
    // for memory-cached resources (they don't go through didReceiveData).
    String content;
    bool base64Encoded;
    if (ResourceUtilities::cachedResourceContent(cachedResource, &content, &base64Encoded))
        m_resourcesData->setResourceContent(resourceID, content, base64Encoded);

    auto timestamp = MonotonicTime::now().secondsSinceEpoch().value();
    auto documentURL = protectedLoader->url().string();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingNetworkAgent::RequestServedFromMemoryCache(
            qualifyResourceID(resourceID), *frameID, contextID, documentURL, cachedResource.resourceRequest(),
            cachedResource.response(), resourceType, timestamp),
        page->identifier());
}

void FrameNetworkAgentProxy::didReceiveScriptResponse(ResourceLoaderIdentifier resourceID)
{
    m_resourcesData->setResourceType(resourceID, ResourceType::Script);
}

void FrameNetworkAgentProxy::didReceiveThreadableLoaderResponse(ResourceLoaderIdentifier, DocumentThreadableLoader&)
{
}

void FrameNetworkAgentProxy::willDestroyCachedResource(CachedResource&)
{
}

void FrameNetworkAgentProxy::setInitialScriptContent(ResourceLoaderIdentifier resourceID, const String& sourceString)
{
    m_resourcesData->setResourceContent(resourceID, sourceString);
}

void FrameNetworkAgentProxy::mainFrameNavigated(DocumentLoader&)
{
    m_resourcesData->clear();
}

} // namespace WebKit
