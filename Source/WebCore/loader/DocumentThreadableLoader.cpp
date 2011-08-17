/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "DocumentThreadableLoader.h"

#include "AuthenticationChallenge.h"
#include "CrossOriginAccessControl.h"
#include "CrossOriginPreflightResultCache.h"
#include "Document.h"
#include "DocumentThreadableLoaderClient.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "ResourceHandle.h"
#include "ResourceLoadScheduler.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "SubresourceLoader.h"
#include "ThreadableLoaderClient.h"
#include <wtf/UnusedParam.h>

#if ENABLE(INSPECTOR)
#include "InspectorInstrumentation.h"
#include "ProgressTracker.h"
#endif

namespace WebCore {

void DocumentThreadableLoader::loadResourceSynchronously(Document* document, const ResourceRequest& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options)
{
    // The loader will be deleted as soon as this function exits.
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, &client, LoadSynchronously, request, options));
    ASSERT(loader->hasOneRef());
}

PassRefPtr<DocumentThreadableLoader> DocumentThreadableLoader::create(Document* document, ThreadableLoaderClient* client, const ResourceRequest& request, const ThreadableLoaderOptions& options)
{
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, client, LoadAsynchronously, request, options));
    if (!loader->m_loader)
        loader = 0;
    return loader.release();
}

DocumentThreadableLoader::DocumentThreadableLoader(Document* document, ThreadableLoaderClient* client, BlockingBehavior blockingBehavior, const ResourceRequest& request, const ThreadableLoaderOptions& options)
    : m_client(client)
    , m_document(document)
    , m_options(options)
    , m_sameOriginRequest(securityOrigin()->canRequest(request.url()))
    , m_async(blockingBehavior == LoadAsynchronously)
#if ENABLE(INSPECTOR)
    , m_preflightRequestIdentifier(0)
#endif
{
    ASSERT(document);
    ASSERT(client);
    // Setting an outgoing referer is only supported in the async code path.
    ASSERT(m_async || request.httpReferrer().isEmpty());

    if (m_sameOriginRequest || m_options.crossOriginRequestPolicy == AllowCrossOriginRequests) {
        loadRequest(request, DoSecurityCheck);
        return;
    }

    if (m_options.crossOriginRequestPolicy == DenyCrossOriginRequests) {
        m_client->didFail(ResourceError(errorDomainWebKitInternal, 0, request.url().string(), "Cross origin requests are not supported."));
        return;
    }
    
    ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);

    OwnPtr<ResourceRequest> crossOriginRequest = adoptPtr(new ResourceRequest(request));
    updateRequestForAccessControl(*crossOriginRequest, securityOrigin(), m_options.allowCredentials);

    if ((m_options.preflightPolicy == ConsiderPreflight && isSimpleCrossOriginAccessRequest(crossOriginRequest->httpMethod(), crossOriginRequest->httpHeaderFields())) || m_options.preflightPolicy == PreventPreflight)
        makeSimpleCrossOriginAccessRequest(*crossOriginRequest);
    else {
        m_actualRequest = crossOriginRequest.release();

        if (CrossOriginPreflightResultCache::shared().canSkipPreflight(securityOrigin()->toString(), m_actualRequest->url(), m_options.allowCredentials, m_actualRequest->httpMethod(), m_actualRequest->httpHeaderFields()))
            preflightSuccess();
        else
            makeCrossOriginAccessRequestWithPreflight(*m_actualRequest);
    }
}

void DocumentThreadableLoader::makeSimpleCrossOriginAccessRequest(const ResourceRequest& request)
{
    ASSERT(m_options.preflightPolicy != ForcePreflight);
    ASSERT(m_options.preflightPolicy == PreventPreflight || isSimpleCrossOriginAccessRequest(request.httpMethod(), request.httpHeaderFields()));

    // Cross-origin requests are only defined for HTTP. We would catch this when checking response headers later, but there is no reason to send a request that's guaranteed to be denied.
    // FIXME: Consider allowing simple CORS requests to non-HTTP URLs.
    if (!request.url().protocolInHTTPFamily()) {
        m_client->didFail(ResourceError(errorDomainWebKitInternal, 0, request.url().string(), "Cross origin requests are only supported for HTTP."));
        return;
    }

    loadRequest(request, DoSecurityCheck);
}

