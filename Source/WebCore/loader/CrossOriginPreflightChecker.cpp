/*
 * Copyright (C) 2016 Canon Inc. All rights reserved.
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
 *     * Neither the name of Canon Inc. nor the names of its
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
#include "CrossOriginPreflightChecker.h"

#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "ContentSecurityPolicy.h"
#include "CrossOriginAccessControl.h"
#include "CrossOriginPreflightResultCache.h"
#include "DocumentThreadableLoader.h"
#include "InspectorInstrumentation.h"
#include "RuntimeEnabledFeatures.h"
#include "ThreadableLoaderClient.h"

namespace WebCore {

CrossOriginPreflightChecker::CrossOriginPreflightChecker(DocumentThreadableLoader& loader, ResourceRequest&& request)
    : m_loader(loader)
    , m_request(WTFMove(request))
{
}

CrossOriginPreflightChecker::~CrossOriginPreflightChecker()
{
    if (m_resource)
        m_resource->removeClient(this);
}

void CrossOriginPreflightChecker::validatePreflightResponse(DocumentThreadableLoader& loader, ResourceRequest&& request, unsigned long identifier, const ResourceResponse& response)
{
    Frame* frame = loader.document().frame();
    ASSERT(frame);
    auto cookie = InspectorInstrumentation::willReceiveResourceResponse(frame);
    InspectorInstrumentation::didReceiveResourceResponse(cookie, identifier, frame->loader().documentLoader(), response, 0);

    if (!response.isSuccessful()) {
        loader.preflightFailure(identifier, ResourceError(errorDomainWebKitInternal, 0, request.url(), ASCIILiteral("Preflight response is not successful"), ResourceError::Type::AccessControl));
        return;
    }

    String description;
    if (!passesAccessControlCheck(response, loader.options().allowCredentials, loader.securityOrigin(), description)) {
        loader.preflightFailure(identifier, ResourceError(errorDomainWebKitInternal, 0, request.url(), description, ResourceError::Type::AccessControl));
        return;
    }

    auto result = std::make_unique<CrossOriginPreflightResultCacheItem>(loader.options().allowCredentials);
    if (!result->parse(response, description)
        || !result->allowsCrossOriginMethod(request.httpMethod(), description)
        || !result->allowsCrossOriginHeaders(request.httpHeaderFields(), description)) {
        loader.preflightFailure(identifier, ResourceError(errorDomainWebKitInternal, 0, request.url(), description, ResourceError::Type::AccessControl));
        return;
    }

    CrossOriginPreflightResultCache::singleton().appendEntry(loader.securityOrigin().toString(), request.url(), WTFMove(result));
    loader.preflightSuccess(WTFMove(request));
}

void CrossOriginPreflightChecker::notifyFinished(CachedResource* resource)
{
    ASSERT_UNUSED(resource, resource == m_resource);
    if (m_resource->loadFailedOrCanceled()) {
        ResourceError preflightError = m_resource->resourceError();
        preflightError.setType(ResourceError::Type::AccessControl);
        m_loader.preflightFailure(m_resource->identifier(), preflightError);
        return;
    }
    validatePreflightResponse(m_loader, WTFMove(m_request), m_resource->identifier(), m_resource->response());
}

void CrossOriginPreflightChecker::startPreflight()
{
    ResourceLoaderOptions options = static_cast<FetchOptions>(m_loader.options());
    options.credentials = FetchOptions::Credentials::Omit;
    options.redirect = FetchOptions::Redirect::Manual;

    CachedResourceRequest preflightRequest(createAccessControlPreflightRequest(m_request, m_loader.securityOrigin()), options);
    if (RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled())
        preflightRequest.setInitiator(m_loader.options().initiator);

    if (!m_loader.referrer().isNull())
        preflightRequest.mutableResourceRequest().setHTTPReferrer(m_loader.referrer());

    ASSERT(!m_resource);
    m_resource = m_loader.document().cachedResourceLoader().requestRawResource(preflightRequest);
    if (m_resource)
        m_resource->addClient(this);
}

void CrossOriginPreflightChecker::doPreflight(DocumentThreadableLoader& loader, ResourceRequest&& request)
{
    if (!loader.document().frame())
        return;

    ResourceRequest preflightRequest = createAccessControlPreflightRequest(request, loader.securityOrigin());
    ResourceError error;
    ResourceResponse response;
    RefPtr<SharedBuffer> data;

    if (!loader.referrer().isNull())
        preflightRequest.setHTTPReferrer(loader.referrer());

    unsigned identifier = loader.document().frame()->loader().loadResourceSynchronously(preflightRequest, DoNotAllowStoredCredentials, ClientCredentialPolicy::CannotAskClientForCredentials, error, response, data);

    // FIXME: Investigate why checking for response httpStatusCode here. In particular, can we have a not-null error and a 2XX response.
    if (!error.isNull() && response.httpStatusCode() <= 0) {
        error.setType(ResourceError::Type::AccessControl);
        loader.preflightFailure(identifier, error);
        return;
    }

    // FIXME: Ideally, we should ask platformLoadResourceSynchronously to set ResourceResponse isRedirected and use it here.
    bool isRedirect = preflightRequest.url().strippedForUseAsReferrer() != response.url().strippedForUseAsReferrer();
    if (isRedirect || !response.isSuccessful()) {
        loader.preflightFailure(identifier, ResourceError(errorDomainWebKitInternal, 0, request.url(), ASCIILiteral("Preflight response is not successful"), ResourceError::Type::AccessControl));
        return;
    }

    validatePreflightResponse(loader, WTFMove(request), identifier, response);
}

void CrossOriginPreflightChecker::setDefersLoading(bool value)
{
    if (m_resource)
        m_resource->setDefersLoading(value);
}

bool CrossOriginPreflightChecker::isXMLHttpRequest() const
{
    return m_loader.isXMLHttpRequest();
}

} // namespace WebCore
