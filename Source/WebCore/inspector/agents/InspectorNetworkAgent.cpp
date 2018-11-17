/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#include "InspectorNetworkAgent.h"

#include "CachedCSSStyleSheet.h"
#include "CachedRawResource.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequestInitiators.h"
#include "CachedScript.h"
#include "CertificateInfo.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentThreadableLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTTPHeaderMap.h"
#include "HTTPHeaderNames.h"
#include "InspectorDOMAgent.h"
#include "InspectorTimelineAgent.h"
#include "InstrumentingAgents.h"
#include "JSExecState.h"
#include "JSWebSocket.h"
#include "LoaderStrategy.h"
#include "MIMETypeRegistry.h"
#include "MemoryCache.h"
#include "NetworkResourcesData.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "ProgressTracker.h"
#include "ResourceError.h"
#include "ResourceLoader.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptState.h"
#include "ScriptableDocumentParser.h"
#include "SubresourceLoader.h"
#include "TextResourceDecoder.h"
#include "ThreadableLoaderClient.h"
#include "URL.h"
#include "WebSocket.h"
#include "WebSocketChannel.h"
#include "WebSocketFrame.h"
#include <JavaScriptCore/ContentSearchUtilities.h>
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <wtf/JSONValues.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/Stopwatch.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringBuilder.h>

typedef Inspector::NetworkBackendDispatcherHandler::LoadResourceCallback LoadResourceCallback;

namespace WebCore {

using namespace Inspector;

namespace {

class InspectorThreadableLoaderClient final : public ThreadableLoaderClient {
    WTF_MAKE_NONCOPYABLE(InspectorThreadableLoaderClient);
public:
    InspectorThreadableLoaderClient(RefPtr<LoadResourceCallback>&& callback)
        : m_callback(WTFMove(callback))
    {
    }

    virtual ~InspectorThreadableLoaderClient() = default;

    void didReceiveResponse(unsigned long, const ResourceResponse& response) override
    {
        m_mimeType = response.mimeType();
        m_statusCode = response.httpStatusCode();

        // FIXME: This assumes text only responses. We should support non-text responses as well.
        TextEncoding textEncoding(response.textEncodingName());
        bool useDetector = false;
        if (!textEncoding.isValid()) {
            textEncoding = UTF8Encoding();
            useDetector = true;
        }

        m_decoder = TextResourceDecoder::create("text/plain"_s, textEncoding, useDetector);
    }

    void didReceiveData(const char* data, int dataLength) override
    {
        if (!dataLength)
            return;

        if (dataLength == -1)
            dataLength = strlen(data);

        m_responseText.append(m_decoder->decode(data, dataLength));
    }

    void didFinishLoading(unsigned long) override
    {
        if (m_decoder)
            m_responseText.append(m_decoder->flush());

        m_callback->sendSuccess(m_responseText.toString(), m_mimeType, m_statusCode);
        dispose();
    }

    void didFail(const ResourceError& error) override
    {
        m_callback->sendFailure(error.isAccessControl() ? "Loading resource for inspector failed access control check"_s : "Loading resource for inspector failed"_s);
        dispose();
    }

    void setLoader(RefPtr<ThreadableLoader>&& loader)
    {
        m_loader = WTFMove(loader);
    }

private:
    void dispose()
    {
        m_loader = nullptr;
        delete this;
    }

