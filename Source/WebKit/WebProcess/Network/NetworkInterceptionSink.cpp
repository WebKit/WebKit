/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "NetworkInterceptionSink.h"

#include "WebPage.h"
#include <WebCore/FormData.h>
#include <WebCore/HTTPHeaderMap.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/Page.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkInterceptionSink);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(NetworkInterceptionSink::InspectorPendingRequest);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(NetworkInterceptionSink::InspectorPendingResponse);

NetworkInterceptionSink::NetworkInterceptionSink(WebPage& page)
    : m_page(page)
{
}

NetworkInterceptionSink::~NetworkInterceptionSink()
{
    // The page is going away, so do not resume paused requests into it. A paused response's
    // CompletionHandler must still run; WebResourceLoader handles a loader that is gone.
    auto pendingResponses = std::exchange(m_inspectorPendingResponses, { });
    for (auto& pendingResponse : pendingResponses.values())
        pendingResponse->continuation(pendingResponse->originalResponse, nullptr);
}

const Inspector::NetworkInterception& NetworkInterceptionSink::configuration() const
{
    static NeverDestroyed<Inspector::NetworkInterception> emptyConfiguration;
    if (RefPtr corePage = Ref { m_page }->corePage())
        return corePage->networkInterception();
    return emptyConfiguration.get();
}

bool NetworkInterceptionSink::matches(const URL& url, Inspector::NetworkInterceptionPhase phase, Inspector::NetworkInterceptOwner owner) const
{
    URL urlWithoutFragment = url;
    urlWithoutFragment.removeFragmentIdentifier();
    if (urlWithoutFragment.string().isEmpty())
        return false;
    return configuration().matches(urlWithoutFragment, phase, owner);
}

static HTTPHeaderMap headerMap(const Vector<std::pair<String, String>>& headers)
{
    HTTPHeaderMap headerMap;
    for (auto& [name, value] : headers)
        headerMap.add(name, value);
    return headerMap;
}

// ResourceResponse::setHTTPHeaderFields is not exported from WebCore, so replace the headers
// through the response's data.
static void setResponseHTTPHeaderFields(ResourceResponse& response, HTTPHeaderMap&& headers)
{
    auto data = response.crossThreadData();
    data.httpHeaderFields = WTF::move(headers);
    response = ResourceResponse::fromCrossThreadData(WTF::move(data));
}

bool NetworkInterceptionSink::pauseRequestForInspector(ResourceLoader& loader, InspectorRequestContinuation&& continuation)
{
    auto identifier = loader.identifier();
    if (!identifier || m_inspectorPendingRequests.contains(*identifier))
        return false;

    m_inspectorPendingRequests.set(*identifier, makeUnique<InspectorPendingRequest>(Ref { loader }, WTF::move(continuation)));
    return true;
}

bool NetworkInterceptionSink::pauseResponseForInspector(ResourceLoaderIdentifier identifier, const ResourceResponse& response, InspectorResponseContinuation&& continuation)
{
    if (m_inspectorPendingResponses.contains(identifier))
        return false;

    m_inspectorPendingResponses.set(identifier, makeUnique<InspectorPendingResponse>(response, WTF::move(continuation)));
    return true;
}

void NetworkInterceptionSink::continueInspectorPauses()
{
    auto pendingRequests = std::exchange(m_inspectorPendingRequests, { });
    for (auto& pendingRequest : pendingRequests.values()) {
        Ref loader = pendingRequest->loader;
        if (!loader->reachedTerminalState())
            pendingRequest->continuation(loader->request());
    }

    auto pendingResponses = std::exchange(m_inspectorPendingResponses, { });
    for (auto& pendingResponse : pendingResponses.values())
        pendingResponse->continuation(pendingResponse->originalResponse, nullptr);
}

void NetworkInterceptionSink::resolveInterception(ResourceLoaderIdentifier identifier, Inspector::NetworkInterceptOwner owner, Inspector::NetworkInterceptionPhase phase, Inspector::NetworkInterceptionResolution&& resolution)
{
    // Only the owner of the pattern that paused a load may resolve it. Every load paused here so
    // far was paused for Web Inspector, so another owner's resolution does not apply to it.
    if (owner != Inspector::NetworkInterceptOwner::Inspector)
        return;

    // A load that is already gone was released; its resolution is ignored.
    switch (phase) {
    case Inspector::NetworkInterceptionPhase::BeforeRequestSent:
        if (m_inspectorPendingRequests.contains(identifier))
            resolveInspectorRequest(identifier, WTF::move(resolution));
        return;
    case Inspector::NetworkInterceptionPhase::ResponseStarted:
        if (m_inspectorPendingResponses.contains(identifier))
            resolveInspectorResponse(identifier, WTF::move(resolution));
        return;
    case Inspector::NetworkInterceptionPhase::AuthRequired:
        return;
    }
}