void DocumentThreadableLoader::makeCrossOriginAccessRequestWithPreflight(const ResourceRequest& request)
{
    ResourceRequest preflightRequest = createAccessControlPreflightRequest(request, securityOrigin(), m_options.allowCredentials);
    loadRequest(preflightRequest, DoSecurityCheck);
}

DocumentThreadableLoader::~DocumentThreadableLoader()
{
    if (m_loader)
        m_loader->clearClient();
}

void DocumentThreadableLoader::cancel()
{
    if (!m_loader)
        return;

    m_loader->cancel();
    m_loader->clearClient();
    m_loader = 0;
    m_client = 0;
}

void DocumentThreadableLoader::setDefersLoading(bool value)
{
    if (m_loader)
        m_loader->setDefersLoading(value);
}

void DocumentThreadableLoader::willSendRequest(SubresourceLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT(m_client);
    ASSERT_UNUSED(loader, loader == m_loader);

    if (!isAllowedRedirect(request.url())) {
        RefPtr<DocumentThreadableLoader> protect(this);
        m_client->didFailRedirectCheck();
        request = ResourceRequest();
    } else {
        if (m_client->isDocumentThreadableLoaderClient())
            static_cast<DocumentThreadableLoaderClient*>(m_client)->willSendRequest(request, redirectResponse);
    }
}

void DocumentThreadableLoader::didSendData(SubresourceLoader* loader, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    ASSERT_UNUSED(loader, loader == m_loader);

    m_client->didSendData(bytesSent, totalBytesToBeSent);
}

void DocumentThreadableLoader::didReceiveResponse(SubresourceLoader* loader, const ResourceResponse& response)
{
    ASSERT(loader == m_loader);
    didReceiveResponse(loader->identifier(), response);
}

void DocumentThreadableLoader::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    ASSERT(m_client);

#if ENABLE(INSPECTOR)
    if (m_preflightRequestIdentifier) {
        DocumentLoader* loader = m_document->frame()->loader()->documentLoader();
        InspectorInstrumentationCookie cookie = InspectorInstrumentation::willReceiveResourceResponse(m_document->frame(), m_preflightRequestIdentifier, response);
        InspectorInstrumentation::didReceiveResourceResponse(cookie, m_preflightRequestIdentifier, loader, response);
    }
#endif

    String accessControlErrorDescription;
    if (m_actualRequest) {
        if (!passesAccessControlCheck(response, m_options.allowCredentials, securityOrigin(), accessControlErrorDescription)) {
            preflightFailure(response.url(), accessControlErrorDescription);
            return;
        }

        OwnPtr<CrossOriginPreflightResultCacheItem> preflightResult = adoptPtr(new CrossOriginPreflightResultCacheItem(m_options.allowCredentials));
        if (!preflightResult->parse(response, accessControlErrorDescription)
            || !preflightResult->allowsCrossOriginMethod(m_actualRequest->httpMethod(), accessControlErrorDescription)
            || !preflightResult->allowsCrossOriginHeaders(m_actualRequest->httpHeaderFields(), accessControlErrorDescription)) {
            preflightFailure(response.url(), accessControlErrorDescription);
            return;
        }

        CrossOriginPreflightResultCache::shared().appendEntry(securityOrigin()->toString(), m_actualRequest->url(), preflightResult.release());
    } else {
        if (!m_sameOriginRequest && m_options.crossOriginRequestPolicy == UseAccessControl) {
            if (!passesAccessControlCheck(response, m_options.allowCredentials, securityOrigin(), accessControlErrorDescription)) {
                m_client->didFail(ResourceError(errorDomainWebKitInternal, 0, response.url().string(), accessControlErrorDescription));
                return;
            }
        }

        m_client->didReceiveResponse(identifier, response);
    }
}