    RefPtr<LoadResourceCallback> m_callback;
    RefPtr<ThreadableLoader> m_loader;
    RefPtr<TextResourceDecoder> m_decoder;
    String m_mimeType;
    StringBuilder m_responseText;
    int m_statusCode;
};

} // namespace

InspectorNetworkAgent::InspectorNetworkAgent(WebAgentContext& context)
    : InspectorAgentBase("Network"_s, context)
    , m_frontendDispatcher(std::make_unique<Inspector::NetworkFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::NetworkBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_resourcesData(std::make_unique<NetworkResourcesData>())
{
}

void InspectorNetworkAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorNetworkAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    ErrorString unused;
    disable(unused);
}

static Ref<JSON::Object> buildObjectForHeaders(const HTTPHeaderMap& headers)
{
    Ref<JSON::Object> headersObject = JSON::Object::create();

    for (const auto& header : headers)
        headersObject->setString(header.key, header.value);
    return headersObject;
}

Ref<Inspector::Protocol::Network::ResourceTiming> InspectorNetworkAgent::buildObjectForTiming(const NetworkLoadMetrics& timing, ResourceLoader& resourceLoader)
{
    auto& loadTiming = resourceLoader.loadTiming();

    auto elapsedTimeSince = [&] (const MonotonicTime& time) {
        return m_environment.executionStopwatch()->elapsedTimeSince(time).seconds();
    };

    return Inspector::Protocol::Network::ResourceTiming::create()
        .setStartTime(elapsedTimeSince(loadTiming.startTime()))
        .setRedirectStart(elapsedTimeSince(loadTiming.redirectStart()))
        .setRedirectEnd(elapsedTimeSince(loadTiming.redirectEnd()))
        .setFetchStart(elapsedTimeSince(loadTiming.fetchStart()))
        .setDomainLookupStart(timing.domainLookupStart.milliseconds())
        .setDomainLookupEnd(timing.domainLookupEnd.milliseconds())
        .setConnectStart(timing.connectStart.milliseconds())
        .setConnectEnd(timing.connectEnd.milliseconds())
        .setSecureConnectionStart(timing.secureConnectionStart.milliseconds())
        .setRequestStart(timing.requestStart.milliseconds())
        .setResponseStart(timing.responseStart.milliseconds())
        .setResponseEnd(timing.responseEnd.milliseconds())
        .release();
}

static Inspector::Protocol::Network::Metrics::Priority toProtocol(NetworkLoadPriority priority)
{
    switch (priority) {
    case NetworkLoadPriority::Low:
        return Inspector::Protocol::Network::Metrics::Priority::Low;
    case NetworkLoadPriority::Medium:
        return Inspector::Protocol::Network::Metrics::Priority::Medium;
    case NetworkLoadPriority::High:
        return Inspector::Protocol::Network::Metrics::Priority::High;
    case NetworkLoadPriority::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
    return Inspector::Protocol::Network::Metrics::Priority::Medium;
}

Ref<Inspector::Protocol::Network::Metrics> InspectorNetworkAgent::buildObjectForMetrics(const NetworkLoadMetrics& networkLoadMetrics)
{
    auto metrics = Inspector::Protocol::Network::Metrics::create().release();

    if (!networkLoadMetrics.protocol.isNull())
        metrics->setProtocol(networkLoadMetrics.protocol);
    if (networkLoadMetrics.priority != NetworkLoadPriority::Unknown)
        metrics->setPriority(toProtocol(networkLoadMetrics.priority));
    if (!networkLoadMetrics.remoteAddress.isNull())
        metrics->setRemoteAddress(networkLoadMetrics.remoteAddress);
    if (!networkLoadMetrics.connectionIdentifier.isNull())
        metrics->setConnectionIdentifier(networkLoadMetrics.connectionIdentifier);
    if (!networkLoadMetrics.requestHeaders.isEmpty())
        metrics->setRequestHeaders(buildObjectForHeaders(networkLoadMetrics.requestHeaders));

    if (networkLoadMetrics.requestHeaderBytesSent != std::numeric_limits<uint32_t>::max())
        metrics->setRequestHeaderBytesSent(networkLoadMetrics.requestHeaderBytesSent);
    if (networkLoadMetrics.requestBodyBytesSent != std::numeric_limits<uint64_t>::max())
        metrics->setRequestBodyBytesSent(networkLoadMetrics.requestBodyBytesSent);
    if (networkLoadMetrics.responseHeaderBytesReceived != std::numeric_limits<uint32_t>::max())
        metrics->setResponseHeaderBytesReceived(networkLoadMetrics.responseHeaderBytesReceived);
    if (networkLoadMetrics.responseBodyBytesReceived != std::numeric_limits<uint64_t>::max())
        metrics->setResponseBodyBytesReceived(networkLoadMetrics.responseBodyBytesReceived);
    if (networkLoadMetrics.responseBodyDecodedSize != std::numeric_limits<uint64_t>::max())
        metrics->setResponseBodyDecodedSize(networkLoadMetrics.responseBodyDecodedSize);

    return metrics;
}

static Ref<Inspector::Protocol::Network::Request> buildObjectForResourceRequest(const ResourceRequest& request)
{
    auto requestObject = Inspector::Protocol::Network::Request::create()
        .setUrl(request.url().string())
        .setMethod(request.httpMethod())
        .setHeaders(buildObjectForHeaders(request.httpHeaderFields()))
        .release();
    if (request.httpBody() && !request.httpBody()->isEmpty()) {
        auto bytes = request.httpBody()->flatten();
        requestObject->setPostData(String::fromUTF8WithLatin1Fallback(bytes.data(), bytes.size()));
    }
    return requestObject;
}

static Inspector::Protocol::Network::Response::Source responseSource(ResourceResponse::Source source)
{
    switch (source) {
    case ResourceResponse::Source::ApplicationCache:
        // FIXME: Add support for ApplicationCache in inspector.
    case ResourceResponse::Source::Unknown:
        return Inspector::Protocol::Network::Response::Source::Unknown;
    case ResourceResponse::Source::Network:
        return Inspector::Protocol::Network::Response::Source::Network;
    case ResourceResponse::Source::MemoryCache:
    case ResourceResponse::Source::MemoryCacheAfterValidation:
        return Inspector::Protocol::Network::Response::Source::MemoryCache;
    case ResourceResponse::Source::DiskCache:
    case ResourceResponse::Source::DiskCacheAfterValidation:
        return Inspector::Protocol::Network::Response::Source::DiskCache;
    case ResourceResponse::Source::ServiceWorker:
        return Inspector::Protocol::Network::Response::Source::ServiceWorker;
    }

    ASSERT_NOT_REACHED();
    return Inspector::Protocol::Network::Response::Source::Unknown;
}

RefPtr<Inspector::Protocol::Network::Response> InspectorNetworkAgent::buildObjectForResourceResponse(const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    if (response.isNull())
        return nullptr;

    Ref<JSON::Object> headers = buildObjectForHeaders(response.httpHeaderFields());

    auto responseObject = Inspector::Protocol::Network::Response::create()
        .setUrl(response.url().string())
        .setStatus(response.httpStatusCode())
        .setStatusText(response.httpStatusText())
        .setHeaders(WTFMove(headers))
        .setMimeType(response.mimeType())
        .setSource(responseSource(response.source()))
        .release();

    if (resourceLoader)
        responseObject->setTiming(buildObjectForTiming(response.deprecatedNetworkLoadMetrics(), *resourceLoader));

    if (auto& certificateInfo = response.certificateInfo()) {
        auto securityPayload = Inspector::Protocol::Security::Security::create()
            .release();

        if (auto certificateSummaryInfo = certificateInfo.value().summaryInfo()) {
            auto certificatePayload = Inspector::Protocol::Security::Certificate::create()
                .release();

            certificatePayload->setSubject(certificateSummaryInfo.value().subject);

            if (auto validFrom = certificateSummaryInfo.value().validFrom)
                certificatePayload->setValidFrom(validFrom.seconds());

            if (auto validUntil = certificateSummaryInfo.value().validUntil)
                certificatePayload->setValidUntil(validUntil.seconds());

            auto dnsNamesPayload = JSON::ArrayOf<String>::create();
            for (auto& dnsName : certificateSummaryInfo.value().dnsNames)
                dnsNamesPayload->addItem(dnsName);
            if (dnsNamesPayload->length())
                certificatePayload->setDnsNames(WTFMove(dnsNamesPayload));

            auto ipAddressesPayload = JSON::ArrayOf<String>::create();
            for (auto& ipAddress : certificateSummaryInfo.value().ipAddresses)
                ipAddressesPayload->addItem(ipAddress);
            if (ipAddressesPayload->length())
                certificatePayload->setDnsNames(WTFMove(ipAddressesPayload));

            securityPayload->setCertificate(WTFMove(certificatePayload));
        }

        responseObject->setSecurity(WTFMove(securityPayload));
    }

    return WTFMove(responseObject);
}

Ref<Inspector::Protocol::Network::CachedResource> InspectorNetworkAgent::buildObjectForCachedResource(CachedResource* cachedResource)
{
    auto resourceObject = Inspector::Protocol::Network::CachedResource::create()
        .setUrl(cachedResource->url())
        .setType(InspectorPageAgent::cachedResourceTypeJSON(*cachedResource))
        .setBodySize(cachedResource->encodedSize())
        .release();

    auto resourceResponse = buildObjectForResourceResponse(cachedResource->response(), cachedResource->loader());
    resourceObject->setResponse(WTFMove(resourceResponse));

    String sourceMappingURL = InspectorPageAgent::sourceMapURLForResource(cachedResource);
    if (!sourceMappingURL.isEmpty())
        resourceObject->setSourceMapURL(sourceMappingURL);

    return resourceObject;
}

InspectorNetworkAgent::~InspectorNetworkAgent()
{
    if (m_enabled) {
        ErrorString unused;
        disable(unused);
    }
    ASSERT(!m_instrumentingAgents.inspectorNetworkAgent());
}

double InspectorNetworkAgent::timestamp()
{
    return m_environment.executionStopwatch()->elapsedTime().seconds();
}

void InspectorNetworkAgent::willSendRequest(unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse, InspectorPageAgent::ResourceType type)
{
    if (request.hiddenFromInspector()) {
        m_hiddenRequestIdentifiers.add(identifier);
        return;
    }

    double sendTimestamp = timestamp();
    WallTime walltime = WallTime::now();

    String requestId = IdentifiersFactory::requestId(identifier);
    String frameId = frameIdentifier(loader);
    String loaderId = loaderIdentifier(loader);
    String targetId = request.initiatorIdentifier();

    if (type == InspectorPageAgent::OtherResource) {
        if (m_loadingXHRSynchronously)
            type = InspectorPageAgent::XHRResource;
        else if (loader && equalIgnoringFragmentIdentifier(request.url(), loader->url()) && !loader->isCommitted())
            type = InspectorPageAgent::DocumentResource;
        else if (loader) {
            for (auto& linkIcon : loader->linkIcons()) {
                if (equalIgnoringFragmentIdentifier(request.url(), linkIcon.url)) {
                    type = InspectorPageAgent::ImageResource;
                    break;
                }
            }
        }
    }

    m_resourcesData->resourceCreated(requestId, loaderId, type);

    for (auto& entry : m_extraRequestHeaders)
        request.setHTTPHeaderField(entry.key, entry.value);

    auto protocolResourceType = InspectorPageAgent::resourceTypeJSON(type);

    Document* document = loader && loader->frame() ? loader->frame()->document() : nullptr;
    auto initiatorObject = buildInitiatorObject(document, request);

    String url = loader ? loader->url().string() : request.url();
    m_frontendDispatcher->requestWillBeSent(requestId, frameId, loaderId, url, buildObjectForResourceRequest(request), sendTimestamp, walltime.secondsSinceEpoch().seconds(), initiatorObject, buildObjectForResourceResponse(redirectResponse, nullptr), type != InspectorPageAgent::OtherResource ? &protocolResourceType : nullptr, targetId.isEmpty() ? nullptr : &targetId);
}

static InspectorPageAgent::ResourceType resourceTypeForCachedResource(CachedResource* resource)
{
    if (resource)
        return InspectorPageAgent::inspectorResourceType(*resource);
    return InspectorPageAgent::OtherResource;
}

static InspectorPageAgent::ResourceType resourceTypeForLoadType(InspectorInstrumentation::LoadType loadType)
{
    switch (loadType) {
    case InspectorInstrumentation::LoadType::Ping:
        return InspectorPageAgent::PingResource;
    case InspectorInstrumentation::LoadType::Beacon:
        return InspectorPageAgent::BeaconResource;
    }

    ASSERT_NOT_REACHED();
    return InspectorPageAgent::OtherResource;
}

void InspectorNetworkAgent::willSendRequest(unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    auto* cachedResource = loader ? InspectorPageAgent::cachedResource(loader->frame(), request.url()) : nullptr;
    willSendRequest(identifier, loader, request, redirectResponse, resourceTypeForCachedResource(cachedResource));
}

void InspectorNetworkAgent::willSendRequestOfType(unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, InspectorInstrumentation::LoadType loadType)
{
    willSendRequest(identifier, loader, request, ResourceResponse(), resourceTypeForLoadType(loadType));
}

void InspectorNetworkAgent::didReceiveResponse(unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    if (m_hiddenRequestIdentifiers.contains(identifier))
        return;

    String requestId = IdentifiersFactory::requestId(identifier);

    std::optional<ResourceResponse> realResponse;
    if (platformStrategies()->loaderStrategy()->havePerformedSecurityChecks(response)) {
        callOnMainThreadAndWait([&] {
            // We do not need to isolate response since it comes straight from IPC, but we might want to isolate it for extra safety.
            auto response = platformStrategies()->loaderStrategy()->responseFromResourceLoadIdentifier(identifier);
            if (!response.isNull())
                realResponse = WTFMove(response);
        });
    }

    RefPtr<Inspector::Protocol::Network::Response> resourceResponse = buildObjectForResourceResponse(realResponse ? *realResponse : response, resourceLoader);

    bool isNotModified = response.httpStatusCode() == 304;

    CachedResource* cachedResource = nullptr;
    if (resourceLoader && resourceLoader->isSubresourceLoader() && !isNotModified)
        cachedResource = static_cast<SubresourceLoader*>(resourceLoader)->cachedResource();
    if (!cachedResource && loader)
        cachedResource = InspectorPageAgent::cachedResource(loader->frame(), response.url());

    if (cachedResource) {
        // Use mime type from cached resource in case the one in response is empty.
        if (resourceResponse && response.mimeType().isEmpty())
            resourceResponse->setString(Inspector::Protocol::Network::Response::MimeType, cachedResource->response().mimeType());
        m_resourcesData->addCachedResource(requestId, cachedResource);
    }

    InspectorPageAgent::ResourceType type = m_resourcesData->resourceType(requestId);
    InspectorPageAgent::ResourceType newType = cachedResource ? InspectorPageAgent::inspectorResourceType(*cachedResource) : type;

    // FIXME: XHRResource is returned for CachedResource::Type::RawResource, it should be OtherResource unless it truly is an XHR.
    // RawResource is used for loading worker scripts, and those should stay as ScriptResource and not change to XHRResource.
    if (type != newType && newType != InspectorPageAgent::XHRResource && newType != InspectorPageAgent::OtherResource)
        type = newType;

    String frameId = frameIdentifier(loader);
    String loaderId = loaderIdentifier(loader);

    m_resourcesData->responseReceived(requestId, frameId, response, type, shouldForceBufferingNetworkResourceData());

    m_frontendDispatcher->responseReceived(requestId, frameId, loaderId, timestamp(), InspectorPageAgent::resourceTypeJSON(type), resourceResponse);

    // If we revalidated the resource and got Not modified, send content length following didReceiveResponse
    // as there will be no calls to didReceiveData from the network stack.
    if (isNotModified && cachedResource && cachedResource->encodedSize())
        didReceiveData(identifier, nullptr, cachedResource->encodedSize(), 0);
}

void InspectorNetworkAgent::didReceiveData(unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    if (m_hiddenRequestIdentifiers.contains(identifier))
        return;

    String requestId = IdentifiersFactory::requestId(identifier);

    if (data) {
        NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->maybeAddResourceData(requestId, data, dataLength);

        // For a synchronous XHR, if we didn't add data then we can apply it here as base64 encoded content.
        // Often the data is text and we would have a decoder, but for non-text we won't have a decoder.
        // Sync XHRs may not have a cached resource, while non-sync XHRs usually transfer data over on completion.
        if (m_loadingXHRSynchronously && resourceData && !resourceData->hasBufferedData() && !resourceData->cachedResource())
            m_resourcesData->setResourceContent(requestId, base64Encode(data, dataLength), true);
    }

    m_frontendDispatcher->dataReceived(requestId, timestamp(), dataLength, encodedDataLength);
}

void InspectorNetworkAgent::didFinishLoading(unsigned long identifier, DocumentLoader* loader, const NetworkLoadMetrics& networkLoadMetrics, ResourceLoader* resourceLoader)
{
    if (m_hiddenRequestIdentifiers.remove(identifier))
        return;

    double elapsedFinishTime;
    if (resourceLoader && networkLoadMetrics.isComplete()) {
        MonotonicTime fetchStart = resourceLoader->loadTiming().fetchStart();
        Seconds fetchStartInInspector = m_environment.executionStopwatch()->elapsedTimeSince(fetchStart);
        elapsedFinishTime = (fetchStartInInspector + networkLoadMetrics.responseEnd).seconds();
    } else
        elapsedFinishTime = timestamp();

    String requestId = IdentifiersFactory::requestId(identifier);
    if (loader && m_resourcesData->resourceType(requestId) == InspectorPageAgent::DocumentResource)
        m_resourcesData->addResourceSharedBuffer(requestId, loader->frameLoader()->documentLoader()->mainResourceData(), loader->frame()->document()->encoding());

    m_resourcesData->maybeDecodeDataToContent(requestId);

    String sourceMappingURL;
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    if (resourceData && resourceData->cachedResource())
        sourceMappingURL = InspectorPageAgent::sourceMapURLForResource(resourceData->cachedResource());

    std::optional<NetworkLoadMetrics> realMetrics;
    if (platformStrategies()->loaderStrategy()->shouldPerformSecurityChecks() && !networkLoadMetrics.isComplete()) {
        callOnMainThreadAndWait([&] {
            realMetrics = platformStrategies()->loaderStrategy()->networkMetricsFromResourceLoadIdentifier(identifier).isolatedCopy();
        });
    }
    RefPtr<Inspector::Protocol::Network::Metrics> metrics = buildObjectForMetrics(realMetrics ? *realMetrics : networkLoadMetrics);

    m_frontendDispatcher->loadingFinished(requestId, elapsedFinishTime, !sourceMappingURL.isEmpty() ? &sourceMappingURL : nullptr, metrics);
}

void InspectorNetworkAgent::didFailLoading(unsigned long identifier, DocumentLoader* loader, const ResourceError& error)
{
    if (m_hiddenRequestIdentifiers.remove(identifier))
        return;

    String requestId = IdentifiersFactory::requestId(identifier);

    if (loader && m_resourcesData->resourceType(requestId) == InspectorPageAgent::DocumentResource) {
        Frame* frame = loader->frame();
        if (frame && frame->loader().documentLoader() && frame->document()) {
            m_resourcesData->addResourceSharedBuffer(requestId,
                frame->loader().documentLoader()->mainResourceData(),
                frame->document()->encoding());
        }
    }

    bool canceled = error.isCancellation();
    m_frontendDispatcher->loadingFailed(requestId, timestamp(), error.localizedDescription(), canceled ? &canceled : nullptr);
}

void InspectorNetworkAgent::didLoadResourceFromMemoryCache(DocumentLoader* loader, CachedResource& resource)
{
    ASSERT(loader);
    if (!loader)
        return;

    unsigned long identifier = loader->frame()->page()->progress().createUniqueIdentifier();
    String requestId = IdentifiersFactory::requestId(identifier);
    String loaderId = loaderIdentifier(loader);
    String frameId = frameIdentifier(loader);

    m_resourcesData->resourceCreated(requestId, loaderId, resource);

    auto initiatorObject = buildInitiatorObject(loader->frame() ? loader->frame()->document() : nullptr, resource.resourceRequest());

    // FIXME: It would be ideal to generate the Network.Response with the MemoryCache source
    // instead of whatever ResourceResponse::Source the CachedResources's response has.
    // The frontend already knows for certain that this was served from the memory cache.

    m_frontendDispatcher->requestServedFromMemoryCache(requestId, frameId, loaderId, loader->url().string(), timestamp(), initiatorObject, buildObjectForCachedResource(&resource));
}

void InspectorNetworkAgent::setInitialScriptContent(unsigned long identifier, const String& sourceString)
{
    m_resourcesData->setResourceContent(IdentifiersFactory::requestId(identifier), sourceString);
}

void InspectorNetworkAgent::didReceiveScriptResponse(unsigned long identifier)
{
    m_resourcesData->setResourceType(IdentifiersFactory::requestId(identifier), InspectorPageAgent::ScriptResource);
}

void InspectorNetworkAgent::didReceiveThreadableLoaderResponse(unsigned long identifier, DocumentThreadableLoader& documentThreadableLoader)
{
    String initiator = documentThreadableLoader.options().initiator;
    if (initiator == cachedResourceRequestInitiators().fetch)
        m_resourcesData->setResourceType(IdentifiersFactory::requestId(identifier), InspectorPageAgent::FetchResource);
    else if (initiator == cachedResourceRequestInitiators().xmlhttprequest)
        m_resourcesData->setResourceType(IdentifiersFactory::requestId(identifier), InspectorPageAgent::XHRResource);
}

void InspectorNetworkAgent::willLoadXHRSynchronously()
{
    m_loadingXHRSynchronously = true;
}

void InspectorNetworkAgent::didLoadXHRSynchronously()
{
    m_loadingXHRSynchronously = false;
}

void InspectorNetworkAgent::willDestroyCachedResource(CachedResource& cachedResource)
{
    Vector<String> requestIds = m_resourcesData->removeCachedResource(&cachedResource);
    if (!requestIds.size())
        return;

    String content;
    bool base64Encoded;
    if (!InspectorNetworkAgent::cachedResourceContent(cachedResource, &content, &base64Encoded))
        return;

    for (auto& id : requestIds)
        m_resourcesData->setResourceContent(id, content, base64Encoded);
}

void InspectorNetworkAgent::willRecalculateStyle()
{
    m_isRecalculatingStyle = true;
}

void InspectorNetworkAgent::didRecalculateStyle()
{
    m_isRecalculatingStyle = false;
    m_styleRecalculationInitiator = nullptr;
}

void InspectorNetworkAgent::didScheduleStyleRecalculation(Document& document)
{
    if (!m_styleRecalculationInitiator)
        m_styleRecalculationInitiator = buildInitiatorObject(&document);
}

RefPtr<Inspector::Protocol::Network::Initiator> InspectorNetworkAgent::buildInitiatorObject(Document* document, std::optional<const ResourceRequest&> resourceRequest)
{
    // FIXME: Worker support.
    if (!isMainThread()) {
        return Inspector::Protocol::Network::Initiator::create()
            .setType(Inspector::Protocol::Network::Initiator::Type::Other)
            .release();
    }

    RefPtr<Inspector::Protocol::Network::Initiator> initiatorObject;

    Ref<ScriptCallStack> stackTrace = createScriptCallStack(JSExecState::currentState());
    if (stackTrace->size() > 0) {
        initiatorObject = Inspector::Protocol::Network::Initiator::create()
            .setType(Inspector::Protocol::Network::Initiator::Type::Script)
            .release();
        initiatorObject->setStackTrace(stackTrace->buildInspectorArray());
    } else if (document && document->scriptableDocumentParser()) {
        initiatorObject = Inspector::Protocol::Network::Initiator::create()
            .setType(Inspector::Protocol::Network::Initiator::Type::Parser)
            .release();
        initiatorObject->setUrl(document->url().string());
        initiatorObject->setLineNumber(document->scriptableDocumentParser()->textPosition().m_line.oneBasedInt());
    }

    auto domAgent = m_instrumentingAgents.inspectorDOMAgent();
    if (domAgent && resourceRequest) {
        if (auto inspectorInitiatorNodeIdentifier = resourceRequest->inspectorInitiatorNodeIdentifier()) {
            if (!initiatorObject) {
                initiatorObject = Inspector::Protocol::Network::Initiator::create()
                    .setType(Inspector::Protocol::Network::Initiator::Type::Other)
                    .release();
            }

            initiatorObject->setNodeId(*inspectorInitiatorNodeIdentifier);
        }
    }

    if (initiatorObject)
        return initiatorObject;

    if (m_isRecalculatingStyle && m_styleRecalculationInitiator)
        return m_styleRecalculationInitiator;

    return Inspector::Protocol::Network::Initiator::create()
        .setType(Inspector::Protocol::Network::Initiator::Type::Other)
        .release();
}

void InspectorNetworkAgent::didCreateWebSocket(unsigned long identifier, const URL& requestURL)
{
    m_frontendDispatcher->webSocketCreated(IdentifiersFactory::requestId(identifier), requestURL.string());
}

void InspectorNetworkAgent::willSendWebSocketHandshakeRequest(unsigned long identifier, const ResourceRequest& request)
{
    auto requestObject = Inspector::Protocol::Network::WebSocketRequest::create()
        .setHeaders(buildObjectForHeaders(request.httpHeaderFields()))
        .release();
    m_frontendDispatcher->webSocketWillSendHandshakeRequest(IdentifiersFactory::requestId(identifier), timestamp(), WallTime::now().secondsSinceEpoch().seconds(), WTFMove(requestObject));
}

void InspectorNetworkAgent::didReceiveWebSocketHandshakeResponse(unsigned long identifier, const ResourceResponse& response)
{
    auto responseObject = Inspector::Protocol::Network::WebSocketResponse::create()
        .setStatus(response.httpStatusCode())
        .setStatusText(response.httpStatusText())
        .setHeaders(buildObjectForHeaders(response.httpHeaderFields()))
        .release();
    m_frontendDispatcher->webSocketHandshakeResponseReceived(IdentifiersFactory::requestId(identifier), timestamp(), WTFMove(responseObject));
}

void InspectorNetworkAgent::didCloseWebSocket(unsigned long identifier)
{
    m_frontendDispatcher->webSocketClosed(IdentifiersFactory::requestId(identifier), timestamp());
}

void InspectorNetworkAgent::didReceiveWebSocketFrame(unsigned long identifier, const WebSocketFrame& frame)
{
    auto frameObject = Inspector::Protocol::Network::WebSocketFrame::create()
        .setOpcode(frame.opCode)
        .setMask(frame.masked)
        .setPayloadData(String::fromUTF8WithLatin1Fallback(frame.payload, frame.payloadLength))
        .setPayloadLength(frame.payloadLength)
        .release();
    m_frontendDispatcher->webSocketFrameReceived(IdentifiersFactory::requestId(identifier), timestamp(), WTFMove(frameObject));
}

void InspectorNetworkAgent::didSendWebSocketFrame(unsigned long identifier, const WebSocketFrame& frame)
{
    auto frameObject = Inspector::Protocol::Network::WebSocketFrame::create()
        .setOpcode(frame.opCode)
        .setMask(frame.masked)
        .setPayloadData(String::fromUTF8WithLatin1Fallback(frame.payload, frame.payloadLength))
        .setPayloadLength(frame.payloadLength)
        .release();
    m_frontendDispatcher->webSocketFrameSent(IdentifiersFactory::requestId(identifier), timestamp(), WTFMove(frameObject));
}

void InspectorNetworkAgent::didReceiveWebSocketFrameError(unsigned long identifier, const String& errorMessage)
{
    m_frontendDispatcher->webSocketFrameError(IdentifiersFactory::requestId(identifier), timestamp(), errorMessage);
}

void InspectorNetworkAgent::enable(ErrorString&)
{
    enable();
}

void InspectorNetworkAgent::enable()
{
    m_enabled = true;
    m_instrumentingAgents.setInspectorNetworkAgent(this);

    {
        LockHolder lock(WebSocket::allActiveWebSocketsMutex());

        for (WebSocket* webSocket : activeWebSockets(lock)) {
            ASSERT(is<WebSocketChannel>(webSocket->channel().get()));
            WebSocketChannel* channel = downcast<WebSocketChannel>(webSocket->channel().get());

            unsigned identifier = channel->identifier();
            didCreateWebSocket(identifier, webSocket->url());
            willSendWebSocketHandshakeRequest(identifier, channel->clientHandshakeRequest());

            if (channel->handshakeMode() == WebSocketHandshake::Connected)
                didReceiveWebSocketHandshakeResponse(identifier, channel->serverHandshakeResponse());

            if (webSocket->readyState() == WebSocket::CLOSED)
                didCloseWebSocket(identifier);
        }
    }
}

void InspectorNetworkAgent::disable(ErrorString&)
{
    m_enabled = false;
    m_instrumentingAgents.setInspectorNetworkAgent(nullptr);
    m_resourcesData->clear();
    m_extraRequestHeaders.clear();

    setResourceCachingDisabled(false);
}

void InspectorNetworkAgent::setExtraHTTPHeaders(ErrorString&, const JSON::Object& headers)
{
    for (auto& entry : headers) {
        String stringValue;
        if (entry.value->asString(stringValue))
            m_extraRequestHeaders.set(entry.key, stringValue);
    }
}

void InspectorNetworkAgent::getResponseBody(ErrorString& errorString, const String& requestId, String* content, bool* base64Encoded)
{
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    if (!resourceData) {
        errorString = "No resource with given identifier found"_s;
        return;
    }

    if (resourceData->hasContent()) {
        *base64Encoded = resourceData->base64Encoded();
        *content = resourceData->content();
        return;
    }

    if (resourceData->isContentEvicted()) {
        errorString = "Request content was evicted from inspector cache"_s;
        return;
    }

    if (resourceData->buffer() && !resourceData->textEncodingName().isNull()) {
        *base64Encoded = false;
        if (InspectorPageAgent::sharedBufferContent(resourceData->buffer(), resourceData->textEncodingName(), *base64Encoded, content))
            return;
    }

    if (resourceData->cachedResource()) {
        if (InspectorNetworkAgent::cachedResourceContent(*resourceData->cachedResource(), content, base64Encoded))
            return;
    }

    errorString = "No data found for resource with given identifier"_s;
}

void InspectorNetworkAgent::setResourceCachingDisabled(ErrorString&, bool disabled)
{
    setResourceCachingDisabled(disabled);
}

void InspectorNetworkAgent::loadResource(const String& frameId, const String& urlString, Ref<LoadResourceCallback>&& callback)
{
    ErrorString error;
    auto* context = scriptExecutionContext(error, frameId);
    if (!context) {
        callback->sendFailure(error);
        return;
    }

    URL url = context->completeURL(urlString);
    ResourceRequest request(url);
    request.setHTTPMethod("GET"_s);
    request.setHiddenFromInspector(true);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks; // So we remove this from m_hiddenRequestIdentifiers on completion.
    options.defersLoadingPolicy = DefersLoadingPolicy::DisallowDefersLoading; // So the request is never deferred.
    options.mode = FetchOptions::Mode::NoCors;
    options.credentials = FetchOptions::Credentials::SameOrigin;
    options.contentSecurityPolicyEnforcement = ContentSecurityPolicyEnforcement::DoNotEnforce;

    // InspectorThreadableLoaderClient deletes itself when the load completes or fails.
    InspectorThreadableLoaderClient* inspectorThreadableLoaderClient = new InspectorThreadableLoaderClient(callback.copyRef());
    auto loader = ThreadableLoader::create(*context, *inspectorThreadableLoaderClient, WTFMove(request), options);
    if (!loader) {
        callback->sendFailure("Could not load requested resource."_s);
        return;
    }

    // If the load already completed, inspectorThreadableLoaderClient will have been deleted and we will have already called the callback.
    if (!callback->isActive())
        return;

    inspectorThreadableLoaderClient->setLoader(WTFMove(loader));
}

void InspectorNetworkAgent::getSerializedCertificate(ErrorString& errorString, const String& requestId, String* serializedCertificate)
{
    auto* resourceData = m_resourcesData->data(requestId);
    if (!resourceData) {
        errorString = "No resource with given identifier found"_s;
        return;
    }

    auto& certificate = resourceData->certificateInfo();
    if (!certificate || certificate.value().isEmpty()) {
        errorString = "No certificate for resource"_s;
        return;
    }

    WTF::Persistence::Encoder encoder;
    encoder << certificate.value();
    *serializedCertificate = base64Encode(encoder.buffer(), encoder.bufferSize());
}

WebSocket* InspectorNetworkAgent::webSocketForRequestId(const String& requestId)
{
    LockHolder lock(WebSocket::allActiveWebSocketsMutex());

    for (WebSocket* webSocket : activeWebSockets(lock)) {
        ASSERT(is<WebSocketChannel>(webSocket->channel().get()));
        WebSocketChannel* channel = downcast<WebSocketChannel>(webSocket->channel().get());
        if (IdentifiersFactory::requestId(channel->identifier()) == requestId)
            return webSocket;
    }

    return nullptr;
}

static JSC::JSValue webSocketAsScriptValue(JSC::ExecState& state, WebSocket* webSocket)
{
    JSC::JSLockHolder lock(&state);
    return toJS(&state, deprecatedGlobalObjectForPrototype(&state), webSocket);
}

void InspectorNetworkAgent::resolveWebSocket(ErrorString& errorString, const String& requestId, const String* objectGroup, RefPtr<Inspector::Protocol::Runtime::RemoteObject>& result)
{
    WebSocket* webSocket = webSocketForRequestId(requestId);
    if (!webSocket) {
        errorString = "WebSocket not found"_s;
        return;
    }

    // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display iframe's and worker's WebSockets
    if (!is<Document>(webSocket->scriptExecutionContext()))
        return;

    auto* document = downcast<Document>(webSocket->scriptExecutionContext());
    auto* frame = document->frame();
    if (!frame) {
        errorString = "WebSocket belongs to document without a frame"_s;
        return;
    }

    auto& state = *mainWorldExecState(frame);
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(&state);
    ASSERT(!injectedScript.hasNoValue());

    String objectGroupName = objectGroup ? *objectGroup : String();
    result = injectedScript.wrapObject(webSocketAsScriptValue(state, webSocket), objectGroupName);
}

bool InspectorNetworkAgent::shouldTreatAsText(const String& mimeType)
{
    return startsWithLettersIgnoringASCIICase(mimeType, "text/")
        || MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType)
        || MIMETypeRegistry::isSupportedJSONMIMEType(mimeType)
        || MIMETypeRegistry::isXMLMIMEType(mimeType);
}

Ref<TextResourceDecoder> InspectorNetworkAgent::createTextDecoder(const String& mimeType, const String& textEncodingName)
{
    if (!textEncodingName.isEmpty())
        return TextResourceDecoder::create("text/plain"_s, textEncodingName);

    if (MIMETypeRegistry::isTextMIMEType(mimeType))
        return TextResourceDecoder::create(mimeType, "UTF-8");
    if (MIMETypeRegistry::isXMLMIMEType(mimeType)) {
        auto decoder = TextResourceDecoder::create("application/xml"_s);
        decoder->useLenientXMLDecoding();
        return decoder;
    }

    return TextResourceDecoder::create("text/plain"_s, "UTF-8");
}

std::optional<String> InspectorNetworkAgent::textContentForCachedResource(CachedResource& cachedResource)
{
    if (!InspectorNetworkAgent::shouldTreatAsText(cachedResource.mimeType()))
        return std::nullopt;

    String result;
    bool base64Encoded;
    if (InspectorNetworkAgent::cachedResourceContent(cachedResource, &result, &base64Encoded)) {
        ASSERT(!base64Encoded);
        return result;
    }

    return std::nullopt;
}

bool InspectorNetworkAgent::cachedResourceContent(CachedResource& resource, String* result, bool* base64Encoded)
{
    ASSERT(result);
    ASSERT(base64Encoded);

    if (!resource.encodedSize()) {
        *base64Encoded = false;
        *result = String();
        return true;
    }

    switch (resource.type()) {
    case CachedResource::Type::CSSStyleSheet:
        *base64Encoded = false;
        *result = downcast<CachedCSSStyleSheet>(resource).sheetText();
        // The above can return a null String if the MIME type is invalid.
        return !result->isNull();
    case CachedResource::Type::Script:
        *base64Encoded = false;
        *result = downcast<CachedScript>(resource).script().toString();
        return true;
    default:
        auto* buffer = resource.resourceBuffer();
        if (!buffer)
            return false;

        if (InspectorNetworkAgent::shouldTreatAsText(resource.mimeType())) {
            auto decoder = InspectorNetworkAgent::createTextDecoder(resource.mimeType(), resource.response().textEncodingName());
            *base64Encoded = false;
            *result = decoder->decodeAndFlush(buffer->data(), buffer->size());
            return true;
        }

        *base64Encoded = true;
        *result = base64Encode(buffer->data(), buffer->size());
        return true;
    }
}

static Ref<Inspector::Protocol::Page::SearchResult> buildObjectForSearchResult(const String& requestId, const String& frameId, const String& url, int matchesCount)
{
    auto searchResult = Inspector::Protocol::Page::SearchResult::create()
        .setUrl(url)
        .setFrameId(frameId)
        .setMatchesCount(matchesCount)
        .release();
    searchResult->setRequestId(requestId);
    return searchResult;
}

static std::optional<String> textContentForResourceData(const NetworkResourcesData::ResourceData& resourceData)
{
    if (resourceData.hasContent() && !resourceData.base64Encoded())
        return resourceData.content();

    if (resourceData.cachedResource())
        return InspectorNetworkAgent::textContentForCachedResource(*resourceData.cachedResource());

    return std::nullopt;
}

void InspectorNetworkAgent::searchOtherRequests(const JSC::Yarr::RegularExpression& regex, RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>>& result)
{
    Vector<NetworkResourcesData::ResourceData*> resources = m_resourcesData->resources();
    for (auto* resourceData : resources) {
        if (auto textContent = textContentForResourceData(*resourceData)) {
            int matchesCount = ContentSearchUtilities::countRegularExpressionMatches(regex, resourceData->content());
            if (matchesCount)
                result->addItem(buildObjectForSearchResult(resourceData->requestId(), resourceData->frameId(), resourceData->url(), matchesCount));
        }
    }
}

void InspectorNetworkAgent::searchInRequest(ErrorString& errorString, const String& requestId, const String& query, bool caseSensitive, bool isRegex, RefPtr<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>>& results)
{
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    if (!resourceData) {
        errorString = "No resource with given identifier found"_s;
        return;
    }

    if (!resourceData->hasContent()) {
        errorString = "No resource content"_s;
        return;
    }

    results = ContentSearchUtilities::searchInTextByLines(resourceData->content(), query, caseSensitive, isRegex);
}

void InspectorNetworkAgent::mainFrameNavigated(DocumentLoader& loader)
{
    m_resourcesData->clear(loaderIdentifier(&loader));
}

} // namespace WebCore