static ResourceError::Type resourceErrorType(Inspector::NetworkInterceptionErrorType errorType)
{
    switch (errorType) {
    case Inspector::NetworkInterceptionErrorType::General:
        return ResourceError::Type::General;
    case Inspector::NetworkInterceptionErrorType::AccessControl:
        return ResourceError::Type::AccessControl;
    case Inspector::NetworkInterceptionErrorType::Cancellation:
        return ResourceError::Type::Cancellation;
    case Inspector::NetworkInterceptionErrorType::Timeout:
        return ResourceError::Type::Timeout;
    }
    ASSERT_NOT_REACHED();
    return ResourceError::Type::General;
}

// Mirrors the resolution half of the single-process InspectorNetworkAgent (interceptContinue,
// interceptWithRequest, interceptRequestWithResponse, interceptRequestWithError), with the
// protocol arguments decoded in the UI process.
void NetworkInterceptionSink::resolveInspectorRequest(ResourceLoaderIdentifier identifier, Inspector::NetworkInterceptionResolution&& resolution)
{
    auto pendingRequest = m_inspectorPendingRequests.take(identifier);
    Ref loader = pendingRequest->loader;
    if (loader->reachedTerminalState())
        return;

    switch (resolution.action) {
    case Inspector::NetworkInterceptionAction::Continue: {
        ResourceRequest request = loader->request();
        if (resolution.url)
            request.setURL(WTF::move(*resolution.url));
        if (!resolution.method.isNull())
            request.setHTTPMethod(resolution.method);
        if (resolution.headers)
            request.setHTTPHeaderFields(headerMap(*resolution.headers));
        if (resolution.body)
            request.setHTTPBody(FormData::create(resolution.body->span()));
        pendingRequest->continuation(request);
        return;
    }

    case Inspector::NetworkInterceptionAction::Fulfill: {
        // Mimic data URL load behavior: report didReceiveResponse and didFinishLoading without
        // going to the network process.
        Ref data = SharedBuffer::create(resolution.body ? resolution.body->span() : std::span<const uint8_t> { });
        ResourceResponse response(URL { loader->url() }, String { resolution.mimeType }, data->size(), String());
        response.setSource(ResourceResponse::Source::InspectorOverride);
        if (resolution.statusCode)
            response.setHTTPStatusCode(*resolution.statusCode);
        response.setHTTPStatusText(String { resolution.reasonPhrase });
        if (resolution.headers)
            setResponseHTTPHeaderFields(response, headerMap(*resolution.headers));
        response.setHTTPHeaderField(HTTPHeaderName::ContentType, response.mimeType());
        loader->didReceiveResponse(WTF::move(response), [loader, data = WTF::move(data)] {
            if (loader->reachedTerminalState())
                return;

            if (data->size())
                loader->didReceiveData(data, data->size(), DataPayloadWholeResource);
            loader->didFinishLoading(NetworkLoadMetrics());
        });
        return;
    }

    case Inspector::NetworkInterceptionAction::Fail:
        loader->didFail(ResourceError("InspectorNetworkAgent"_s, 0, loader->url(), "Blocked by Web Inspector"_s, resourceErrorType(resolution.errorType)));
        return;
    }
}

// Mirrors interceptContinue and interceptWithResponse of the single-process InspectorNetworkAgent
// for a paused response: Continue lets the original response go on, and Fulfill replaces it with
// the response and body of the resolution. Like the single-process agent, a paused response cannot
// be failed; ProxyingNetworkAgent rejects such a resolution before sending it.
void NetworkInterceptionSink::resolveInspectorResponse(ResourceLoaderIdentifier identifier, Inspector::NetworkInterceptionResolution&& resolution)
{
    auto pendingResponse = m_inspectorPendingResponses.take(identifier);

    switch (resolution.action) {
    case Inspector::NetworkInterceptionAction::Continue:
        pendingResponse->continuation(pendingResponse->originalResponse, nullptr);
        return;
    case Inspector::NetworkInterceptionAction::Fail:
        ASSERT_NOT_REACHED();
        pendingResponse->continuation(pendingResponse->originalResponse, nullptr);
        return;
    case Inspector::NetworkInterceptionAction::Fulfill:
        break;
    }

    ResourceResponse overrideResponse = pendingResponse->originalResponse;
    overrideResponse.setSource(ResourceResponse::Source::InspectorOverride);
    if (resolution.statusCode)
        overrideResponse.setHTTPStatusCode(*resolution.statusCode);
    if (!resolution.reasonPhrase.isNull())
        overrideResponse.setHTTPStatusText(String { resolution.reasonPhrase });
    if (!resolution.mimeType.isNull())
        overrideResponse.setMimeType(String { resolution.mimeType });
    if (resolution.headers) {
        setResponseHTTPHeaderFields(overrideResponse, headerMap(*resolution.headers));
        overrideResponse.setHTTPHeaderField(HTTPHeaderName::ContentType, overrideResponse.mimeType());
    }

    RefPtr<FragmentedSharedBuffer> overrideData = SharedBuffer::create(resolution.body ? resolution.body->span() : std::span<const uint8_t> { });
    pendingResponse->continuation(overrideResponse, WTF::move(overrideData));
}

} // namespace WebKit