void DocumentThreadableLoader::didReceiveData(SubresourceLoader* loader, const char* data, int dataLength)
{
    ASSERT(m_client);
    ASSERT_UNUSED(loader, loader == m_loader);

#if ENABLE(INSPECTOR)
    if (m_preflightRequestIdentifier)
        InspectorInstrumentation::didReceiveData(m_document->frame(), m_preflightRequestIdentifier, 0, 0, dataLength);
#endif

    // Preflight data should be invisible to clients.
    if (m_actualRequest)
        return;

    m_client->didReceiveData(data, dataLength);
}

void DocumentThreadableLoader::didReceiveCachedMetadata(SubresourceLoader* loader, const char* data, int dataLength)
{
    ASSERT(m_client);
    ASSERT_UNUSED(loader, loader == m_loader);

    // Preflight data should be invisible to clients.
    if (m_actualRequest)
        return;

    m_client->didReceiveCachedMetadata(data, dataLength);
}

void DocumentThreadableLoader::didFinishLoading(SubresourceLoader* loader, double finishTime)
{
    ASSERT(loader == m_loader);
    ASSERT(m_client);
    didFinishLoading(loader->identifier(), finishTime);
}

void DocumentThreadableLoader::didFinishLoading(unsigned long identifier, double finishTime)
{
#if ENABLE(INSPECTOR)
    if (m_preflightRequestIdentifier)
        InspectorInstrumentation::didFinishLoading(m_document->frame(), m_document->frame()->loader()->documentLoader(), m_preflightRequestIdentifier, finishTime);
#endif

    if (m_actualRequest) {
        ASSERT(!m_sameOriginRequest);
        ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);
        preflightSuccess();
    } else
        m_client->didFinishLoading(identifier, finishTime);
}

void DocumentThreadableLoader::didFail(SubresourceLoader* loader, const ResourceError& error)
{
    ASSERT(m_client);
    // m_loader may be null if we arrive here via SubresourceLoader::create in the ctor
    ASSERT_UNUSED(loader, loader == m_loader || !m_loader);

#if ENABLE(INSPECTOR)
    if (m_preflightRequestIdentifier)
        InspectorInstrumentation::didFailLoading(m_document->frame(), m_document->frame()->loader()->documentLoader(), m_preflightRequestIdentifier, error);
#endif

    m_client->didFail(error);
}

bool DocumentThreadableLoader::getShouldUseCredentialStorage(SubresourceLoader* loader, bool& shouldUseCredentialStorage)
{
    ASSERT_UNUSED(loader, loader == m_loader || !m_loader);

    if (!m_options.allowCredentials) {
        shouldUseCredentialStorage = false;
        return true;
    }

    return false; // Only FrameLoaderClient can ultimately permit credential use.
}

void DocumentThreadableLoader::didReceiveAuthenticationChallenge(SubresourceLoader* loader, const AuthenticationChallenge& challenge)
{
    ASSERT(loader == m_loader);
    // Users are not prompted for credentials for cross-origin requests.
    if (!m_sameOriginRequest) {
#if PLATFORM(MAC) || USE(CFNETWORK) || USE(CURL)
        loader->handle()->receivedRequestToContinueWithoutCredential(challenge);
#else
        // These platforms don't provide a way to continue without credentials, cancel the load altogether.
        UNUSED_PARAM(challenge);
        RefPtr<DocumentThreadableLoader> protect(this);
        m_client->didFail(loader->blockedError());
        cancel();
#endif
    }
}

void DocumentThreadableLoader::preflightSuccess()
{
    OwnPtr<ResourceRequest> actualRequest;
    actualRequest.swap(m_actualRequest);

    actualRequest->setHTTPOrigin(securityOrigin()->toString());

    // It should be ok to skip the security check since we already asked about the preflight request.
    loadRequest(*actualRequest, SkipSecurityCheck);
}

void DocumentThreadableLoader::preflightFailure(const String& url, const String& errorDescription)
{
    m_actualRequest = nullptr; // Prevent didFinishLoading() from bypassing access check.
    m_client->didFail(ResourceError(errorDomainWebKitInternal, 0, url, errorDescription));
}

void DocumentThreadableLoader::loadRequest(const ResourceRequest& request, SecurityCheckPolicy securityCheck)
{
    // Any credential should have been removed from the cross-site requests.
    const KURL& requestURL = request.url();
    ASSERT(m_sameOriginRequest || requestURL.user().isEmpty());
    ASSERT(m_sameOriginRequest || requestURL.pass().isEmpty());

    if (m_async) {
        // Don't sniff content or send load callbacks for the preflight request.
        bool sendLoadCallbacks = m_options.sendLoadCallbacks && !m_actualRequest;
        bool sniffContent = m_options.sniffContent && !m_actualRequest;
        // Keep buffering the data for the preflight request.
        bool shouldBufferData = m_options.shouldBufferData || m_actualRequest;

#if ENABLE(INSPECTOR)
        if (m_actualRequest) {
            ResourceRequest newRequest(request);
            // Because willSendRequest only gets called during redirects, we initialize the identifier and the first willSendRequest here.
            m_preflightRequestIdentifier = m_document->frame()->page()->progress()->createUniqueIdentifier();
            ResourceResponse redirectResponse = ResourceResponse();
            InspectorInstrumentation::willSendRequest(m_document->frame(), m_preflightRequestIdentifier, m_document->frame()->loader()->documentLoader(), newRequest, redirectResponse);
        }
#endif

        // Clear the loader so that any callbacks from SubresourceLoader::create will not have the old loader.
        m_loader = 0;
        m_loader = resourceLoadScheduler()->scheduleSubresourceLoad(m_document->frame(), this, request, ResourceLoadPriorityMedium, securityCheck, sendLoadCallbacks,
                                                                    sniffContent, shouldBufferData);
        return;
    }
    
    // FIXME: ThreadableLoaderOptions.sniffContent is not supported for synchronous requests.
    StoredCredentials storedCredentials = m_options.allowCredentials ? AllowStoredCredentials : DoNotAllowStoredCredentials;

    Vector<char> data;
    ResourceError error;
    ResourceResponse response;
    unsigned long identifier = std::numeric_limits<unsigned long>::max();
    if (m_document->frame())
        identifier = m_document->frame()->loader()->loadResourceSynchronously(request, storedCredentials, error, response, data);

    // No exception for file:/// resources, see <rdar://problem/4962298>.
    // Also, if we have an HTTP response, then it wasn't a network error in fact.
    if (!error.isNull() && !requestURL.isLocalFile() && response.httpStatusCode() <= 0) {
        m_client->didFail(error);
        return;
    }

    // FIXME: FrameLoader::loadSynchronously() does not tell us whether a redirect happened or not, so we guess by comparing the
    // request and response URLs. This isn't a perfect test though, since a server can serve a redirect to the same URL that was
    // requested. Also comparing the request and response URLs as strings will fail if the requestURL still has its credentials.
    if (requestURL != response.url() && !isAllowedRedirect(response.url())) {
        m_client->didFailRedirectCheck();
        return;
    }

    didReceiveResponse(identifier, response);

    const char* bytes = static_cast<const char*>(data.data());
    int len = static_cast<int>(data.size());
    didReceiveData(0, bytes, len);

    didFinishLoading(identifier, 0.0);
}

bool DocumentThreadableLoader::isAllowedRedirect(const KURL& url)
{
    if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
        return true;

    // FIXME: We need to implement access control for each redirect. This will require some refactoring though, because the code
    // that processes redirects doesn't know about access control and expects a synchronous answer from its client about whether
    // a redirect should proceed.
    return m_sameOriginRequest && securityOrigin()->canRequest(url);
}

SecurityOrigin* DocumentThreadableLoader::securityOrigin() const
{
    return m_options.securityOrigin ? m_options.securityOrigin.get() : m_document->securityOrigin();
}

} // namespace WebCore
